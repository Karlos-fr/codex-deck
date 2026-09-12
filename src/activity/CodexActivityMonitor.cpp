// ============================================================================
// Codex Glass - Moniteur d'activite locale Codex
// ----------------------------------------------------------------------------
// Ce fichier surveille les JSONL en lecture seule, conserve des curseurs en
// memoire et n'extrait que les marqueurs techniques necessaires a l'interface.
// ============================================================================

#include "CodexActivityMonitor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

// Intervalle du polling de secours lorsque les notifications sont perdues.
constexpr DWORD kFallbackPollingIntervalMs = 2000;

// Delai de regroupement des ecritures JSONL rapprochees.
constexpr DWORD kFileNotificationDebounceMs = 100;

// Taille maximale de prefixe conservee pour une ligne JSONL.
constexpr std::size_t kMaximumMetadataPrefixBytes = 64U * 1024U;

// Taille de lecture incrementale utilisee avec ReadFile.
constexpr DWORD kReadBufferBytes = 64U * 1024U;

// Portion recente inspectee au premier demarrage pour une session active.
constexpr std::uint64_t kInitialTailBytes = 4ULL * 1024ULL * 1024ULL;

// Duree sans signe de vie apres laquelle un tour implicite est oublie.
constexpr std::chrono::minutes kStaleSessionDelay{10};

// Duree de conservation technique d'un etat terminal pour le thread UI.
constexpr std::chrono::seconds kTerminalRetentionDelay{3};

// Droits de partage permettant de lire un journal encore utilise par Codex.
constexpr DWORD kJsonlFileShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

// Changements de repertoire pertinents pour les fichiers de session.
constexpr DWORD kSessionDirectoryChangeMask = FILE_NOTIFY_CHANGE_FILE_NAME
    | FILE_NOTIFY_CHANGE_DIR_NAME
    | FILE_NOTIFY_CHANGE_SIZE
    | FILE_NOTIFY_CHANGE_LAST_WRITE
    | FILE_NOTIFY_CHANGE_CREATION;

// ----------------------------------------------------------------------------
// Convertit une chaine UTF-8 technique en UTF-16.
//
// Parametres :
// - input : valeur UTF-8 a convertir.
//
// Retour :
// - chaine UTF-16 ou chaine vide en cas d'echec.
// ----------------------------------------------------------------------------
std::wstring Utf8ToWide(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        nullptr,
        0
    );
    if (required <= 0) {
        return {};
    }
    std::wstring output(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            input.data(),
            static_cast<int>(input.size()),
            output.data(),
            required
        ) != required) {
        return {};
    }
    return output;
}

// ----------------------------------------------------------------------------
// Decode les echappements simples d'une valeur JSON technique.
//
// Parametres :
// - value : contenu situe entre les guillemets JSON.
//
// Retour :
// - valeur decodee, limitee aux identifiants techniques attendus.
// ----------------------------------------------------------------------------
std::string DecodeJsonTechnicalString(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    bool escaped = false;
    for (const char character : value) {
        if (!escaped) {
            if (character == '\\') {
                escaped = true;
            } else {
                output.push_back(character);
            }
            continue;
        }
        escaped = false;
        switch (character) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        default: return {};
        }
    }
    return escaped ? std::string{} : output;
}

// ----------------------------------------------------------------------------
// Lit une valeur chaine dans un prefixe JSON sans materialiser les autres champs.
//
// Parametres :
// - source : prefixe JSON a examiner.
// - key : nom exact du champ technique.
// - start : position a partir de laquelle effectuer la recherche.
//
// Retour :
// - valeur decodee ou chaine vide lorsque le champ est absent ou invalide.
// ----------------------------------------------------------------------------
std::string ReadJsonTechnicalString(
    std::string_view source,
    std::string_view key,
    std::size_t start = 0
) {
    std::string marker;
    marker.reserve(key.size() + 5U);
    marker.push_back('"');
    marker.append(key);
    marker.append("\":\"");
    const std::size_t marker_position = source.find(marker, start);
    if (marker_position == std::string_view::npos) {
        return {};
    }
    const std::size_t value_start = marker_position + marker.size();
    bool escaped = false;
    for (std::size_t index = value_start; index < source.size(); ++index) {
        const char character = source[index];
        if (!escaped && character == '"') {
            return DecodeJsonTechnicalString(source.substr(value_start, index - value_start));
        }
        if (!escaped && character == '\\') {
            escaped = true;
        } else {
            escaped = false;
        }
    }
    return {};
}

// ----------------------------------------------------------------------------
// Localise le debut de l'objet payload dans une ligne JSONL.
//
// Parametres :
// - line : prefixe de ligne a examiner.
//
// Retour :
// - position suivant l'accolade ouvrante, ou npos.
// ----------------------------------------------------------------------------
std::size_t PayloadStart(std::string_view line) {
    constexpr std::string_view marker = "\"payload\":{";
    const std::size_t position = line.find(marker);
    return position == std::string_view::npos ? position : position + marker.size();
}

// ----------------------------------------------------------------------------
// Parse un timestamp ISO UTC technique sans conserver le texte source.
//
// Parametres :
// - text : timestamp ISO retourne par Codex.
//
// Retour :
// - instant UTC ou valeur absente lorsque le format est inconnu.
// ----------------------------------------------------------------------------
std::optional<std::chrono::system_clock::time_point> ParseTechnicalTimestamp(
    const std::string& text
) {
    if (text.size() < 19U) {
        return std::nullopt;
    }
    std::tm utc{};
    try {
        utc.tm_year = std::stoi(text.substr(0, 4)) - 1900;
        utc.tm_mon = std::stoi(text.substr(5, 2)) - 1;
        utc.tm_mday = std::stoi(text.substr(8, 2));
        utc.tm_hour = std::stoi(text.substr(11, 2));
        utc.tm_min = std::stoi(text.substr(14, 2));
        utc.tm_sec = std::stoi(text.substr(17, 2));
    } catch (...) {
        return std::nullopt;
    }
    const __time64_t seconds = _mkgmtime64(&utc);
    if (seconds < 0) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(seconds);
}

// ----------------------------------------------------------------------------
// Met une cle Windows en minuscules pour des comparaisons stables.
//
// Parametres :
// - value : cle a normaliser.
//
// Retour :
// - copie normalisee.
// ----------------------------------------------------------------------------
std::wstring LowercaseKey(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value;
}

// ----------------------------------------------------------------------------
// Construit une cle de repli stable a partir du nom du JSONL.
//
// Parametres :
// - path : chemin de session observe.
//
// Retour :
// - nom de fichier normalise, commun avant et apres archivage.
// ----------------------------------------------------------------------------
std::wstring FallbackSessionKey(const std::filesystem::path& path) {
    return LowercaseKey(path.filename().wstring());
}

// ----------------------------------------------------------------------------
// Normalise un chemin pour indexer les curseurs sans doublon de casse.
//
// Parametres :
// - path : chemin a normaliser.
//
// Retour :
// - chemin absolu lexical en minuscules.
// ----------------------------------------------------------------------------
std::wstring NormalizePathKey(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) {
        absolute = path;
    }
    return LowercaseKey(absolute.lexically_normal().wstring());
}

// ----------------------------------------------------------------------------
// Resout la racine Codex active sans lire de secret.
//
// Retour :
// - CODEX_HOME ou le dossier .codex du profil Windows.
// ----------------------------------------------------------------------------
std::filesystem::path ResolveCodexHome() {
    const DWORD required = GetEnvironmentVariableW(L"CODEX_HOME", nullptr, 0);
    if (required > 1) {
        std::wstring value(static_cast<std::size_t>(required), L'\0');
        const DWORD written = GetEnvironmentVariableW(L"CODEX_HOME", value.data(), required);
        if (written > 0 && written < required) {
            value.resize(written);
            return std::filesystem::path(value);
        }
    }
    const DWORD profile_required = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (profile_required <= 1) {
        return {};
    }
    std::wstring profile(static_cast<std::size_t>(profile_required), L'\0');
    const DWORD profile_written = GetEnvironmentVariableW(
        L"USERPROFILE",
        profile.data(),
        profile_required
    );
    if (profile_written == 0 || profile_written >= profile_required) {
        return {};
    }
    profile.resize(profile_written);
    return std::filesystem::path(profile) / L".codex";
}

// ----------------------------------------------------------------------------
// Indique si deux snapshots portent le meme etat fonctionnel.
//
// Parametres :
// - left : premier snapshot.
// - right : second snapshot.
//
// Retour :
// - true lorsque le thread UI n'a rien de nouveau a traiter.
// ----------------------------------------------------------------------------
bool SameActivitySnapshot(
    const CodexActivitySnapshot& left,
    const CodexActivitySnapshot& right
) {
    if (left.monitor_available != right.monitor_available
        || left.sessions.size() != right.sessions.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.sessions.size(); ++index) {
        const auto& a = left.sessions[index];
        const auto& b = right.sessions[index];
        if (a.session_key != b.session_key
            || a.turn_id != b.turn_id
            || a.state != b.state
            || a.last_event_at != b.last_event_at
            || a.open_tool_count != b.open_tool_count
            || a.waiting_for_user != b.waiting_for_user
            || a.inferred != b.inferred) {
            return false;
        }
    }
    return true;
}

} // namespace

// ----------------------------------------------------------------------------
// Implementation privee du moniteur et de ses ressources de thread.
// ----------------------------------------------------------------------------
class CodexActivityMonitor::Impl {
public:
    // ------------------------------------------------------------------------
    // Cree l'implementation pour une racine explicite ou automatique.
    // ------------------------------------------------------------------------
    explicit Impl(std::filesystem::path codex_home)
        : codex_home_(codex_home.empty() ? ResolveCodexHome() : std::move(codex_home)) {}

    // ------------------------------------------------------------------------
    // Arrete automatiquement le thread et ferme l'evenement.
    // ------------------------------------------------------------------------
    ~Impl() {
        Stop();
    }

    // ------------------------------------------------------------------------
    // Demarre le thread de surveillance.
    // ------------------------------------------------------------------------
    bool Start(HWND hwnd) {
        if (thread_.joinable()) {
            return false;
        }
        stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stop_event_ == nullptr) {
            return false;
        }
        notify_hwnd_ = hwnd;
        try {
            thread_ = std::thread([this]() { Run(); });
            return true;
        } catch (...) {
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
            notify_hwnd_ = nullptr;
            return false;
        }
    }

    // ------------------------------------------------------------------------
    // Arrete le thread et libere les ressources partagees.
    // ------------------------------------------------------------------------
    void Stop() {
        if (stop_event_ != nullptr) {
            SetEvent(stop_event_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        if (stop_event_ != nullptr) {
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
        }
        notify_hwnd_ = nullptr;
        std::lock_guard lock(snapshot_mutex_);
        pending_snapshot_.reset();
    }

    // ------------------------------------------------------------------------
    // Indique si le thread est encore possede.
    // ------------------------------------------------------------------------
    bool IsActive() const {
        return thread_.joinable();
    }

    // ------------------------------------------------------------------------
    // Transfere le dernier snapshot disponible.
    // ------------------------------------------------------------------------
    std::optional<CodexActivitySnapshot> TakeSnapshot() {
        std::lock_guard lock(snapshot_mutex_);
        std::optional<CodexActivitySnapshot> result = std::move(pending_snapshot_);
        pending_snapshot_.reset();
        return result;
    }

private:
    // Etat de lecture incremental d'un fichier JSONL.
    struct FileCursor {
        std::uint64_t offset = 0;
        std::string partial_prefix;
        std::wstring session_key;
        bool discard_until_newline = false;
        bool archived = false;
    };

    // Etat technique interne d'une session Codex.
    struct SessionState {
        std::wstring session_key;
        std::wstring turn_id;
        CodexActivityState state = CodexActivityState::Thinking;
        std::chrono::system_clock::time_point last_event_at{};
        std::chrono::steady_clock::time_point last_seen_at{};
        std::chrono::steady_clock::time_point terminal_at{};
        std::map<std::wstring, bool> open_tools;
        bool active = false;
        bool inferred = false;
        bool explicit_lifecycle_seen = false;
    };

    // ------------------------------------------------------------------------
    // Execute la boucle notifications plus polling de secours.
    // ------------------------------------------------------------------------
    void Run() {
        bool initial_scan = true;
        while (WaitForSingleObject(stop_event_, 0) != WAIT_OBJECT_0) {
            ScanDirectories(initial_scan);
            initial_scan = false;

            std::array<HANDLE, 3> handles{stop_event_, nullptr, nullptr};
            DWORD handle_count = 1;
            const std::array<std::filesystem::path, 2> roots{
                codex_home_ / L"sessions",
                codex_home_ / L"archived_sessions",
            };
            for (const auto& root : roots) {
                std::error_code error;
                if (!std::filesystem::is_directory(root, error)) {
                    continue;
                }
                const HANDLE change = FindFirstChangeNotificationW(
                    root.c_str(),
                    TRUE,
                    kSessionDirectoryChangeMask
                );
                if (change != INVALID_HANDLE_VALUE) {
                    handles[handle_count++] = change;
                }
            }

            const DWORD wait_result = WaitForMultipleObjects(
                handle_count,
                handles.data(),
                FALSE,
                kFallbackPollingIntervalMs
            );
            for (DWORD index = 1; index < handle_count; ++index) {
                FindCloseChangeNotification(handles[index]);
            }
            if (wait_result == WAIT_OBJECT_0) {
                break;
            }
            if (wait_result > WAIT_OBJECT_0 && wait_result < WAIT_OBJECT_0 + handle_count) {
                if (WaitForSingleObject(stop_event_, kFileNotificationDebounceMs) == WAIT_OBJECT_0) {
                    break;
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // Enumere les JSONL, avance leurs curseurs et publie un snapshot.
    //
    // Parametres :
    // - initial_scan : autorise une lecture limitee de la fin des fichiers recents.
    // ------------------------------------------------------------------------
    void ScanDirectories(bool initial_scan) {
        const auto steady_now = std::chrono::steady_clock::now();
        const auto system_now = std::chrono::system_clock::now();
        std::vector<std::pair<std::filesystem::path, bool>> files;
        bool monitor_available = false;
        const std::array<std::pair<std::filesystem::path, bool>, 2> roots{{
            {codex_home_ / L"sessions", false},
            {codex_home_ / L"archived_sessions", true},
        }};
        for (const auto& [root, archived] : roots) {
            std::error_code error;
            if (!std::filesystem::is_directory(root, error)) {
                continue;
            }
            monitor_available = true;
            std::filesystem::recursive_directory_iterator iterator(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                error
            );
            const std::filesystem::recursive_directory_iterator end;
            while (!error && iterator != end) {
                const auto path = iterator->path();
                const bool regular_file = iterator->is_regular_file(error);
                if (!error
                    && regular_file
                    && LowercaseKey(path.extension().wstring()) == L".jsonl") {
                    files.emplace_back(path, archived);
                }
                error.clear();
                iterator.increment(error);
                if (error) {
                    error.clear();
                }
            }
        }

        std::map<std::wstring, bool> seen_paths;
        for (const auto& [path, archived] : files) {
            const std::wstring path_key = NormalizePathKey(path);
            seen_paths[path_key] = true;
            std::error_code error;
            const std::uint64_t file_size = std::filesystem::file_size(path, error);
            if (error) {
                continue;
            }
            auto [iterator, inserted] = cursors_.try_emplace(path_key);
            FileCursor& cursor = iterator->second;
            if (inserted) {
                cursor.session_key = FallbackSessionKey(path);
                cursor.archived = archived;
                const auto modified_at = std::filesystem::last_write_time(path, error);
                const bool recently_modified = !error
                    && std::filesystem::file_time_type::clock::now() - modified_at
                        < kStaleSessionDelay;
                const bool inspect_tail = initial_scan && !archived && recently_modified;
                if (archived || (initial_scan && !inspect_tail)) {
                    cursor.offset = file_size;
                } else if (inspect_tail && file_size > kInitialTailBytes) {
                    cursor.offset = file_size - kInitialTailBytes;
                    cursor.discard_until_newline = true;
                }
            }
            ReadIncrementalFile(path, cursor, file_size, steady_now, system_now);
            if (archived) {
                auto session = sessions_.find(cursor.session_key);
                if (session != sessions_.end() && session->second.active) {
                    MarkTerminal(session->second, CodexActivityState::Aborted, steady_now, system_now);
                }
            }
        }

        for (auto iterator = cursors_.begin(); iterator != cursors_.end();) {
            if (seen_paths.contains(iterator->first)) {
                ++iterator;
                continue;
            }
            const std::wstring session_key = iterator->second.session_key;
            iterator = cursors_.erase(iterator);
            const bool still_has_file = std::any_of(
                cursors_.begin(),
                cursors_.end(),
                [&session_key](const auto& value) {
                    return value.second.session_key == session_key;
                }
            );
            auto session = sessions_.find(session_key);
            if (!still_has_file && session != sessions_.end() && session->second.active) {
                MarkTerminal(session->second, CodexActivityState::Aborted, steady_now, system_now);
            }
        }

        RemoveExpiredSessions(steady_now);
        PublishSnapshot(monitor_available, system_now);
    }

    // ------------------------------------------------------------------------
    // Lit uniquement les octets ajoutes depuis le dernier passage.
    // ------------------------------------------------------------------------
    void ReadIncrementalFile(
        const std::filesystem::path& path,
        FileCursor& cursor,
        std::uint64_t file_size,
        std::chrono::steady_clock::time_point steady_now,
        std::chrono::system_clock::time_point system_now
    ) {
        if (file_size < cursor.offset) {
            cursor.offset = 0;
            cursor.partial_prefix.clear();
            cursor.discard_until_newline = false;
        }
        if (file_size == cursor.offset) {
            return;
        }
        const HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            kJsonlFileShareMode,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(cursor.offset);
        if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) {
            CloseHandle(file);
            return;
        }

        std::array<char, kReadBufferBytes> buffer{};
        while (cursor.offset < file_size) {
            const std::uint64_t remaining = file_size - cursor.offset;
            const DWORD requested = static_cast<DWORD>(std::min<std::uint64_t>(remaining, buffer.size()));
            DWORD bytes_read = 0;
            if (!ReadFile(file, buffer.data(), requested, &bytes_read, nullptr) || bytes_read == 0) {
                break;
            }
            cursor.offset += bytes_read;
            for (DWORD index = 0; index < bytes_read; ++index) {
                const char character = buffer[index];
                if (cursor.discard_until_newline) {
                    if (character == '\n') {
                        cursor.discard_until_newline = false;
                    }
                    continue;
                }
                if (character == '\n') {
                    ProcessTechnicalLine(cursor, cursor.partial_prefix, steady_now, system_now);
                    cursor.partial_prefix.clear();
                    continue;
                }
                if (character == '\r') {
                    continue;
                }
                if (cursor.partial_prefix.size() < kMaximumMetadataPrefixBytes) {
                    cursor.partial_prefix.push_back(character);
                }
            }
        }
        CloseHandle(file);
    }

    // ------------------------------------------------------------------------
    // Applique les seuls marqueurs techniques autorises d'une ligne JSONL.
    // ------------------------------------------------------------------------
    void ProcessTechnicalLine(
        FileCursor& cursor,
        std::string_view line,
        std::chrono::steady_clock::time_point steady_now,
        std::chrono::system_clock::time_point system_now
    ) {
        if (line.empty()) {
            return;
        }
        const std::string record_type = ReadJsonTechnicalString(line, "type");
        const std::size_t payload_start = PayloadStart(line);
        const std::string_view payload = payload_start == std::string_view::npos
            ? std::string_view{}
            : line.substr(payload_start);
        const std::string payload_type = ReadJsonTechnicalString(payload, "type");

        if (record_type == "session_meta") {
            std::string session_id = ReadJsonTechnicalString(payload, "session_id");
            if (session_id.empty()) {
                session_id = ReadJsonTechnicalString(payload, "id");
            }
            const std::wstring wide_id = Utf8ToWide(session_id);
            if (!wide_id.empty() && wide_id != cursor.session_key) {
                RenameSession(cursor.session_key, wide_id);
                cursor.session_key = wide_id;
            }
            return;
        }
        if (payload_type.empty()) {
            return;
        }

        const std::wstring turn_id = Utf8ToWide(ReadJsonTechnicalString(payload, "turn_id"));
        const std::wstring call_id = Utf8ToWide(ReadJsonTechnicalString(payload, "call_id"));
        const std::string timestamp_text = ReadJsonTechnicalString(line, "timestamp");
        const auto event_time = ParseTechnicalTimestamp(timestamp_text).value_or(system_now);
        SessionState& session = EnsureSession(cursor.session_key);

        if (record_type == "event_msg" && payload_type == "task_started") {
            session.turn_id = turn_id;
            session.state = CodexActivityState::Thinking;
            session.open_tools.clear();
            session.active = true;
            session.inferred = false;
            session.explicit_lifecycle_seen = true;
            TouchSession(session, steady_now, event_time);
            return;
        }
        if (record_type == "event_msg" && payload_type == "task_complete") {
            if (MatchesCurrentTurn(session, turn_id)) {
                MarkTerminal(session, CodexActivityState::Completed, steady_now, event_time);
            }
            return;
        }
        if (record_type == "event_msg" && payload_type == "turn_aborted") {
            if (MatchesCurrentTurn(session, turn_id)) {
                const std::string reason = ReadJsonTechnicalString(payload, "reason");
                const CodexActivityState terminal_kind = reason == "interrupted"
                    ? CodexActivityState::Aborted
                    : CodexActivityState::Error;
                MarkTerminal(session, terminal_kind, steady_now, event_time);
            }
            return;
        }

        const bool call_started = record_type == "response_item"
            && (payload_type == "custom_tool_call"
                || payload_type == "function_call"
                || payload_type == "tool_search_call");
        if (call_started) {
            OpenInferredTurn(session, turn_id);
            const std::string name = ReadJsonTechnicalString(payload, "name");
            const std::wstring effective_call_id = call_id.empty()
                ? Utf8ToWide(ReadJsonTechnicalString(payload, "id"))
                : call_id;
            if (!effective_call_id.empty()) {
                session.open_tools[effective_call_id] = name == "request_user_input";
            }
            RefreshToolKind(session);
            TouchSession(session, steady_now, event_time);
            return;
        }

        const bool call_finished = record_type == "response_item"
            && (payload_type == "custom_tool_call_output"
                || payload_type == "function_call_output"
                || payload_type == "tool_search_output");
        if (call_finished) {
            OpenInferredTurn(session, turn_id);
            if (!call_id.empty()) {
                session.open_tools.erase(call_id);
            }
            RefreshToolKind(session);
            TouchSession(session, steady_now, event_time);
            return;
        }

        if (record_type == "response_item" && payload_type == "reasoning") {
            OpenInferredTurn(session, turn_id);
            RefreshToolKind(session);
            TouchSession(session, steady_now, event_time);
            return;
        }
        if (record_type == "event_msg" && payload_type == "token_count") {
            if (!session.explicit_lifecycle_seen || session.active) {
                OpenInferredTurn(session, turn_id);
                TouchSession(session, steady_now, event_time);
            }
            return;
        }
        if (session.active) {
            TouchSession(session, steady_now, event_time);
        }
    }

    // ------------------------------------------------------------------------
    // Retourne ou cree l'etat associe a une cle de session.
    // ------------------------------------------------------------------------
    SessionState& EnsureSession(const std::wstring& session_key) {
        auto [iterator, inserted] = sessions_.try_emplace(session_key);
        if (inserted) {
            iterator->second.session_key = session_key;
        }
        return iterator->second;
    }

    // ------------------------------------------------------------------------
    // Remplace une cle de repli par l'identifiant session_meta.
    // ------------------------------------------------------------------------
    void RenameSession(const std::wstring& old_key, const std::wstring& new_key) {
        if (old_key == new_key) {
            return;
        }
        auto old = sessions_.find(old_key);
        if (old == sessions_.end()) {
            return;
        }
        SessionState moved = std::move(old->second);
        sessions_.erase(old);
        moved.session_key = new_key;
        sessions_.insert_or_assign(new_key, std::move(moved));
        for (auto& [path, cursor] : cursors_) {
            static_cast<void>(path);
            if (cursor.session_key == old_key) {
                cursor.session_key = new_key;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Ouvre un tour de compatibilite lorsqu'un marqueur de debut manque.
    // ------------------------------------------------------------------------
    void OpenInferredTurn(SessionState& session, const std::wstring& turn_id) {
        if (!session.active) {
            session.active = true;
            session.state = CodexActivityState::Thinking;
            session.inferred = true;
            session.terminal_at = {};
        }
        if (!turn_id.empty()) {
            session.turn_id = turn_id;
        }
    }

    // ------------------------------------------------------------------------
    // Indique si un marqueur terminal vise le tour courant.
    // ------------------------------------------------------------------------
    bool MatchesCurrentTurn(const SessionState& session, const std::wstring& turn_id) const {
        return turn_id.empty() || session.turn_id.empty() || session.turn_id == turn_id;
    }

    // ------------------------------------------------------------------------
    // Recalcule l'etat a partir des appels d'outil encore ouverts.
    // ------------------------------------------------------------------------
    void RefreshToolKind(SessionState& session) {
        if (!session.active) {
            return;
        }
        const bool has_waiting = std::any_of(
            session.open_tools.begin(),
            session.open_tools.end(),
            [](const auto& value) { return value.second; }
        );
        const bool has_running = std::any_of(
            session.open_tools.begin(),
            session.open_tools.end(),
            [](const auto& value) { return !value.second; }
        );
        session.state = has_running
            ? CodexActivityState::ToolRunning
            : (has_waiting
                ? CodexActivityState::WaitingForUser
                : CodexActivityState::Thinking);
    }

    // ------------------------------------------------------------------------
    // Met a jour les horodatages non sensibles d'une session.
    // ------------------------------------------------------------------------
    void TouchSession(
        SessionState& session,
        std::chrono::steady_clock::time_point steady_now,
        std::chrono::system_clock::time_point event_time
    ) {
        session.last_seen_at = steady_now;
        session.last_event_at = event_time;
    }

    // ------------------------------------------------------------------------
    // Ferme un tour avec un etat terminal temporairement publiable.
    // ------------------------------------------------------------------------
    void MarkTerminal(
        SessionState& session,
        CodexActivityState state,
        std::chrono::steady_clock::time_point steady_now,
        std::chrono::system_clock::time_point event_time
    ) {
        session.state = state;
        session.active = false;
        session.open_tools.clear();
        const auto terminal_age = std::chrono::system_clock::now() - event_time;
        session.terminal_at = terminal_age > std::chrono::system_clock::duration::zero()
            ? steady_now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(terminal_age)
            : steady_now;
        session.explicit_lifecycle_seen = true;
        TouchSession(session, steady_now, event_time);
    }

    // ------------------------------------------------------------------------
    // Supprime les sessions inactives ou terminales devenues inutiles.
    // ------------------------------------------------------------------------
    void RemoveExpiredSessions(std::chrono::steady_clock::time_point steady_now) {
        for (auto iterator = sessions_.begin(); iterator != sessions_.end();) {
            SessionState& session = iterator->second;
            const bool stale_active = session.active
                && session.last_seen_at.time_since_epoch().count() != 0
                && steady_now - session.last_seen_at >= kStaleSessionDelay;
            const bool expired_terminal = !session.active
                && session.terminal_at.time_since_epoch().count() != 0
                && steady_now - session.terminal_at >= kTerminalRetentionDelay;
            if (stale_active || expired_terminal) {
                iterator = sessions_.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Construit et publie le snapshot seulement lorsque son contenu change.
    // ------------------------------------------------------------------------
    void PublishSnapshot(
        bool monitor_available,
        std::chrono::system_clock::time_point system_now
    ) {
        CodexActivitySnapshot snapshot{};
        snapshot.monitor_available = monitor_available;
        snapshot.sampled_at = system_now;
        snapshot.sessions.reserve(sessions_.size());
        for (const auto& [key, state] : sessions_) {
            static_cast<void>(key);
            const bool waiting = std::any_of(
                state.open_tools.begin(),
                state.open_tools.end(),
                [](const auto& value) { return value.second; }
            );
            snapshot.sessions.push_back(CodexSessionActivitySnapshot{
                state.session_key,
                state.turn_id,
                state.state,
                state.last_event_at,
                state.open_tools.size(),
                waiting,
                state.inferred,
            });
        }
        std::sort(snapshot.sessions.begin(), snapshot.sessions.end(), [](const auto& left, const auto& right) {
            return left.session_key < right.session_key;
        });
        if (last_published_snapshot_.has_value()
            && SameActivitySnapshot(*last_published_snapshot_, snapshot)) {
            return;
        }
        last_published_snapshot_ = snapshot;
        {
            std::lock_guard lock(snapshot_mutex_);
            pending_snapshot_ = snapshot;
        }
        if (notify_hwnd_ != nullptr) {
            PostMessageW(notify_hwnd_, kCodexActivityChangedMessage, 0, 0);
        }
    }

    // Racine locale des sessions surveillees.
    std::filesystem::path codex_home_;

    // Fenetre destinataire des notifications de snapshot.
    HWND notify_hwnd_ = nullptr;

    // Evenement manuel utilise pour interrompre les attentes Windows.
    HANDLE stop_event_ = nullptr;

    // Thread unique qui possede les notifications et les curseurs.
    std::thread thread_;

    // Protege le snapshot transfere au thread UI.
    std::mutex snapshot_mutex_;

    // Dernier snapshot en attente de consommation.
    std::optional<CodexActivitySnapshot> pending_snapshot_;

    // Dernier snapshot compare pour eviter les messages identiques.
    std::optional<CodexActivitySnapshot> last_published_snapshot_;

    // Curseurs incrementaux indexes par chemin Windows normalise.
    std::map<std::wstring, FileCursor> cursors_;

    // Etats techniques indexes par identifiant stable de session.
    std::map<std::wstring, SessionState> sessions_;
};

// ----------------------------------------------------------------------------
// Cree un moniteur utilisant le profil Codex courant.
// ----------------------------------------------------------------------------
CodexActivityMonitor::CodexActivityMonitor()
    : impl_(std::make_unique<Impl>(std::filesystem::path{})) {}

// ----------------------------------------------------------------------------
// Cree un moniteur pour un profil explicite.
// ----------------------------------------------------------------------------
CodexActivityMonitor::CodexActivityMonitor(std::filesystem::path codex_home)
    : impl_(std::make_unique<Impl>(std::move(codex_home))) {}

// ----------------------------------------------------------------------------
// Arrete et joint automatiquement le thread de surveillance.
// ----------------------------------------------------------------------------
CodexActivityMonitor::~CodexActivityMonitor() = default;

// ----------------------------------------------------------------------------
// Demarre la surveillance et branche sa fenetre de notification.
// ----------------------------------------------------------------------------
bool CodexActivityMonitor::Start(HWND hwnd) {
    return impl_->Start(hwnd);
}

// ----------------------------------------------------------------------------
// Arrete la surveillance et libere ses handles.
// ----------------------------------------------------------------------------
void CodexActivityMonitor::Stop() {
    impl_->Stop();
}

// ----------------------------------------------------------------------------
// Indique si le thread de surveillance est possede.
// ----------------------------------------------------------------------------
bool CodexActivityMonitor::IsActive() const {
    return impl_->IsActive();
}

// ----------------------------------------------------------------------------
// Transfere le dernier snapshot publie.
// ----------------------------------------------------------------------------
std::optional<CodexActivitySnapshot> CodexActivityMonitor::TakeSnapshot() {
    return impl_->TakeSnapshot();
}

// ============================================================================
// Codex Glass - Scanner des sessions locales Codex
// ----------------------------------------------------------------------------
// Ce fichier lit les JSONL ligne par ligne, convertit les compteurs cumulatifs
// en deltas et ne conserve que des agregats techniques sans contenu utilisateur.
// ============================================================================

#include "CodexSessionScanner.h"
#include "TokenUsageBuckets.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string_view>
#include <utility>

namespace {

// Nom attribue aux evenements dont le modele est absent.
constexpr wchar_t kUnknownModel[] = L"Inconnu";

// ----------------------------------------------------------------------------
// Vue memoire en lecture seule d'un fichier partage avec le processus Codex.
// ----------------------------------------------------------------------------
class MappedReadOnlyFile {
public:
    // ------------------------------------------------------------------------
    // Libere automatiquement la vue et les handles Windows.
    // ------------------------------------------------------------------------
    ~MappedReadOnlyFile() {
        if (data_ != nullptr) {
            UnmapViewOfFile(data_);
        }
        if (mapping_ != nullptr) {
            CloseHandle(mapping_);
        }
        if (file_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_);
        }
    }

    // ------------------------------------------------------------------------
    // Ouvre et mappe la taille instantanee du fichier.
    // ------------------------------------------------------------------------
    bool Open(const std::filesystem::path& path, std::uintmax_t size) {
        if (size == 0 || size > static_cast<std::uintmax_t>(SIZE_MAX)) {
            return false;
        }
        file_ = CreateFileW(
            path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr
        );
        if (file_ == INVALID_HANDLE_VALUE) {
            return false;
        }
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mapping_ == nullptr) {
            return false;
        }
        data_ = static_cast<const char*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, static_cast<SIZE_T>(size)));
        size_ = data_ != nullptr ? static_cast<std::size_t>(size) : 0;
        return data_ != nullptr;
    }

    // ------------------------------------------------------------------------
    // Retourne le debut de la vue mappee.
    // ------------------------------------------------------------------------
    const char* Data() const {
        return data_;
    }

    // ------------------------------------------------------------------------
    // Retourne la taille stable de la vue mappee.
    // ------------------------------------------------------------------------
    std::size_t Size() const {
        return size_;
    }

private:
    // Handle du fichier partage.
    HANDLE file_ = INVALID_HANDLE_VALUE;

    // Handle de mapping possede.
    HANDLE mapping_ = nullptr;

    // Adresse de la vue en lecture seule.
    const char* data_ = nullptr;

    // Taille de la vue en octets.
    std::size_t size_ = 0;
};

// ----------------------------------------------------------------------------
// Additionne des compteurs sans recompter leurs sous-ensembles.
// ----------------------------------------------------------------------------
void AddCounts(TokenUsageCounts& target, const TokenUsageCounts& value) {
    const auto add = [](std::uint64_t left, std::uint64_t right) {
        return left > std::numeric_limits<std::uint64_t>::max() - right
            ? std::numeric_limits<std::uint64_t>::max()
            : left + right;
    };
    target.input_tokens = add(target.input_tokens, value.input_tokens);
    target.cached_input_tokens = add(target.cached_input_tokens, value.cached_input_tokens);
    target.output_tokens = add(target.output_tokens, value.output_tokens);
    target.reasoning_output_tokens = add(
        target.reasoning_output_tokens,
        value.reasoning_output_tokens
    );
}

// ----------------------------------------------------------------------------
// Incremente un compteur de requetes sans debordement entier.
//
// Parametres :
// - value : compteur a incrementer.
//
// Effet de bord :
// - sature le compteur a la valeur maximale de uint64_t.
// ----------------------------------------------------------------------------
void IncrementRequestCount(std::uint64_t& value) {
    if (value < std::numeric_limits<std::uint64_t>::max()) {
        ++value;
    }
}

// ----------------------------------------------------------------------------
// Additionne un nombre de requetes sans debordement entier.
//
// Parametres :
// - target : compteur recevant la valeur.
// - value : nombre de requetes a ajouter.
//
// Effet de bord :
// - sature le compteur a la valeur maximale de uint64_t.
// ----------------------------------------------------------------------------
void AddRequestCount(std::uint64_t& target, std::uint64_t value) {
    target = target > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : target + value;
}

// ----------------------------------------------------------------------------
// Soustrait deux compteurs cumulatifs sans produire de valeur negative.
// ----------------------------------------------------------------------------
TokenUsageCounts DeltaCounts(const TokenUsageCounts& current, const TokenUsageCounts& previous) {
    TokenUsageCounts delta{};
    delta.input_tokens = current.input_tokens >= previous.input_tokens
        ? current.input_tokens - previous.input_tokens
        : current.input_tokens;
    delta.cached_input_tokens = current.cached_input_tokens >= previous.cached_input_tokens
        ? current.cached_input_tokens - previous.cached_input_tokens
        : current.cached_input_tokens;
    delta.output_tokens = current.output_tokens >= previous.output_tokens
        ? current.output_tokens - previous.output_tokens
        : current.output_tokens;
    delta.reasoning_output_tokens = current.reasoning_output_tokens >= previous.reasoning_output_tokens
        ? current.reasoning_output_tokens - previous.reasoning_output_tokens
        : current.reasoning_output_tokens;
    return delta;
}

// ----------------------------------------------------------------------------
// Convertit une valeur JSON numerique positive en entier non signe.
// ----------------------------------------------------------------------------
std::uint64_t ReadTokenValue(const nlohmann::json& object, const char* key) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_number()) {
        return 0;
    }
    const double value = iterator->get<double>();
    if (!std::isfinite(value) || value <= 0.0) {
        return 0;
    }
    return value >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())
        ? std::numeric_limits<std::uint64_t>::max()
        : static_cast<std::uint64_t>(value);
}

// ----------------------------------------------------------------------------
// Extrait les quatre categories de tokens d'un objet JSON.
// ----------------------------------------------------------------------------
TokenUsageCounts ReadCounts(const nlohmann::json& object) {
    TokenUsageCounts counts{};
    if (!object.is_object()) {
        return counts;
    }
    counts.input_tokens = ReadTokenValue(object, "input_tokens");
    counts.cached_input_tokens = ReadTokenValue(object, "cached_input_tokens");
    counts.output_tokens = ReadTokenValue(object, "output_tokens");
    counts.reasoning_output_tokens = ReadTokenValue(object, "reasoning_output_tokens");
    return counts;
}

// ----------------------------------------------------------------------------
// Convertit une chaine UTF-8 en UTF-16.
// ----------------------------------------------------------------------------
std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring output(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), size);
    return output;
}

// ----------------------------------------------------------------------------
// Lit une chaine JSON optionnelle.
// ----------------------------------------------------------------------------
std::string ReadString(const nlohmann::json& object, const char* key) {
    const auto iterator = object.find(key);
    return iterator != object.end() && iterator->is_string() ? iterator->get<std::string>() : std::string{};
}

// ----------------------------------------------------------------------------
// Parse un timestamp ISO UTC utilise par les sessions Codex.
// ----------------------------------------------------------------------------
std::optional<std::chrono::system_clock::time_point> ParseTimestamp(const std::string& text) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (sscanf_s(
            text.c_str(),
            "%d-%d-%dT%d:%d:%d",
            &year,
            &month,
            &day,
            &hour,
            &minute,
            &second
        ) != 6) {
        return std::nullopt;
    }
    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
    const __time64_t timestamp = _mkgmtime64(&utc);
    if (timestamp < 0) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(timestamp);
}

// ----------------------------------------------------------------------------
// Produit une cle de jour local YYYY-MM-DD.
// ----------------------------------------------------------------------------
std::string LocalDateKey(std::chrono::system_clock::time_point value) {
    const __time64_t timestamp = std::chrono::system_clock::to_time_t(value);
    std::tm local{};
    _localtime64_s(&local, &timestamp);
    char text[11]{};
    std::snprintf(text, sizeof(text), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return text;
}

// ----------------------------------------------------------------------------
// Recule d'un nombre de jours civils locaux.
// ----------------------------------------------------------------------------
std::string LocalDateKeyDaysBefore(std::chrono::system_clock::time_point now, int days_before) {
    const __time64_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    _localtime64_s(&local, &timestamp);
    local.tm_hour = 12;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_mday -= days_before;
    const __time64_t shifted = _mktime64(&local);
    return LocalDateKey(std::chrono::system_clock::from_time_t(shifted));
}

// ----------------------------------------------------------------------------
// Etat minimal necessaire pendant le scan d'un fichier.
// ----------------------------------------------------------------------------
struct FileScanState {
    std::wstring session_id;
    std::wstring model = kUnknownModel;
    TokenUsageCounts previous_total{};
    bool has_previous_total = false;
    TokenSessionUsage session{};
};

// ----------------------------------------------------------------------------
// Ajoute un evenement a un agregat temporel et a sa repartition par modele.
//
// Parametres :
// - aggregate : jour ou heure recevant les compteurs.
// - model_name : modele attribue a l'evenement.
// - counts : delta de tokens a ajouter.
//
// Effet de bord :
// - met a jour les compteurs et le nombre de requetes de l'agregat.
// ----------------------------------------------------------------------------
template<typename Aggregate>
void AddToAggregate(
    Aggregate& aggregate,
    const std::wstring& model_name,
    const TokenUsageCounts& counts
) {
    AddCounts(aggregate.counts, counts);
    IncrementRequestCount(aggregate.request_count);
    auto model = std::find_if(
        aggregate.models.begin(),
        aggregate.models.end(),
        [&model_name](const TokenModelUsage& value) {
            return value.model == model_name;
        }
    );
    if (model == aggregate.models.end()) {
        aggregate.models.push_back(TokenModelUsage{model_name, {}, 0});
        model = std::prev(aggregate.models.end());
    }
    AddCounts(model->counts, counts);
    IncrementRequestCount(model->request_count);
}

// ----------------------------------------------------------------------------
// Ajoute un evenement aux agregats quotidiens et infra-journaliers.
//
// Parametres :
// - days : agregats quotidiens du fichier.
// - hours : agregats horaires recents du fichier.
// - five_minutes : agregats recents par tranches de cinq minutes.
// - state : metadonnees et compteurs de la session.
// - date_key : jour civil local de l'evenement.
// - timestamp : instant UTC non ambigu de l'evenement.
// - first_hour_key : premiere heure a conserver dans le cache incremental.
// - first_five_minute_key : premiere tranche de cinq minutes a conserver.
// - counts : delta de tokens a ajouter.
// ----------------------------------------------------------------------------
void AddEvent(
    std::map<std::string, TokenDailyUsage>& days,
    std::map<std::int64_t, TokenHourlyUsage>& hours,
    std::map<std::int64_t, TokenFiveMinuteUsage>& five_minutes,
    FileScanState& state,
    const std::string& date_key,
    std::chrono::system_clock::time_point timestamp,
    std::int64_t first_hour_key,
    std::int64_t first_five_minute_key,
    const TokenUsageCounts& counts
) {
    TokenDailyUsage& day = days[date_key];
    day.date_key = date_key;
    AddToAggregate(day, state.model, counts);

    const auto hour_start = TokenLocalHourStart(timestamp);
    const std::int64_t hour_key = TokenHourKey(hour_start);
    if (hour_key >= first_hour_key) {
        TokenHourlyUsage& hour = hours[hour_key];
        hour.bucket_start = hour_start;
        AddToAggregate(hour, state.model, counts);
    }

    const auto five_minute_start = TokenFiveMinuteStart(timestamp);
    const std::int64_t five_minute_key = TokenHourKey(five_minute_start);
    if (five_minute_key >= first_five_minute_key) {
        TokenFiveMinuteUsage& bucket = five_minutes[five_minute_key];
        bucket.bucket_start = five_minute_start;
        AddToAggregate(bucket, state.model, counts);
    }

    AddCounts(state.session.counts, counts);
    IncrementRequestCount(state.session.request_count);
    state.session.last_activity = std::max(state.session.last_activity, timestamp);
}

// ----------------------------------------------------------------------------
// Elague les agregats sortis des couvertures quotidienne et horaire.
//
// Parametres :
// - cached : cache du fichier a nettoyer.
// - first_date : premier jour civil conserve.
// - last_date : dernier jour civil conserve.
// - first_hour_key : premiere heure non ambigue conservee.
// - first_five_minute_key : premiere tranche de cinq minutes conservee.
//
// Effet de bord :
// - supprime uniquement les agregats devenus inutiles, jamais les curseurs.
// ----------------------------------------------------------------------------
void PruneCachedAggregates(
    TokenFileScanCache& cached,
    const std::string& first_date,
    const std::string& last_date,
    std::int64_t first_hour_key,
    std::int64_t first_five_minute_key
) {
    std::erase_if(cached.days, [&first_date, &last_date](const auto& value) {
        return value.first < first_date || value.first > last_date;
    });
    std::erase_if(cached.hours, [first_hour_key](const auto& value) {
        return value.first < first_hour_key;
    });
    std::erase_if(cached.five_minutes, [first_five_minute_key](const auto& value) {
        return value.first < first_five_minute_key;
    });
}

// ----------------------------------------------------------------------------
// Analyse un fichier JSONL et alimente les agregats de la periode.
// ----------------------------------------------------------------------------
void ScanFile(
    const std::filesystem::path& path,
    const std::string& first_date,
    const std::string& last_date,
    std::int64_t first_hour_key,
    std::int64_t first_five_minute_key,
    TokenFileScanCache& cached,
    const std::atomic_bool* cancellation_requested
) {
    PruneCachedAggregates(
        cached,
        first_date,
        last_date,
        first_hour_key,
        first_five_minute_key
    );
    std::error_code metadata_error;
    const std::uintmax_t current_size = std::filesystem::file_size(path, metadata_error);
    if (metadata_error) {
        return;
    }
    const auto modified = std::filesystem::last_write_time(path, metadata_error);
    if (metadata_error) {
        return;
    }
    const std::int64_t modified_ticks = modified.time_since_epoch().count();
    if (cached.file_size == current_size && cached.modified_ticks == modified_ticks) {
        return;
    }

    const bool can_resume = !cached.path.empty()
        && current_size >= cached.file_size
        && current_size > cached.file_size
        && cached.valid_offset <= cached.file_size;
    TokenFileScanCache working = can_resume ? cached : TokenFileScanCache{};
    working.path = path;
    MappedReadOnlyFile input;
    if (!input.Open(path, current_size)) {
        return;
    }

    FileScanState state{};
    state.session_id = working.session_id.empty() ? path.stem().wstring() : working.session_id;
    state.model = working.model.empty() ? kUnknownModel : working.model;
    state.previous_total = working.previous_total;
    state.has_previous_total = working.has_previous_total;
    state.session = working.session;
    std::size_t offset = can_resume ? static_cast<std::size_t>(working.valid_offset) : 0;
    while (offset < input.Size()) {
        if (cancellation_requested != nullptr && cancellation_requested->load()) {
            return;
        }
        const char* line_start = input.Data() + offset;
        const void* newline_address = std::memchr(line_start, '\n', input.Size() - offset);
        const char* line_end = newline_address != nullptr
            ? static_cast<const char*>(newline_address)
            : input.Data() + input.Size();
        const std::size_t next_position = newline_address != nullptr
            ? static_cast<std::size_t>(line_end - input.Data()) + 1
            : input.Size();
        if (line_end > line_start && line_end[-1] == '\r') {
            --line_end;
        }
        const std::string_view line(line_start, static_cast<std::size_t>(line_end - line_start));
        offset = next_position;
        const std::size_t type_position = line.find("\"type\":\"");
        const std::size_t type_value = type_position == std::string_view::npos
            ? std::string_view::npos
            : type_position + 8;
        const bool direct_metadata = type_value != std::string_view::npos
            && (line.compare(type_value, 12, "session_meta") == 0
                || line.compare(type_value, 12, "turn_context") == 0);
        const bool token_event = type_value != std::string_view::npos
            && line.compare(type_value, 9, "event_msg") == 0
            && line.find("\"token_count\"", type_value + 9) != std::string_view::npos;
        const bool relevant_line = direct_metadata || token_event;
        if (!relevant_line) {
            working.valid_offset = next_position;
            continue;
        }
        const nlohmann::json record = nlohmann::json::parse(line.begin(), line.end(), nullptr, false);
        if (record.is_discarded() || !record.is_object()) {
            break;
        }
        working.valid_offset = next_position;
        const std::string type = ReadString(record, "type");
        const auto payload_iterator = record.find("payload");
        const nlohmann::json empty = nlohmann::json::object();
        const nlohmann::json& payload = payload_iterator != record.end() && payload_iterator->is_object()
            ? *payload_iterator
            : empty;
        if (type == "session_meta") {
            const std::string id = ReadString(payload, "id");
            if (!id.empty()) {
                state.session_id = Utf8ToWide(id);
            }
            continue;
        }
        if (type == "turn_context") {
            const std::string model = ReadString(payload, "model");
            if (!model.empty()) {
                state.model = Utf8ToWide(model);
            }
            continue;
        }
        if (type != "event_msg" || ReadString(payload, "type") != "token_count") {
            continue;
        }

        const auto timestamp = ParseTimestamp(ReadString(record, "timestamp"));
        if (!timestamp.has_value()) {
            continue;
        }
        const std::string date_key = LocalDateKey(*timestamp);
        const auto info_iterator = payload.find("info");
        if (info_iterator == payload.end() || !info_iterator->is_object()) {
            continue;
        }
        const nlohmann::json& info = *info_iterator;
        const std::string model = ReadString(info, "model");
        if (!model.empty()) {
            state.model = Utf8ToWide(model);
        }

        TokenUsageCounts last{};
        bool has_last = false;
        if (const auto iterator = info.find("last_token_usage"); iterator != info.end() && iterator->is_object()) {
            last = ReadCounts(*iterator);
            has_last = last.Total() > 0;
        }
        TokenUsageCounts total{};
        bool has_total = false;
        if (const auto iterator = info.find("total_token_usage"); iterator != info.end() && iterator->is_object()) {
            total = ReadCounts(*iterator);
            has_total = total.Total() > 0;
        }
        const TokenUsageCounts delta = has_last
            ? last
            : (has_total && state.has_previous_total ? DeltaCounts(total, state.previous_total) : total);
        if (has_total) {
            state.previous_total = total;
            state.has_previous_total = true;
        }
        if (date_key < first_date || date_key > last_date) {
            continue;
        }
        if (delta.Total() == 0) {
            continue;
        }

        const std::string turn_id = ReadString(payload, "turn_id");
        const std::string event_key = !turn_id.empty()
            ? "turn:" + turn_id
            : "event:" + ReadString(record, "timestamp") + ":" + std::to_string(total.Total());
        if (!working.seen_events.insert(event_key).second) {
            continue;
        }
        state.session.session_id = state.session_id;
        AddEvent(
            working.days,
            working.hours,
            working.five_minutes,
            state,
            date_key,
            *timestamp,
            first_hour_key,
            first_five_minute_key,
            delta
        );
    }
    working.file_size = current_size;
    working.modified_ticks = modified_ticks;
    working.session_id = state.session_id;
    working.model = state.model;
    working.previous_total = state.previous_total;
    working.has_previous_total = state.has_previous_total;
    working.session = std::move(state.session);
    cached = std::move(working);
}

// ----------------------------------------------------------------------------
// Enumere les fichiers JSONL d'un repertoire, recursivement ou non.
// ----------------------------------------------------------------------------
std::vector<std::filesystem::path> EnumerateJsonl(const std::filesystem::path& root, bool recursive) {
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::exists(root, error)) {
        return files;
    }
    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
            if (!error && entry.is_regular_file() && entry.path().extension() == L".jsonl") {
                files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
            if (!error && entry.is_regular_file() && entry.path().extension() == L".jsonl") {
                files.push_back(entry.path());
            }
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace

// ----------------------------------------------------------------------------
// Cree un scanner pour le profil Codex actif.
// ----------------------------------------------------------------------------
CodexSessionScanner::CodexSessionScanner() : codex_home_(ResolveCodexHome()) {}

// ----------------------------------------------------------------------------
// Cree un scanner pour un profil explicite.
// ----------------------------------------------------------------------------
CodexSessionScanner::CodexSessionScanner(std::filesystem::path codex_home)
    : codex_home_(std::move(codex_home)) {}

// ----------------------------------------------------------------------------
// Resolut le repertoire CODEX_HOME actif sans lire auth.json.
// ----------------------------------------------------------------------------
std::filesystem::path CodexSessionScanner::ResolveCodexHome() {
    const DWORD codex_size = GetEnvironmentVariableW(L"CODEX_HOME", nullptr, 0);
    if (codex_size > 0) {
        std::wstring value(codex_size, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"CODEX_HOME", value.data(), codex_size);
        if (written > 0 && written < codex_size) {
            value.resize(written);
            return value;
        }
    }
    const DWORD profile_size = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    if (profile_size > 0) {
        std::wstring value(profile_size, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"USERPROFILE", value.data(), profile_size);
        if (written > 0 && written < profile_size) {
            value.resize(written);
            return std::filesystem::path(value) / L".codex";
        }
    }
    return std::filesystem::path(L".codex");
}

// ----------------------------------------------------------------------------
// Analyse une fenetre de jours jusqu'a l'instant fourni.
// ----------------------------------------------------------------------------
TokenUsageSnapshot CodexSessionScanner::Scan(
    std::chrono::system_clock::time_point now,
    int coverage_days,
    const std::atomic_bool* cancellation_requested,
    TokenScanCache* cache
) const {
    TokenUsageSnapshot snapshot{};
    snapshot.updated_at = now;
    snapshot.coverage_days = std::clamp(coverage_days, 1, 365);
    std::error_code error;
    snapshot.available = std::filesystem::exists(codex_home_, error);
    if (!snapshot.available) {
        snapshot.freshness = TokenUsageFreshness::Error;
        snapshot.error_message = L"Repertoire de sessions Codex introuvable";
    }

    const std::string last_date = LocalDateKeyDaysBefore(now, 0);
    const std::string first_date = LocalDateKeyDaysBefore(now, snapshot.coverage_days - 1);
    const auto current_hour = TokenLocalHourStart(now);
    const std::int64_t first_cached_hour_key = TokenHourKey(
        current_hour - std::chrono::hours{kTokenHourlyCacheRetentionHours - 1}
    );
    const auto current_five_minute = TokenFiveMinuteStart(now);
    const std::int64_t first_cached_five_minute_key = TokenHourKey(
        current_five_minute
        - std::chrono::minutes{kTokenFiveMinuteCacheRetentionMinutes - 5}
    );
    std::map<std::string, TokenDailyUsage> days;
    for (int offset = snapshot.coverage_days - 1; offset >= 0; --offset) {
        const std::string key = LocalDateKeyDaysBefore(now, offset);
        days.emplace(key, TokenDailyUsage{key});
    }
    std::map<std::int64_t, TokenHourlyUsage> visible_hours;
    for (TokenHourlyUsage& hour : BuildRecentTokenHours(now, kTokenHourlyGraphBucketCount)) {
        visible_hours.emplace(TokenHourKey(hour.bucket_start), std::move(hour));
    }
    std::map<std::int64_t, TokenFiveMinuteUsage> visible_five_minutes;
    for (TokenFiveMinuteUsage& bucket : BuildRecentTokenFiveMinutes(now, kTokenFiveMinuteGraphBucketCount)) {
        visible_five_minutes.emplace(TokenHourKey(bucket.bucket_start), std::move(bucket));
    }

    std::vector<std::filesystem::path> files = EnumerateJsonl(codex_home_ / L"sessions", true);
    std::vector<std::filesystem::path> archived = EnumerateJsonl(codex_home_ / L"archived_sessions", false);
    files.insert(files.end(), archived.begin(), archived.end());
    const auto oldest_relevant_write = std::filesystem::file_time_type::clock::now()
        - std::chrono::hours{24 * (snapshot.coverage_days + 1)};
    files.erase(std::remove_if(files.begin(), files.end(), [oldest_relevant_write](const auto& file) {
        std::error_code write_error;
        const auto write_time = std::filesystem::last_write_time(file, write_error);
        return write_error || write_time < oldest_relevant_write;
    }), files.end());
    TokenScanCache local_cache;
    TokenScanCache& active_cache = cache != nullptr ? *cache : local_cache;
    std::set<std::wstring> present_files;
    for (const auto& file : files) {
        if (cancellation_requested != nullptr && cancellation_requested->load()) {
            break;
        }
        std::error_code path_error;
        const std::filesystem::path normalized = std::filesystem::weakly_canonical(file, path_error);
        const std::wstring key = (path_error ? file.lexically_normal() : normalized).wstring();
        present_files.insert(key);
        TokenFileScanCache& entry = active_cache.files[key];
        ScanFile(
            file,
            first_date,
            last_date,
            first_cached_hour_key,
            first_cached_five_minute_key,
            entry,
            cancellation_requested
        );
    }
    for (auto iterator = active_cache.files.begin(); iterator != active_cache.files.end();) {
        if (!present_files.contains(iterator->first)) {
            iterator = active_cache.files.erase(iterator);
        } else {
            ++iterator;
        }
    }

    std::map<std::wstring, const TokenFileScanCache*> selected_sessions;
    for (const auto& [path, entry] : active_cache.files) {
        const std::wstring identity = entry.session_id.empty() ? path : entry.session_id;
        const auto selected = selected_sessions.find(identity);
        if (selected == selected_sessions.end()
            || entry.file_size > selected->second->file_size
            || (entry.file_size == selected->second->file_size
                && entry.modified_ticks > selected->second->modified_ticks)) {
            selected_sessions[identity] = &entry;
        }
    }
    for (const auto& [identity, entry_pointer] : selected_sessions) {
        (void)identity;
        const TokenFileScanCache& entry = *entry_pointer;
        for (const auto& [date_key, cached_day] : entry.days) {
            if (date_key < first_date || date_key > last_date) {
                continue;
            }
            TokenDailyUsage& day = days[date_key];
            AddCounts(day.counts, cached_day.counts);
            AddRequestCount(day.request_count, cached_day.request_count);
            for (const TokenModelUsage& cached_model : cached_day.models) {
                auto model = std::find_if(day.models.begin(), day.models.end(), [&cached_model](const TokenModelUsage& value) {
                    return value.model == cached_model.model;
                });
                if (model == day.models.end()) {
                    day.models.push_back(cached_model);
                } else {
                    AddCounts(model->counts, cached_model.counts);
                    AddRequestCount(model->request_count, cached_model.request_count);
                }
            }
        }
        for (const auto& [hour_key, cached_hour] : entry.hours) {
            const auto visible = visible_hours.find(hour_key);
            if (visible == visible_hours.end()) {
                continue;
            }
            TokenHourlyUsage& hour = visible->second;
            AddCounts(hour.counts, cached_hour.counts);
            AddRequestCount(hour.request_count, cached_hour.request_count);
            for (const TokenModelUsage& cached_model : cached_hour.models) {
                auto model = std::find_if(
                    hour.models.begin(),
                    hour.models.end(),
                    [&cached_model](const TokenModelUsage& value) {
                        return value.model == cached_model.model;
                    }
                );
                if (model == hour.models.end()) {
                    hour.models.push_back(cached_model);
                } else {
                    AddCounts(model->counts, cached_model.counts);
                    AddRequestCount(model->request_count, cached_model.request_count);
                }
            }
        }
        for (const auto& [bucket_key, cached_bucket] : entry.five_minutes) {
            const auto visible = visible_five_minutes.find(bucket_key);
            if (visible == visible_five_minutes.end()) {
                continue;
            }
            TokenFiveMinuteUsage& bucket = visible->second;
            AddCounts(bucket.counts, cached_bucket.counts);
            AddRequestCount(bucket.request_count, cached_bucket.request_count);
            for (const TokenModelUsage& cached_model : cached_bucket.models) {
                auto model = std::find_if(
                    bucket.models.begin(),
                    bucket.models.end(),
                    [&cached_model](const TokenModelUsage& value) {
                        return value.model == cached_model.model;
                    }
                );
                if (model == bucket.models.end()) {
                    bucket.models.push_back(cached_model);
                } else {
                    AddCounts(model->counts, cached_model.counts);
                    AddRequestCount(model->request_count, cached_model.request_count);
                }
            }
        }
        if (entry.session.request_count > 0) {
            snapshot.sessions.push_back(entry.session);
        }
    }

    snapshot.daily.reserve(days.size());
    for (auto& [key, day] : days) {
        (void)key;
        snapshot.daily.push_back(std::move(day));
    }
    snapshot.hourly.reserve(visible_hours.size());
    for (auto& [key, hour] : visible_hours) {
        (void)key;
        snapshot.hourly.push_back(std::move(hour));
    }
    snapshot.five_minute.reserve(visible_five_minutes.size());
    for (auto& [key, bucket] : visible_five_minutes) {
        (void)key;
        snapshot.five_minute.push_back(std::move(bucket));
    }
    if (snapshot.available) {
        snapshot.freshness = snapshot.sessions.empty()
            ? TokenUsageFreshness::Empty
            : TokenUsageFreshness::Fresh;
    }
    return snapshot;
}

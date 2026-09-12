// ============================================================================
// Codex Glass - Stockage incremental de consommation de tokens
// ----------------------------------------------------------------------------
// Ce fichier serialise uniquement les compteurs et metadonnees de scan dans
// SQLite. Toutes les mises a jour d'un scan sont transactionnelles.
// ============================================================================

#include "TokenUsageStore.h"

#include <sqlite3.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <nlohmann/json.hpp>
#include <utility>

namespace {

// Version du payload JSON incluant les agregats par tranches de cinq minutes.
constexpr int kTokenScanCacheSchemaVersion = 4;

// Schema du cache technique par fichier.
constexpr char kCreateFilesSql[] =
    "CREATE TABLE IF NOT EXISTS token_scan_files ("
    "path TEXT PRIMARY KEY, file_size INTEGER NOT NULL, modified_ticks INTEGER NOT NULL, "
    "valid_offset INTEGER NOT NULL, payload TEXT NOT NULL);";

// Schema des agregats journaliers directement exploitables par l'UI.
constexpr char kCreateDailySql[] =
    "CREATE TABLE IF NOT EXISTS token_daily_aggregates ("
    "date_key TEXT PRIMARY KEY, input_tokens INTEGER NOT NULL, cached_input_tokens INTEGER NOT NULL, "
    "output_tokens INTEGER NOT NULL, reasoning_output_tokens INTEGER NOT NULL, request_count INTEGER NOT NULL, "
    "updated_at INTEGER NOT NULL);";

// Schema des agregats horaires recents directement exploitables par l'UI.
constexpr char kCreateHourlySql[] =
    "CREATE TABLE IF NOT EXISTS token_hourly_aggregates ("
    "bucket_start INTEGER PRIMARY KEY, input_tokens INTEGER NOT NULL, "
    "cached_input_tokens INTEGER NOT NULL, output_tokens INTEGER NOT NULL, "
    "reasoning_output_tokens INTEGER NOT NULL, request_count INTEGER NOT NULL, "
    "updated_at INTEGER NOT NULL);";

// Schema des agregats de cinq minutes directement exploitables par l'UI.
constexpr char kCreateFiveMinuteSql[] =
    "CREATE TABLE IF NOT EXISTS token_five_minute_aggregates ("
    "bucket_start INTEGER PRIMARY KEY, input_tokens INTEGER NOT NULL, "
    "cached_input_tokens INTEGER NOT NULL, output_tokens INTEGER NOT NULL, "
    "reasoning_output_tokens INTEGER NOT NULL, request_count INTEGER NOT NULL, "
    "updated_at INTEGER NOT NULL);";

// Convertit une chaine UTF-16 en UTF-8 pour SQLite et JSON.
std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// Convertit une chaine UTF-8 en UTF-16.
std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

// Copie les compteurs dans un objet JSON compact.
nlohmann::json CountsToJson(const TokenUsageCounts& counts) {
    return {
        {"input", counts.input_tokens},
        {"cached", counts.cached_input_tokens},
        {"output", counts.output_tokens},
        {"reasoning", counts.reasoning_output_tokens},
    };
}

// Relit les compteurs depuis un objet JSON interne.
TokenUsageCounts CountsFromJson(const nlohmann::json& value) {
    TokenUsageCounts counts{};
    counts.input_tokens = value.value("input", std::uint64_t{0});
    counts.cached_input_tokens = value.value("cached", std::uint64_t{0});
    counts.output_tokens = value.value("output", std::uint64_t{0});
    counts.reasoning_output_tokens = value.value("reasoning", std::uint64_t{0});
    return counts;
}

// ----------------------------------------------------------------------------
// Convertit un compteur non signe vers l'entier signe accepte par SQLite.
//
// Parametres :
// - value : compteur a persister.
//
// Retour :
// - valeur conservee ou saturee a la limite positive de SQLite.
// ----------------------------------------------------------------------------
sqlite3_int64 ToSqliteInteger(std::uint64_t value) {
    // Plus grand compteur representable par le type entier signe de SQLite.
    constexpr std::uint64_t kMaximumSqliteInteger = static_cast<std::uint64_t>(
        std::numeric_limits<sqlite3_int64>::max()
    );
    return static_cast<sqlite3_int64>(std::min(value, kMaximumSqliteInteger));
}

// ----------------------------------------------------------------------------
// Relit un compteur SQLite en refusant les valeurs negatives inattendues.
//
// Parametres :
// - statement : requete positionnee sur une ligne.
// - column : index de la colonne a lire.
//
// Retour :
// - compteur positif, zero servant de repli pour une valeur negative.
// ----------------------------------------------------------------------------
std::uint64_t ReadUnsignedColumn(sqlite3_stmt* statement, int column) {
    const sqlite3_int64 value = sqlite3_column_int64(statement, column);
    return value > 0 ? static_cast<std::uint64_t>(value) : 0;
}

// Serialise l'etat fonctionnel d'un fichier sans son chemin ni son contenu.
std::string SerializeEntry(const TokenFileScanCache& entry) {
    nlohmann::json root{
        {"schema_version", kTokenScanCacheSchemaVersion},
        {"session_id", WideToUtf8(entry.session_id)},
        {"model", WideToUtf8(entry.model)},
        {"previous_total", CountsToJson(entry.previous_total)},
        {"has_previous_total", entry.has_previous_total},
        {"events", entry.seen_events},
        {"days", nlohmann::json::array()},
        {"hours", nlohmann::json::array()},
        {"five_minutes", nlohmann::json::array()},
    };
    for (const auto& [key, day] : entry.days) {
        nlohmann::json models = nlohmann::json::array();
        for (const TokenModelUsage& model : day.models) {
            models.push_back({
                {"name", WideToUtf8(model.model)},
                {"counts", CountsToJson(model.counts)},
                {"requests", model.request_count},
            });
        }
        root["days"].push_back({
            {"date", key}, {"counts", CountsToJson(day.counts)},
            {"requests", day.request_count}, {"models", std::move(models)},
        });
    }
    for (const auto& [key, hour] : entry.hours) {
        nlohmann::json models = nlohmann::json::array();
        for (const TokenModelUsage& model : hour.models) {
            models.push_back({
                {"name", WideToUtf8(model.model)},
                {"counts", CountsToJson(model.counts)},
                {"requests", model.request_count},
            });
        }
        root["hours"].push_back({
            {"bucket_start", key}, {"counts", CountsToJson(hour.counts)},
            {"requests", hour.request_count}, {"models", std::move(models)},
        });
    }
    for (const auto& [key, bucket] : entry.five_minutes) {
        nlohmann::json models = nlohmann::json::array();
        for (const TokenModelUsage& model : bucket.models) {
            models.push_back({
                {"name", WideToUtf8(model.model)},
                {"counts", CountsToJson(model.counts)},
                {"requests", model.request_count},
            });
        }
        root["five_minutes"].push_back({
            {"bucket_start", key}, {"counts", CountsToJson(bucket.counts)},
            {"requests", bucket.request_count}, {"models", std::move(models)},
        });
    }
    root["session"] = {
        {"id", WideToUtf8(entry.session.session_id)},
        {"last_activity", std::chrono::system_clock::to_time_t(entry.session.last_activity)},
        {"counts", CountsToJson(entry.session.counts)},
        {"requests", entry.session.request_count},
    };
    return root.dump();
}

// Deserialise l'etat fonctionnel d'un fichier depuis le cache interne.
bool DeserializeEntry(const std::string& payload, TokenFileScanCache& entry) {
    const nlohmann::json root = nlohmann::json::parse(payload, nullptr, false);
    if (root.is_discarded()
        || !root.is_object()
        || root.value("schema_version", 0) != kTokenScanCacheSchemaVersion) {
        return false;
    }
    for (const auto& value : root.value("hours", nlohmann::json::array())) {
        if (!value.is_object()) {
            continue;
        }
        const std::int64_t bucket_key = value.value("bucket_start", std::int64_t{0});
        TokenHourlyUsage hour{};
        hour.bucket_start = std::chrono::system_clock::time_point{
            std::chrono::seconds{bucket_key}
        };
        hour.counts = CountsFromJson(value.value("counts", nlohmann::json::object()));
        hour.request_count = value.value("requests", std::uint64_t{0});
        for (const auto& model_value : value.value("models", nlohmann::json::array())) {
            hour.models.push_back(TokenModelUsage{
                Utf8ToWide(model_value.value("name", std::string{})),
                CountsFromJson(model_value.value("counts", nlohmann::json::object())),
                model_value.value("requests", std::uint64_t{0}),
            });
        }
        entry.hours.emplace(bucket_key, std::move(hour));
    }
    for (const auto& value : root.value("five_minutes", nlohmann::json::array())) {
        if (!value.is_object()) {
            continue;
        }
        const std::int64_t bucket_key = value.value("bucket_start", std::int64_t{0});
        TokenFiveMinuteUsage bucket{};
        bucket.bucket_start = std::chrono::system_clock::time_point{
            std::chrono::seconds{bucket_key}
        };
        bucket.counts = CountsFromJson(value.value("counts", nlohmann::json::object()));
        bucket.request_count = value.value("requests", std::uint64_t{0});
        for (const auto& model_value : value.value("models", nlohmann::json::array())) {
            bucket.models.push_back(TokenModelUsage{
                Utf8ToWide(model_value.value("name", std::string{})),
                CountsFromJson(model_value.value("counts", nlohmann::json::object())),
                model_value.value("requests", std::uint64_t{0}),
            });
        }
        entry.five_minutes.emplace(bucket_key, std::move(bucket));
    }
    entry.session_id = Utf8ToWide(root.value("session_id", std::string{}));
    entry.model = Utf8ToWide(root.value("model", std::string{}));
    entry.previous_total = CountsFromJson(root.value("previous_total", nlohmann::json::object()));
    entry.has_previous_total = root.value("has_previous_total", false);
    for (const auto& event : root.value("events", nlohmann::json::array())) {
        if (event.is_string()) {
            entry.seen_events.insert(event.get<std::string>());
        }
    }
    for (const auto& value : root.value("days", nlohmann::json::array())) {
        if (!value.is_object()) {
            continue;
        }
        TokenDailyUsage day{};
        day.date_key = value.value("date", std::string{});
        day.counts = CountsFromJson(value.value("counts", nlohmann::json::object()));
        day.request_count = value.value("requests", std::uint64_t{0});
        for (const auto& model_value : value.value("models", nlohmann::json::array())) {
            day.models.push_back(TokenModelUsage{
                Utf8ToWide(model_value.value("name", std::string{})),
                CountsFromJson(model_value.value("counts", nlohmann::json::object())),
                model_value.value("requests", std::uint64_t{0}),
            });
        }
        if (!day.date_key.empty()) {
            entry.days.emplace(day.date_key, std::move(day));
        }
    }
    const nlohmann::json session = root.value("session", nlohmann::json::object());
    entry.session.session_id = Utf8ToWide(session.value("id", std::string{}));
    entry.session.last_activity = std::chrono::system_clock::from_time_t(session.value("last_activity", std::time_t{0}));
    entry.session.counts = CountsFromJson(session.value("counts", nlohmann::json::object()));
    entry.session.request_count = session.value("requests", std::uint64_t{0});
    return true;
}

// Execute une instruction SQL sans resultat.
bool Execute(sqlite3* database, const char* sql) {
    return sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

} // namespace

// ----------------------------------------------------------------------------
// Ferme automatiquement la connexion.
// ----------------------------------------------------------------------------
TokenUsageStore::~TokenUsageStore() {
    Close();
}

// ----------------------------------------------------------------------------
// Ouvre la base et initialise les tables tokens.
// ----------------------------------------------------------------------------
bool TokenUsageStore::Open(const std::wstring& database_path) {
    Close();
    const std::string path = WideToUtf8(database_path);
    if (sqlite3_open_v2(path.c_str(), &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK
        || !Execute(database_, "PRAGMA busy_timeout=3000;")
        || !Execute(database_, kCreateFilesSql)
        || !Execute(database_, kCreateDailySql)
        || !Execute(database_, kCreateHourlySql)
        || !Execute(database_, kCreateFiveMinuteSql)) {
        last_error_ = L"Impossible d'initialiser le cache de tokens";
        Close();
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// Ferme la connexion courante.
// ----------------------------------------------------------------------------
void TokenUsageStore::Close() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

// ----------------------------------------------------------------------------
// Charge les curseurs et agregats techniques par fichier.
// ----------------------------------------------------------------------------
bool TokenUsageStore::LoadScanCache(TokenScanCache& cache) {
    cache.files.clear();
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT path,file_size,modified_ticks,valid_offset,payload FROM token_scan_files;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char* path_text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* payload_text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        if (path_text == nullptr || payload_text == nullptr) {
            continue;
        }
        TokenFileScanCache entry{};
        entry.path = Utf8ToWide(path_text);
        entry.file_size = static_cast<std::uintmax_t>(sqlite3_column_int64(statement, 1));
        entry.modified_ticks = sqlite3_column_int64(statement, 2);
        entry.valid_offset = static_cast<std::uintmax_t>(sqlite3_column_int64(statement, 3));
        if (DeserializeEntry(payload_text, entry)) {
            cache.files.emplace(entry.path.wstring(), std::move(entry));
        }
    }
    sqlite3_finalize(statement);
    return true;
}

// ----------------------------------------------------------------------------
// Enregistre atomiquement le cache et les trois granularites du snapshot.
// ----------------------------------------------------------------------------
bool TokenUsageStore::Save(const TokenScanCache& cache, const TokenUsageSnapshot& snapshot) {
    if (!Execute(database_, "BEGIN IMMEDIATE;") || !Execute(database_, "DELETE FROM token_scan_files;")) {
        Execute(database_, "ROLLBACK;");
        return false;
    }
    sqlite3_stmt* file_statement = nullptr;
    if (sqlite3_prepare_v2(database_, "INSERT INTO token_scan_files(path,file_size,modified_ticks,valid_offset,payload) VALUES(?,?,?,?,?);", -1, &file_statement, nullptr) != SQLITE_OK) {
        Execute(database_, "ROLLBACK;");
        return false;
    }
    for (const auto& [key, entry] : cache.files) {
        const std::string path = WideToUtf8(key);
        const std::string payload = SerializeEntry(entry);
        sqlite3_bind_text(file_statement, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(file_statement, 2, static_cast<sqlite3_int64>(entry.file_size));
        sqlite3_bind_int64(file_statement, 3, entry.modified_ticks);
        sqlite3_bind_int64(file_statement, 4, static_cast<sqlite3_int64>(entry.valid_offset));
        sqlite3_bind_text(file_statement, 5, payload.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(file_statement) != SQLITE_DONE) {
            sqlite3_finalize(file_statement);
            Execute(database_, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(file_statement);
        sqlite3_clear_bindings(file_statement);
    }
    sqlite3_finalize(file_statement);
    if (!Execute(database_, "DELETE FROM token_daily_aggregates;")
        || !Execute(database_, "DELETE FROM token_hourly_aggregates;")
        || !Execute(database_, "DELETE FROM token_five_minute_aggregates;")) {
        Execute(database_, "ROLLBACK;");
        return false;
    }
    sqlite3_stmt* day_statement = nullptr;
    if (sqlite3_prepare_v2(database_, "INSERT INTO token_daily_aggregates VALUES(?,?,?,?,?,?,?);", -1, &day_statement, nullptr) != SQLITE_OK) {
        Execute(database_, "ROLLBACK;");
        return false;
    }
    const sqlite3_int64 updated_at = std::chrono::system_clock::to_time_t(snapshot.updated_at);
    for (const TokenDailyUsage& day : snapshot.daily) {
        sqlite3_bind_text(day_statement, 1, day.date_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(day_statement, 2, ToSqliteInteger(day.counts.input_tokens));
        sqlite3_bind_int64(day_statement, 3, ToSqliteInteger(day.counts.cached_input_tokens));
        sqlite3_bind_int64(day_statement, 4, ToSqliteInteger(day.counts.output_tokens));
        sqlite3_bind_int64(day_statement, 5, ToSqliteInteger(day.counts.reasoning_output_tokens));
        sqlite3_bind_int64(day_statement, 6, ToSqliteInteger(day.request_count));
        sqlite3_bind_int64(day_statement, 7, updated_at);
        if (sqlite3_step(day_statement) != SQLITE_DONE) {
            sqlite3_finalize(day_statement);
            Execute(database_, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(day_statement);
        sqlite3_clear_bindings(day_statement);
    }
    sqlite3_finalize(day_statement);
    sqlite3_stmt* hour_statement = nullptr;
    if (sqlite3_prepare_v2(database_, "INSERT INTO token_hourly_aggregates VALUES(?,?,?,?,?,?,?);", -1, &hour_statement, nullptr) != SQLITE_OK) {
        Execute(database_, "ROLLBACK;");
        return false;
    }
    for (const TokenHourlyUsage& hour : snapshot.hourly) {
        sqlite3_bind_int64(
            hour_statement,
            1,
            static_cast<sqlite3_int64>(std::chrono::system_clock::to_time_t(hour.bucket_start))
        );
        sqlite3_bind_int64(hour_statement, 2, ToSqliteInteger(hour.counts.input_tokens));
        sqlite3_bind_int64(hour_statement, 3, ToSqliteInteger(hour.counts.cached_input_tokens));
        sqlite3_bind_int64(hour_statement, 4, ToSqliteInteger(hour.counts.output_tokens));
        sqlite3_bind_int64(hour_statement, 5, ToSqliteInteger(hour.counts.reasoning_output_tokens));
        sqlite3_bind_int64(hour_statement, 6, ToSqliteInteger(hour.request_count));
        sqlite3_bind_int64(hour_statement, 7, updated_at);
        if (sqlite3_step(hour_statement) != SQLITE_DONE) {
            sqlite3_finalize(hour_statement);
            Execute(database_, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(hour_statement);
        sqlite3_clear_bindings(hour_statement);
    }
    sqlite3_finalize(hour_statement);
    sqlite3_stmt* five_minute_statement = nullptr;
    if (sqlite3_prepare_v2(database_, "INSERT INTO token_five_minute_aggregates VALUES(?,?,?,?,?,?,?);", -1, &five_minute_statement, nullptr) != SQLITE_OK) {
        Execute(database_, "ROLLBACK;");
        return false;
    }
    for (const TokenFiveMinuteUsage& bucket : snapshot.five_minute) {
        sqlite3_bind_int64(
            five_minute_statement,
            1,
            static_cast<sqlite3_int64>(std::chrono::system_clock::to_time_t(bucket.bucket_start))
        );
        sqlite3_bind_int64(five_minute_statement, 2, ToSqliteInteger(bucket.counts.input_tokens));
        sqlite3_bind_int64(five_minute_statement, 3, ToSqliteInteger(bucket.counts.cached_input_tokens));
        sqlite3_bind_int64(five_minute_statement, 4, ToSqliteInteger(bucket.counts.output_tokens));
        sqlite3_bind_int64(five_minute_statement, 5, ToSqliteInteger(bucket.counts.reasoning_output_tokens));
        sqlite3_bind_int64(five_minute_statement, 6, ToSqliteInteger(bucket.request_count));
        sqlite3_bind_int64(five_minute_statement, 7, updated_at);
        if (sqlite3_step(five_minute_statement) != SQLITE_DONE) {
            sqlite3_finalize(five_minute_statement);
            Execute(database_, "ROLLBACK;");
            return false;
        }
        sqlite3_reset(five_minute_statement);
        sqlite3_clear_bindings(five_minute_statement);
    }
    sqlite3_finalize(five_minute_statement);
    return Execute(database_, "COMMIT;");
}

// ----------------------------------------------------------------------------
// Charge le dernier snapshot multi-granularite valide en solution de repli.
// ----------------------------------------------------------------------------
bool TokenUsageStore::LoadLatestSnapshot(TokenUsageSnapshot& snapshot) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT date_key,input_tokens,cached_input_tokens,output_tokens,reasoning_output_tokens,request_count,updated_at FROM token_daily_aggregates ORDER BY date_key;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    snapshot = TokenUsageSnapshot{};
    while (sqlite3_step(statement) == SQLITE_ROW) {
        TokenDailyUsage day{};
        day.date_key = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        day.counts.input_tokens = ReadUnsignedColumn(statement, 1);
        day.counts.cached_input_tokens = ReadUnsignedColumn(statement, 2);
        day.counts.output_tokens = ReadUnsignedColumn(statement, 3);
        day.counts.reasoning_output_tokens = ReadUnsignedColumn(statement, 4);
        day.request_count = ReadUnsignedColumn(statement, 5);
        snapshot.updated_at = std::chrono::system_clock::from_time_t(sqlite3_column_int64(statement, 6));
        snapshot.daily.push_back(std::move(day));
    }
    sqlite3_finalize(statement);
    statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT bucket_start,input_tokens,cached_input_tokens,output_tokens,reasoning_output_tokens,request_count,updated_at FROM token_hourly_aggregates ORDER BY bucket_start;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        TokenHourlyUsage hour{};
        hour.bucket_start = std::chrono::system_clock::from_time_t(sqlite3_column_int64(statement, 0));
        hour.counts.input_tokens = ReadUnsignedColumn(statement, 1);
        hour.counts.cached_input_tokens = ReadUnsignedColumn(statement, 2);
        hour.counts.output_tokens = ReadUnsignedColumn(statement, 3);
        hour.counts.reasoning_output_tokens = ReadUnsignedColumn(statement, 4);
        hour.request_count = ReadUnsignedColumn(statement, 5);
        snapshot.updated_at = std::max(
            snapshot.updated_at,
            std::chrono::system_clock::from_time_t(sqlite3_column_int64(statement, 6))
        );
        snapshot.hourly.push_back(std::move(hour));
    }
    sqlite3_finalize(statement);
    statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT bucket_start,input_tokens,cached_input_tokens,output_tokens,reasoning_output_tokens,request_count,updated_at FROM token_five_minute_aggregates ORDER BY bucket_start;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
        TokenFiveMinuteUsage bucket{};
        bucket.bucket_start = std::chrono::system_clock::from_time_t(sqlite3_column_int64(statement, 0));
        bucket.counts.input_tokens = ReadUnsignedColumn(statement, 1);
        bucket.counts.cached_input_tokens = ReadUnsignedColumn(statement, 2);
        bucket.counts.output_tokens = ReadUnsignedColumn(statement, 3);
        bucket.counts.reasoning_output_tokens = ReadUnsignedColumn(statement, 4);
        bucket.request_count = ReadUnsignedColumn(statement, 5);
        snapshot.updated_at = std::max(
            snapshot.updated_at,
            std::chrono::system_clock::from_time_t(sqlite3_column_int64(statement, 6))
        );
        snapshot.five_minute.push_back(std::move(bucket));
    }
    sqlite3_finalize(statement);
    snapshot.available = !snapshot.daily.empty()
        || !snapshot.hourly.empty()
        || !snapshot.five_minute.empty();
    snapshot.freshness = snapshot.available
        ? TokenUsageFreshness::Fresh
        : TokenUsageFreshness::Empty;
    snapshot.coverage_days = static_cast<int>(snapshot.daily.size());
    return snapshot.available;
}

// ----------------------------------------------------------------------------
// Retourne la derniere erreur SQLite lisible.
// ----------------------------------------------------------------------------
const std::wstring& TokenUsageStore::LastError() const {
    return last_error_;
}

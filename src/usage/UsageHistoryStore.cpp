// ============================================================================
// Codex Glass - Implementation du stockage historique SQLite
// ----------------------------------------------------------------------------
// Ce fichier ouvre la base locale portable, cree la table usage_samples et
// enregistre les releves utiles aux graphes.
// ============================================================================

#include "UsageHistoryStore.h"

#include <sqlite3.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <optional>
#include <string>
#include <utility>

namespace {

// Nom du fichier SQLite portable.
constexpr wchar_t kDatabaseFileName[] = L"CodexGlass.db";

// Taille maximale du chemin d'executable Windows lu au demarrage.
constexpr DWORD kExecutablePathBufferLength = 32767;

// Nombre de jours conserves dans l'historique local.
constexpr int kRetentionDays = 90;

// Delai minimal entre deux checkpoints identiques en secondes.
constexpr sqlite3_int64 kCheckpointIntervalSeconds = 60 * 60;

// Ecart minimal de pourcentage considere comme un changement significatif.
constexpr double kPercentChangeEpsilon = 0.01;

// Requete SQL de creation de la table historique principale.
constexpr char kCreateUsageSamplesTableSql[] =
    "CREATE TABLE IF NOT EXISTS usage_samples ("
    "id INTEGER PRIMARY KEY,"
    "sampled_at INTEGER NOT NULL,"
    "five_hour_used REAL,"
    "weekly_used REAL,"
    "five_hour_reset_at INTEGER,"
    "weekly_reset_at INTEGER"
    ");";

// Requete SQL de creation de l'index temporel.
constexpr char kCreateUsageSamplesIndexSql[] =
    "CREATE INDEX IF NOT EXISTS idx_usage_samples_sampled_at "
    "ON usage_samples(sampled_at);";

// Requete SQL de lecture du dernier sample persiste.
constexpr char kSelectLatestSampleSql[] =
    "SELECT sampled_at, five_hour_used, weekly_used, five_hour_reset_at, weekly_reset_at "
    "FROM usage_samples ORDER BY sampled_at DESC LIMIT 1;";

// Requete SQL d'insertion d'un sample historique.
constexpr char kInsertSampleSql[] =
    "INSERT INTO usage_samples "
    "(sampled_at, five_hour_used, weekly_used, five_hour_reset_at, weekly_reset_at) "
    "VALUES (?, ?, ?, ?, ?);";

// Requete SQL de suppression des samples trop anciens.
constexpr char kDeleteOldSamplesSql[] =
    "DELETE FROM usage_samples WHERE sampled_at < ?;";

// Requete SQL de lecture des samples necessaires au graphe.
constexpr char kSelectSamplesSinceSql[] =
    "SELECT sampled_at, five_hour_used, weekly_used, five_hour_reset_at, weekly_reset_at "
    "FROM usage_samples WHERE sampled_at >= ? ORDER BY sampled_at ASC LIMIT ?;";

// Requete SQL de lecture du dernier sample anterieur a une plage.
constexpr char kSelectLatestSampleBeforeSql[] =
    "SELECT sampled_at, five_hour_used, weekly_used, five_hour_reset_at, weekly_reset_at "
    "FROM usage_samples WHERE sampled_at < ? ORDER BY sampled_at DESC LIMIT 1;";

// ----------------------------------------------------------------------------
// Represente le dernier sample stocke pour comparer les nouvelles valeurs.
// ----------------------------------------------------------------------------
struct StoredSample {
    // Date du sample en secondes Unix.
    sqlite3_int64 sampled_at = 0;

    // Pourcentage utilise sur la fenetre 5 h.
    std::optional<double> five_hour_used;

    // Pourcentage utilise sur la fenetre hebdomadaire.
    std::optional<double> weekly_used;

    // Reset 5 h en secondes Unix.
    std::optional<sqlite3_int64> five_hour_reset_at;

    // Reset hebdomadaire en secondes Unix.
    std::optional<sqlite3_int64> weekly_reset_at;
};

// ----------------------------------------------------------------------------
// Assemble deux fragments de chemin Windows.
//
// Parametres :
// - base : chemin de base.
// - child : fragment a ajouter.
//
// Retour :
// - chemin combine avec un separateur si necessaire.
// ----------------------------------------------------------------------------
std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    std::wstring result = base;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/') {
        result.push_back(L'\\');
    }

    result += child;
    return result;
}

// ----------------------------------------------------------------------------
// Supprime le nom de fichier final d'un chemin Windows.
//
// Parametres :
// - path : chemin complet a tronquer.
//
// Retour :
// - dossier parent du chemin fourni.
// ----------------------------------------------------------------------------
std::wstring DirectoryName(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return L".";
    }

    return path.substr(0, separator);
}

// ----------------------------------------------------------------------------
// Retourne le dossier contenant l'executable courant.
//
// Retour :
// - chemin du dossier de l'executable, ou dossier courant en secours.
// ----------------------------------------------------------------------------
std::wstring GetExecutableDirectory() {
    std::wstring buffer(kExecutablePathBufferLength, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return L".";
    }

    buffer.resize(length);
    return DirectoryName(buffer);
}

// ----------------------------------------------------------------------------
// Convertit une chaine UTF-8 courte en chaine UTF-16.
//
// Parametres :
// - value : texte UTF-8 a convertir.
//
// Retour :
// - texte UTF-16 equivalent.
// ----------------------------------------------------------------------------
std::wstring Utf8ToWide(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return L"";
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
    if (length <= 0) {
        return L"Erreur SQLite";
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value, -1, result.data(), length);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }

    return result;
}

// ----------------------------------------------------------------------------
// Convertit un time_point en secondes Unix.
//
// Parametres :
// - value : date a convertir.
//
// Retour :
// - timestamp Unix en secondes.
// ----------------------------------------------------------------------------
sqlite3_int64 ToUnixSeconds(std::chrono::system_clock::time_point value) {
    return static_cast<sqlite3_int64>(std::chrono::system_clock::to_time_t(value));
}

// ----------------------------------------------------------------------------
// Convertit des secondes Unix en time_point.
//
// Parametres :
// - value : timestamp Unix en secondes.
//
// Retour :
// - date C++ correspondante.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point FromUnixSeconds(sqlite3_int64 value) {
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(value));
}

// ----------------------------------------------------------------------------
// Convertit une borne minimale en secondes Unix en arrondissant vers le futur.
//
// Parametres :
// - value : date minimale a convertir.
//
// Retour :
// - premiere seconde Unix qui n'est pas anterieure a la date.
// ----------------------------------------------------------------------------
sqlite3_int64 ToUnixCeilingSeconds(std::chrono::system_clock::time_point value) {
    sqlite3_int64 seconds = ToUnixSeconds(value);
    if (FromUnixSeconds(seconds) < value) {
        ++seconds;
    }
    return seconds;
}

// ----------------------------------------------------------------------------
// Indique si une date de reset est exploitable.
//
// Parametres :
// - value : date de reset a verifier.
//
// Retour :
// - true si la date contient une valeur non nulle.
// - false sinon.
// ----------------------------------------------------------------------------
bool HasTimeValue(std::chrono::system_clock::time_point value) {
    return value.time_since_epoch().count() != 0;
}

// ----------------------------------------------------------------------------
// Execute une requete SQL sans resultat.
//
// Parametres :
// - database : connexion SQLite cible.
// - sql : requete SQL a executer.
// - last_error : message d'erreur a renseigner en cas d'echec.
//
// Retour :
// - true si la requete a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool ExecuteSql(sqlite3* database, const char* sql, std::wstring& last_error) {
    char* error_message = nullptr;
    const int result = sqlite3_exec(database, sql, nullptr, nullptr, &error_message);
    if (result == SQLITE_OK) {
        return true;
    }

    last_error = Utf8ToWide(error_message);
    sqlite3_free(error_message);
    return false;
}

// ----------------------------------------------------------------------------
// Prepare une requete SQLite.
//
// Parametres :
// - database : connexion SQLite cible.
// - sql : requete SQL a preparer.
// - statement : statement produit.
// - last_error : message d'erreur a renseigner en cas d'echec.
//
// Retour :
// - true si la preparation a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool PrepareStatement(sqlite3* database, const char* sql, sqlite3_stmt** statement, std::wstring& last_error) {
    const int result = sqlite3_prepare_v2(database, sql, -1, statement, nullptr);
    if (result == SQLITE_OK) {
        return true;
    }

    last_error = Utf8ToWide(sqlite3_errmsg(database));
    return false;
}

// ----------------------------------------------------------------------------
// Lit un double optionnel depuis une colonne SQLite.
//
// Parametres :
// - statement : statement SQLite positionne sur une ligne.
// - column : index de colonne a lire.
//
// Retour :
// - valeur double ou absence de valeur.
// ----------------------------------------------------------------------------
std::optional<double> ReadOptionalDouble(sqlite3_stmt* statement, int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }

    return sqlite3_column_double(statement, column);
}

// ----------------------------------------------------------------------------
// Lit un entier optionnel depuis une colonne SQLite.
//
// Parametres :
// - statement : statement SQLite positionne sur une ligne.
// - column : index de colonne a lire.
//
// Retour :
// - valeur entiere ou absence de valeur.
// ----------------------------------------------------------------------------
std::optional<sqlite3_int64> ReadOptionalInt64(sqlite3_stmt* statement, int column) {
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }

    return sqlite3_column_int64(statement, column);
}

// ----------------------------------------------------------------------------
// Convertit la ligne courante d'un statement en sample historique public.
//
// Parametres :
// - statement : statement SQLite positionne sur une ligne compatible.
//
// Retour :
// - sample historique complet.
// ----------------------------------------------------------------------------
UsageHistorySample ReadUsageHistorySample(sqlite3_stmt* statement) {
    UsageHistorySample sample{};
    sample.sampled_at = FromUnixSeconds(sqlite3_column_int64(statement, 0));
    sample.five_hour_used_percent = ReadOptionalDouble(statement, 1);
    sample.weekly_used_percent = ReadOptionalDouble(statement, 2);

    const std::optional<sqlite3_int64> five_hour_reset_at = ReadOptionalInt64(statement, 3);
    if (five_hour_reset_at) {
        sample.five_hour_reset_at = FromUnixSeconds(*five_hour_reset_at);
    }

    const std::optional<sqlite3_int64> weekly_reset_at = ReadOptionalInt64(statement, 4);
    if (weekly_reset_at) {
        sample.weekly_reset_at = FromUnixSeconds(*weekly_reset_at);
    }

    return sample;
}

// ----------------------------------------------------------------------------
// Lit le dernier sample enregistre.
//
// Parametres :
// - database : connexion SQLite cible.
// - last_error : message d'erreur a renseigner en cas d'echec.
//
// Retour :
// - dernier sample, ou absence de sample.
// ----------------------------------------------------------------------------
std::optional<StoredSample> ReadLatestSample(sqlite3* database, std::wstring& last_error) {
    sqlite3_stmt* statement = nullptr;
    if (!PrepareStatement(database, kSelectLatestSampleSql, &statement, last_error)) {
        return std::nullopt;
    }

    const int step_result = sqlite3_step(statement);
    if (step_result == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return std::nullopt;
    }

    if (step_result != SQLITE_ROW) {
        last_error = Utf8ToWide(sqlite3_errmsg(database));
        sqlite3_finalize(statement);
        return std::nullopt;
    }

    StoredSample sample{};
    sample.sampled_at = sqlite3_column_int64(statement, 0);
    sample.five_hour_used = ReadOptionalDouble(statement, 1);
    sample.weekly_used = ReadOptionalDouble(statement, 2);
    sample.five_hour_reset_at = ReadOptionalInt64(statement, 3);
    sample.weekly_reset_at = ReadOptionalInt64(statement, 4);
    sqlite3_finalize(statement);
    return sample;
}

// ----------------------------------------------------------------------------
// Compare deux doubles optionnels avec une tolerance.
//
// Parametres :
// - left : premiere valeur.
// - right : seconde valeur.
//
// Retour :
// - true si les valeurs sont differentes.
// - false sinon.
// ----------------------------------------------------------------------------
bool OptionalDoubleChanged(std::optional<double> left, std::optional<double> right) {
    if (left.has_value() != right.has_value()) {
        return true;
    }

    if (!left && !right) {
        return false;
    }

    return std::fabs(*left - *right) >= kPercentChangeEpsilon;
}

// ----------------------------------------------------------------------------
// Indique si le nouveau sample doit etre persiste.
//
// Parametres :
// - latest : dernier sample persiste.
// - current : sample courant construit depuis le releve.
//
// Retour :
// - true si une insertion est necessaire.
// - false sinon.
// ----------------------------------------------------------------------------
bool ShouldInsertSample(const StoredSample& latest, const StoredSample& current) {
    if (OptionalDoubleChanged(latest.five_hour_used, current.five_hour_used)) {
        return true;
    }

    if (OptionalDoubleChanged(latest.weekly_used, current.weekly_used)) {
        return true;
    }

    if (latest.five_hour_reset_at != current.five_hour_reset_at) {
        return true;
    }

    if (latest.weekly_reset_at != current.weekly_reset_at) {
        return true;
    }

    return current.sampled_at - latest.sampled_at >= kCheckpointIntervalSeconds;
}

// ----------------------------------------------------------------------------
// Indique si un releve est assez fiable pour etre historise.
//
// Parametres :
// - snapshot : releve a verifier.
//
// Retour :
// - true si le releve peut etre enregistre.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsRecordableSnapshot(const UsageSnapshot& snapshot) {
    if (snapshot.freshness != UsageFreshness::Fresh && snapshot.freshness != UsageFreshness::Stale) {
        return false;
    }

    return snapshot.five_hour_available || snapshot.weekly_available;
}

// ----------------------------------------------------------------------------
// Construit un sample stockable a partir d'un releve d'usage.
//
// Parametres :
// - snapshot : releve source.
//
// Retour :
// - sample pret pour SQLite.
// ----------------------------------------------------------------------------
StoredSample MakeStoredSample(const UsageSnapshot& snapshot) {
    StoredSample sample{};
    sample.sampled_at = ToUnixSeconds(snapshot.sampled_at);
    sample.five_hour_used = snapshot.five_hour_available
        ? std::optional<double>{std::clamp(snapshot.five_hour_used_percent, 0.0, 100.0)}
        : std::nullopt;
    sample.weekly_used = snapshot.weekly_available
        ? std::optional<double>{std::clamp(snapshot.weekly_used_percent, 0.0, 100.0)}
        : std::nullopt;
    sample.five_hour_reset_at = snapshot.five_hour_available && HasTimeValue(snapshot.five_hour_reset_at)
        ? std::optional<sqlite3_int64>{ToUnixSeconds(snapshot.five_hour_reset_at)}
        : std::nullopt;
    sample.weekly_reset_at = snapshot.weekly_available && HasTimeValue(snapshot.weekly_reset_at)
        ? std::optional<sqlite3_int64>{ToUnixSeconds(snapshot.weekly_reset_at)}
        : std::nullopt;
    return sample;
}

// ----------------------------------------------------------------------------
// Lie une valeur double optionnelle sur un statement SQLite.
//
// Parametres :
// - statement : statement cible.
// - index : index du parametre SQL.
// - value : valeur optionnelle a lier.
// ----------------------------------------------------------------------------
void BindOptionalDouble(sqlite3_stmt* statement, int index, std::optional<double> value) {
    if (value) {
        sqlite3_bind_double(statement, index, *value);
        return;
    }

    sqlite3_bind_null(statement, index);
}

// ----------------------------------------------------------------------------
// Lie une valeur entiere optionnelle sur un statement SQLite.
//
// Parametres :
// - statement : statement cible.
// - index : index du parametre SQL.
// - value : valeur optionnelle a lier.
// ----------------------------------------------------------------------------
void BindOptionalInt64(sqlite3_stmt* statement, int index, std::optional<sqlite3_int64> value) {
    if (value) {
        sqlite3_bind_int64(statement, index, *value);
        return;
    }

    sqlite3_bind_null(statement, index);
}

}  // namespace

// ----------------------------------------------------------------------------
// Ferme automatiquement la base SQLite encore ouverte.
// ----------------------------------------------------------------------------
UsageHistoryStore::~UsageHistoryStore() {
    Close();
}

// ----------------------------------------------------------------------------
// Ouvre ou cree la base SQLite puis initialise son schema.
//
// Parametres :
// - database_path : chemin complet du fichier SQLite.
//
// Retour :
// - true si la base est prete.
// - false sinon.
// ----------------------------------------------------------------------------
bool UsageHistoryStore::Open(const std::wstring& database_path) {
    Close();

    sqlite3* opened_database = nullptr;
    const int open_result = sqlite3_open16(database_path.c_str(), &opened_database);
    if (open_result != SQLITE_OK) {
        last_error_ = opened_database != nullptr ? Utf8ToWide(sqlite3_errmsg(opened_database)) : L"Ouverture SQLite impossible";
        if (opened_database != nullptr) {
            sqlite3_close(opened_database);
        }
        return false;
    }

    database_ = opened_database;
    sqlite3_busy_timeout(database_, 1000);

    if (!ExecuteSql(database_, kCreateUsageSamplesTableSql, last_error_)) {
        Close();
        return false;
    }

    if (!ExecuteSql(database_, kCreateUsageSamplesIndexSql, last_error_)) {
        Close();
        return false;
    }

    last_error_.clear();
    return true;
}

// ----------------------------------------------------------------------------
// Ferme la connexion SQLite courante.
// ----------------------------------------------------------------------------
void UsageHistoryStore::Close() {
    if (database_ == nullptr) {
        return;
    }

    sqlite3_close(database_);
    database_ = nullptr;
}

// ----------------------------------------------------------------------------
// Indique si la base SQLite est actuellement ouverte.
//
// Retour :
// - true si une connexion SQLite est disponible.
// - false sinon.
// ----------------------------------------------------------------------------
bool UsageHistoryStore::IsOpen() const {
    return database_ != nullptr;
}

// ----------------------------------------------------------------------------
// Enregistre un releve si les valeurs changent ou si un checkpoint est du.
//
// Parametres :
// - snapshot : releve d'usage a considerer.
//
// Retour :
// - true si le traitement s'est termine sans erreur SQLite.
// - false sinon.
// ----------------------------------------------------------------------------
bool UsageHistoryStore::RecordSnapshot(const UsageSnapshot& snapshot) {
    if (database_ == nullptr || !IsRecordableSnapshot(snapshot)) {
        return true;
    }

    const StoredSample current = MakeStoredSample(snapshot);
    const std::optional<StoredSample> latest = ReadLatestSample(database_, last_error_);
    if (latest && !ShouldInsertSample(*latest, current)) {
        return true;
    }

    sqlite3_stmt* statement = nullptr;
    if (!PrepareStatement(database_, kInsertSampleSql, &statement, last_error_)) {
        return false;
    }

    sqlite3_bind_int64(statement, 1, current.sampled_at);
    BindOptionalDouble(statement, 2, current.five_hour_used);
    BindOptionalDouble(statement, 3, current.weekly_used);
    BindOptionalInt64(statement, 4, current.five_hour_reset_at);
    BindOptionalInt64(statement, 5, current.weekly_reset_at);

    const int step_result = sqlite3_step(statement);
    if (step_result != SQLITE_DONE) {
        last_error_ = Utf8ToWide(sqlite3_errmsg(database_));
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);

    sqlite3_stmt* prune_statement = nullptr;
    if (!PrepareStatement(database_, kDeleteOldSamplesSql, &prune_statement, last_error_)) {
        return false;
    }

    const sqlite3_int64 retention_cutoff = current.sampled_at - (static_cast<sqlite3_int64>(kRetentionDays) * 24 * 60 * 60);
    sqlite3_bind_int64(prune_statement, 1, retention_cutoff);
    const int prune_result = sqlite3_step(prune_statement);
    if (prune_result != SQLITE_DONE) {
        last_error_ = Utf8ToWide(sqlite3_errmsg(database_));
        sqlite3_finalize(prune_statement);
        return false;
    }

    sqlite3_finalize(prune_statement);
    last_error_.clear();
    return true;
}

// ----------------------------------------------------------------------------
// Charge les samples historiques depuis une date minimale.
//
// Parametres :
// - since : date minimale incluse.
// - max_samples : nombre maximal de samples retournes.
//
// Retour :
// - liste ordonnee de samples historiques.
// ----------------------------------------------------------------------------
std::vector<UsageHistorySample> UsageHistoryStore::LoadSamplesSince(
    std::chrono::system_clock::time_point since,
    int max_samples
) const {
    std::vector<UsageHistorySample> samples;
    if (database_ == nullptr || max_samples <= 0) {
        return samples;
    }

    sqlite3_stmt* statement = nullptr;
    if (!PrepareStatement(database_, kSelectSamplesSinceSql, &statement, last_error_)) {
        return samples;
    }

    sqlite3_bind_int64(statement, 1, ToUnixCeilingSeconds(since));
    sqlite3_bind_int(statement, 2, max_samples);

    while (true) {
        const int step_result = sqlite3_step(statement);
        if (step_result == SQLITE_DONE) {
            break;
        }

        if (step_result != SQLITE_ROW) {
            last_error_ = Utf8ToWide(sqlite3_errmsg(database_));
            sqlite3_finalize(statement);
            return {};
        }

        samples.push_back(ReadUsageHistorySample(statement));
    }

    sqlite3_finalize(statement);
    last_error_.clear();
    return samples;
}

// ----------------------------------------------------------------------------
// Charge une plage et prolonge le dernier sample connu jusqu'a son debut.
//
// Parametres :
// - range_start : borne temporelle gauche incluse.
// - max_samples : nombre maximal de samples reels retournes.
//
// Retour :
// - liste ordonnee, eventuellement precedee d'un point de continuite.
// ----------------------------------------------------------------------------
std::vector<UsageHistorySample> UsageHistoryStore::LoadSamplesForRange(
    std::chrono::system_clock::time_point range_start,
    int max_samples
) const {
    std::vector<UsageHistorySample> samples = LoadSamplesSince(range_start, max_samples);
    if (database_ == nullptr || max_samples <= 0 || !last_error_.empty()) {
        return samples;
    }

    if (!samples.empty() && samples.front().sampled_at == range_start) {
        return samples;
    }

    sqlite3_stmt* statement = nullptr;
    if (!PrepareStatement(database_, kSelectLatestSampleBeforeSql, &statement, last_error_)) {
        return samples;
    }

    sqlite3_bind_int64(statement, 1, ToUnixCeilingSeconds(range_start));
    const int step_result = sqlite3_step(statement);
    if (step_result == SQLITE_ROW) {
        UsageHistorySample boundary = ReadUsageHistorySample(statement);
        boundary.sampled_at = range_start;
        samples.insert(samples.begin(), std::move(boundary));
    } else if (step_result != SQLITE_DONE) {
        last_error_ = Utf8ToWide(sqlite3_errmsg(database_));
    }

    sqlite3_finalize(statement);
    if (step_result == SQLITE_ROW || step_result == SQLITE_DONE) {
        last_error_.clear();
    }
    return samples;
}

// ----------------------------------------------------------------------------
// Retourne la derniere erreur lisible produite par le store.
//
// Retour :
// - message d'erreur courant, ou chaine vide.
// ----------------------------------------------------------------------------
const std::wstring& UsageHistoryStore::LastError() const {
    return last_error_;
}

// ----------------------------------------------------------------------------
// Retourne le chemin portable de la base historique.
//
// Retour :
// - chemin complet vers CodexGlass.db a cote de l'executable.
// ----------------------------------------------------------------------------
std::wstring GetUsageHistoryDatabasePath() {
    return JoinPath(GetExecutableDirectory(), kDatabaseFileName);
}

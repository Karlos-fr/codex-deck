// ============================================================================
// Codex Deck - Implementation du service de preferences
// ----------------------------------------------------------------------------
// Ce fichier utilise un statement prepare et ignore les champs JSON inconnus
// pour permettre les evolutions futures du document.
// ============================================================================

#include "DeckPreferencesService.h"

#include <nlohmann/json.hpp>
#include <sqlite3.h>

namespace {

// Cle stable du document de preferences.
constexpr char kPreferencesKey[] = "preferences";
// Version courante du document JSON.
constexpr int kPreferencesVersion = 1;

// Convertit un mode de theme en valeur persistante.
const char* ThemeName(ThemeMode mode) {
    switch (mode) {
    case ThemeMode::Light:
        return "light";
    case ThemeMode::Dark:
        return "dark";
    case ThemeMode::System:
    default:
        return "system";
    }
}

// Parse un mode de theme tolerant.
ThemeMode ParseTheme(std::string_view value) {
    if (value == "light") {
        return ThemeMode::Light;
    }
    if (value == "dark") {
        return ThemeMode::Dark;
    }
    return ThemeMode::System;
}

// Construit une erreur SQLite locale.
StorageError SqlError(SqliteDatabase& database, int code) {
    return StorageError{StorageErrorCode::SqlFailed, code, sqlite3_errmsg(database.handle())};
}

}  // namespace

// Cree le service sur une base non possedee.
DeckPreferencesService::DeckPreferencesService(SqliteDatabase& database)
    : database_(database) {
}

// Charge les preferences ou retourne les valeurs par defaut si absentes.
std::expected<DeckPreferences, StorageError> DeckPreferencesService::Load() {
    sqlite3_stmt* statement = nullptr;
    int result = sqlite3_prepare_v2(database_.handle(), "SELECT value_json FROM workspace_state WHERE key=?;", -1, &statement, nullptr);
    if (result != SQLITE_OK) {
        return std::unexpected(SqlError(database_, result));
    }
    sqlite3_bind_text(statement, 1, kPreferencesKey, -1, SQLITE_STATIC);
    result = sqlite3_step(statement);
    if (result == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return DeckPreferences{};
    }
    if (result != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return std::unexpected(SqlError(database_, result));
    }
    const char* raw = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    const nlohmann::json json = nlohmann::json::parse(raw == nullptr ? "{}" : raw, nullptr, false);
    sqlite3_finalize(statement);
    if (json.is_discarded() || !json.is_object()) {
        return DeckPreferences{};
    }
    DeckPreferences preferences{};
    preferences.theme_mode = ParseTheme(json.value("theme", "system"));
    preferences.notify_approvals = json.value("notifyApprovals", true);
    preferences.notify_errors = json.value("notifyErrors", true);
    preferences.notify_completions = json.value("notifyCompletions", true);
    preferences.reduced_motion_override = json.value("reducedMotionOverride", false);
    return preferences;
}

// Sauvegarde le document JSON versionne.
std::expected<void, StorageError> DeckPreferencesService::Save(const DeckPreferences& preferences) {
    const std::string json = nlohmann::json{
        {"version", kPreferencesVersion},
        {"theme", ThemeName(preferences.theme_mode)},
        {"notifyApprovals", preferences.notify_approvals},
        {"notifyErrors", preferences.notify_errors},
        {"notifyCompletions", preferences.notify_completions},
        {"reducedMotionOverride", preferences.reduced_motion_override},
    }.dump();
    sqlite3_stmt* statement = nullptr;
    int result = sqlite3_prepare_v2(
        database_.handle(),
        "INSERT INTO workspace_state(key,value_json) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value_json=excluded.value_json;",
        -1,
        &statement,
        nullptr
    );
    if (result != SQLITE_OK) {
        return std::unexpected(SqlError(database_, result));
    }
    sqlite3_bind_text(statement, 1, kPreferencesKey, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, json.c_str(), -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return result == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, result));
}

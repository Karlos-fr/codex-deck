// ============================================================================
// Codex Deck - Implementation du service de preferences
// ----------------------------------------------------------------------------
// Ce fichier utilise un statement prepare et ignore les champs JSON inconnus
// pour permettre les evolutions futures du document.
// ============================================================================

#include "DeckPreferencesService.h"

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <algorithm>

namespace {

// Cle stable du document de preferences.
constexpr char kPreferencesKey[] = "preferences";
// Version courante du document JSON.
constexpr int kPreferencesVersion = 2;

// Serialise les reglages Glass dans le document de preferences.
nlohmann::json GlassJson(const DeckGlassSettings& deck) {
    const GlassEffectSettings& glass = deck.effect;
    return {
        {"enabled", deck.enabled}, {"opacity", deck.opacity_percent},
        {"appearance", {{"diffusion", glass.appearance.diffusion_percent}, {"tint", glass.appearance.tint_percent},
            {"grain", glass.appearance.grain_percent}, {"edgeRefraction", glass.appearance.edge_refraction_percent},
            {"edgeWidth", glass.appearance.edge_width_percent}, {"chromatic", glass.appearance.chromatic_aberration_percent},
            {"elementGlass", glass.appearance.element_glass_enabled}, {"elementZoom", glass.appearance.indicator_refraction_percent},
            {"elementExtent", glass.appearance.indicator_width_percent}, {"elementSoftness", glass.appearance.element_softness_percent}}},
        {"calmWater", {{"enabled", glass.calm_water.enabled}, {"intensity", glass.calm_water.intensity_percent},
            {"speed", glass.calm_water.speed_percent}, {"wavelength", glass.calm_water.wavelength_percent}, {"noise", glass.calm_water.noise_percent}}},
        {"liquid", {{"enabled", glass.liquid.enabled}, {"intensity", glass.liquid.intensity_percent},
            {"speed", glass.liquid.speed_percent}, {"wavelength", glass.liquid.wavelength_percent},
            {"fluidity", glass.liquid.fluidity_percent}, {"noise", glass.liquid.noise_percent}}},
        {"rain", {{"enabled", glass.rain.enabled}, {"intensity", glass.rain.intensity_percent},
            {"speed", glass.rain.speed_percent}, {"density", glass.rain.density_percent},
            {"ringSize", glass.rain.ring_size_percent}, {"fade", glass.rain.fade_percent}}},
    };
}

// Charge un bloc Glass tolerant en conservant les valeurs par defaut absentes.
DeckGlassSettings ParseGlass(const nlohmann::json& json) {
    DeckGlassSettings deck{};
    if (!json.is_object()) return deck;
    deck.enabled = json.value("enabled", deck.enabled);
    deck.opacity_percent = std::clamp(json.value("opacity", deck.opacity_percent), 20, 100);
    const auto appearance = json.value("appearance", nlohmann::json::object());
    deck.effect.appearance.diffusion_percent = appearance.value("diffusion", deck.effect.appearance.diffusion_percent);
    deck.effect.appearance.tint_percent = appearance.value("tint", deck.effect.appearance.tint_percent);
    deck.effect.appearance.grain_percent = appearance.value("grain", deck.effect.appearance.grain_percent);
    deck.effect.appearance.edge_refraction_percent = appearance.value("edgeRefraction", deck.effect.appearance.edge_refraction_percent);
    deck.effect.appearance.edge_width_percent = appearance.value("edgeWidth", deck.effect.appearance.edge_width_percent);
    deck.effect.appearance.chromatic_aberration_percent = appearance.value("chromatic", deck.effect.appearance.chromatic_aberration_percent);
    deck.effect.appearance.element_glass_enabled = appearance.value("elementGlass", deck.effect.appearance.element_glass_enabled);
    deck.effect.appearance.indicator_refraction_percent = appearance.value("elementZoom", deck.effect.appearance.indicator_refraction_percent);
    deck.effect.appearance.indicator_width_percent = appearance.value("elementExtent", deck.effect.appearance.indicator_width_percent);
    deck.effect.appearance.element_softness_percent = appearance.value("elementSoftness", deck.effect.appearance.element_softness_percent);
    const auto calm = json.value("calmWater", nlohmann::json::object());
    deck.effect.calm_water.enabled = calm.value("enabled", deck.effect.calm_water.enabled);
    deck.effect.calm_water.intensity_percent = calm.value("intensity", deck.effect.calm_water.intensity_percent);
    deck.effect.calm_water.speed_percent = calm.value("speed", deck.effect.calm_water.speed_percent);
    deck.effect.calm_water.wavelength_percent = calm.value("wavelength", deck.effect.calm_water.wavelength_percent);
    deck.effect.calm_water.noise_percent = calm.value("noise", deck.effect.calm_water.noise_percent);
    const auto liquid = json.value("liquid", nlohmann::json::object());
    deck.effect.liquid.enabled = liquid.value("enabled", deck.effect.liquid.enabled);
    deck.effect.liquid.intensity_percent = liquid.value("intensity", deck.effect.liquid.intensity_percent);
    deck.effect.liquid.speed_percent = liquid.value("speed", deck.effect.liquid.speed_percent);
    deck.effect.liquid.wavelength_percent = liquid.value("wavelength", deck.effect.liquid.wavelength_percent);
    deck.effect.liquid.fluidity_percent = liquid.value("fluidity", deck.effect.liquid.fluidity_percent);
    deck.effect.liquid.noise_percent = liquid.value("noise", deck.effect.liquid.noise_percent);
    const auto rain = json.value("rain", nlohmann::json::object());
    deck.effect.rain.enabled = rain.value("enabled", deck.effect.rain.enabled);
    deck.effect.rain.intensity_percent = rain.value("intensity", deck.effect.rain.intensity_percent);
    deck.effect.rain.speed_percent = rain.value("speed", deck.effect.rain.speed_percent);
    deck.effect.rain.density_percent = rain.value("density", deck.effect.rain.density_percent);
    deck.effect.rain.ring_size_percent = rain.value("ringSize", deck.effect.rain.ring_size_percent);
    deck.effect.rain.fade_percent = rain.value("fade", deck.effect.rain.fade_percent);
    deck.effect = NormalizeGlassEffectSettings(deck.effect);
    return deck;
}

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
    preferences.glass = ParseGlass(json.value("glass", nlohmann::json::object()));
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
        {"glass", GlassJson(preferences.glass)},
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

// ============================================================================
// Codex Deck - Tests du service de preferences
// ----------------------------------------------------------------------------
// Ce fichier valide valeurs par defaut et round-trip du JSON versionne.
// ============================================================================

#include "settings/DeckPreferencesService.h"

#include "storage/SchemaMigrator.h"

#include <windows.h>

// Verifie la persistance independante des preferences globales.
int main() {
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const auto path = std::filesystem::path(temp) / L"CodexDeckTests"
        / (L"preferences-" + std::to_wstring(GetCurrentProcessId()) + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    SchemaMigrator migrator;
    if (!database || !migrator.Migrate(*database)) {
        return 1;
    }
    DeckPreferencesService service(*database);
    const auto defaults = service.Load();
    if (!defaults || defaults->theme_mode != ThemeMode::System || !defaults->notify_approvals) {
        return 2;
    }
    DeckPreferences changed{};
    changed.theme_mode = ThemeMode::Dark;
    changed.notify_errors = false;
    changed.reduced_motion_override = true;
    if (!service.Save(changed)) {
        return 3;
    }
    const auto loaded = service.Load();
    if (!loaded || loaded->theme_mode != ThemeMode::Dark || loaded->notify_errors
        || !loaded->notify_completions || !loaded->reduced_motion_override) {
        return 4;
    }
    return 0;
}

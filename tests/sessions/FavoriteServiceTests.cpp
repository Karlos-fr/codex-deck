// ============================================================================
// Codex Deck - Tests du service favoris
// ----------------------------------------------------------------------------
// Ce fichier valide publication immediate, persistance et rollback SQLite.
// ============================================================================

#include "sessions/FavoriteService.h"

#include "storage/SchemaMigrator.h"

#include <windows.h>

// Verifie le toggle puis le rollback lorsque la table devient indisponible.
int main() {
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const auto path = std::filesystem::path(temp) / L"CodexDeckTests"
        / (L"favorite-" + std::to_wstring(GetCurrentProcessId()) + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    SchemaMigrator migrator;
    if (!database || !migrator.Migrate(*database)) {
        return 1;
    }
    SessionMetadataRepository repository(*database);
    if (!repository.UpsertCache("thr_favorite", "Favorite", L"D:\\Work", 1, SessionStatus::Idle)) {
        return 2;
    }
    SessionCatalog catalog;
    SessionCatalogSnapshot snapshot{};
    SessionRecord record{};
    record.codex.id = "thr_favorite";
    snapshot.sessions.push_back(record);
    catalog.Publish(snapshot);
    FavoriteService service(catalog, repository);
    const auto enabled = service.ToggleFavorite("thr_favorite");
    const auto persisted = repository.Get("thr_favorite");
    if (!enabled || !*enabled || !persisted || !*persisted || !(*persisted)->favorite) {
        return 3;
    }
    if (!database->Execute("DROP TABLE session_metadata;")) {
        return 4;
    }
    const auto failed = service.ToggleFavorite("thr_favorite");
    const auto rolled_back = catalog.Current();
    return !failed && rolled_back && rolled_back->sessions.front().favorite ? 0 : 5;
}

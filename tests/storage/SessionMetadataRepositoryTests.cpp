// ============================================================================
// Codex Deck - Tests du repository metadonnees de session
// ----------------------------------------------------------------------------
// Ce fichier valide le cache leger de session, les favoris et la priorite des
// assignations manuelles.
// ============================================================================

#include "projects/ProjectRepository.h"
#include "storage/SchemaMigrator.h"
#include "storage/SessionMetadataRepository.h"
#include "storage/SqliteDatabase.h"

#include <windows.h>

#include <filesystem>

namespace {

// ----------------------------------------------------------------------------
// Cree une base temporaire migree.
//
// Retour :
// - connexion SQLite prete pour le test.
// ----------------------------------------------------------------------------
SqliteDatabase CreateTestDatabase() {
    wchar_t temp_path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp_path);
    const std::filesystem::path path = std::filesystem::path(temp_path)
        / L"CodexDeckTests"
        / (L"sessions-" + std::to_wstring(GetCurrentProcessId()) + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    SchemaMigrator migrator;
    [[maybe_unused]] const auto migrated = migrator.Migrate(*database);
    return std::move(*database);
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie upsert cache, assignations et favori.
//
// Retour :
// - zero si les metadonnees sont relues correctement.
// ----------------------------------------------------------------------------
int main() {
    auto database = CreateTestDatabase();
    ProjectRepository project_repository(database);
    auto first = project_repository.Create("First");
    auto second = project_repository.Create("Second");
    if (!first || !second) {
        return 1;
    }

    SessionMetadataRepository repository(database);
    if (!repository.UpsertCache("thr_123", "Audio parity", "D:\\VibeCoding\\spotifyamp", 200, SessionStatus::Idle)) {
        return 2;
    }
    if (!repository.SetAssignment("thr_123", first->id, AssignmentSource::Automatic)) {
        return 3;
    }
    if (!repository.SetAssignment("thr_123", second->id, AssignmentSource::Manual)) {
        return 4;
    }
    if (!repository.SetFavorite("thr_123", true)) {
        return 5;
    }

    auto metadata = repository.Get("thr_123");
    if (!metadata || !*metadata) {
        return 6;
    }
    if ((*metadata)->project_id != second->id) {
        return 7;
    }
    if ((*metadata)->assignment_source != AssignmentSource::Manual || !(*metadata)->favorite) {
        return 8;
    }
    if ((*metadata)->cached_title != "Audio parity" || (*metadata)->last_activity != 200) {
        return 9;
    }

    auto all = repository.ListAll();
    if (!all || all->size() != 1) {
        return 10;
    }
    return 0;
}

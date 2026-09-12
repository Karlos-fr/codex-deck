// ============================================================================
// Codex Deck - Tests du repository projets
// ----------------------------------------------------------------------------
// Ce fichier valide la persistance transactionnelle des projets, roots et
// suppression en cascade dans SQLite.
// ============================================================================

#include "projects/ProjectRepository.h"
#include "storage/SchemaMigrator.h"
#include "storage/SqliteDatabase.h"

#include <sqlite3.h>
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
        / (L"projects-" + std::to_wstring(GetCurrentProcessId()) + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    SchemaMigrator migrator;
    [[maybe_unused]] const auto migrated = migrator.Migrate(*database);
    return std::move(*database);
}

// ----------------------------------------------------------------------------
// Compte les roots persistants.
//
// Parametres :
// - database : base a interroger.
//
// Retour :
// - nombre de lignes project_roots.
// ----------------------------------------------------------------------------
int CountRoots(SqliteDatabase& database) {
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database.handle(), "SELECT COUNT(*) FROM project_roots;", -1, &statement, nullptr);
    int count = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        count = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return count;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie create, roots, remote, list et cascade delete.
//
// Retour :
// - zero si le repository conserve les donnees attendues.
// ----------------------------------------------------------------------------
int main() {
    auto database = CreateTestDatabase();
    ProjectRepository repository(database);

    auto created = repository.Create("SpotifyAmp");
    if (!created) {
        return 1;
    }
    if (!repository.AddRoot(created->id, "D:\\VibeCoding\\spotifyamp")) {
        return 2;
    }
    if (!repository.AddRoot(created->id, "D:\\VibeCoding\\spotifyamp\\src")) {
        return 3;
    }
    if (!repository.SetGitRemote(created->id, "https://github.com/example/spotifyamp.git")) {
        return 4;
    }

    auto projects = repository.List();
    if (!projects || projects->size() != 1) {
        return 5;
    }
    if (projects->front().name != "SpotifyAmp" || projects->front().roots.size() != 2) {
        return 6;
    }
    if (!projects->front().git_remote || *projects->front().git_remote != "https://github.com/example/spotifyamp.git") {
        return 7;
    }

    if (!repository.Delete(created->id)) {
        return 8;
    }
    projects = repository.List();
    if (!projects || !projects->empty()) {
        return 9;
    }
    return CountRoots(database) == 0 ? 0 : 10;
}

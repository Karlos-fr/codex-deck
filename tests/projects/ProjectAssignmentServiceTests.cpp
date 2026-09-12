// ============================================================================
// Codex Deck - Tests du service d'assignation projet
// ----------------------------------------------------------------------------
// Ce fichier valide les mutations locales optimistes et leur persistance.
// ============================================================================

#include "projects/ProjectAssignmentService.h"

#include "model/SessionCatalog.h"
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
// Parametres :
// - suffix : suffixe de fichier.
//
// Retour :
// - connexion SQLite prete.
// ----------------------------------------------------------------------------
SqliteDatabase CreateTestDatabase(const wchar_t* suffix) {
    wchar_t temp_path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp_path);
    const std::filesystem::path path = std::filesystem::path(temp_path)
        / L"CodexDeckTests"
        / (std::wstring(L"assign-") + std::to_wstring(GetCurrentProcessId()) + suffix + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    SchemaMigrator migrator;
    [[maybe_unused]] const auto migrated = migrator.Migrate(*database);
    return std::move(*database);
}

// ----------------------------------------------------------------------------
// Publie un snapshot avec une session.
//
// Parametres :
// - catalog : catalogue cible.
// - thread_id : session a publier.
// - project_id : projet initial.
// ----------------------------------------------------------------------------
void PublishOne(SessionCatalog& catalog, const CodexThreadId& thread_id, std::optional<ProjectId> project_id) {
    SessionCatalogSnapshot snapshot{};
    SessionRecord record{};
    record.codex.id = thread_id;
    record.project_id = project_id;
    snapshot.sessions.push_back(record);
    catalog.Publish(std::move(snapshot));
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie l'assignation manuelle optimiste.
//
// Retour :
// - zero si le snapshot et SQLite sont mis a jour.
// ----------------------------------------------------------------------------
int TestAssignManual() {
    auto database = CreateTestDatabase(L"-manual");
    ProjectRepository project_repository(database);
    auto project = project_repository.Create("Target");
    if (!project) {
        return 1;
    }

    SessionCatalog catalog;
    PublishOne(catalog, "thr_assign", std::nullopt);
    SessionMetadataRepository metadata(database);
    ProjectAssignmentService service(catalog, metadata);

    auto assigned = service.AssignManual("thr_assign", project->id);
    auto snapshot = catalog.Current();
    auto persisted = metadata.Get("thr_assign");
    if (!assigned || !snapshot || !persisted || !*persisted) {
        return 2;
    }
    if (snapshot->sessions.front().project_id != project->id
        || snapshot->sessions.front().assignment_source != AssignmentSource::Manual) {
        return 3;
    }
    return (*persisted)->project_id == project->id && (*persisted)->assignment_source == AssignmentSource::Manual ? 0 : 4;
}

// ----------------------------------------------------------------------------
// Verifie le retour manuel vers Unassigned.
//
// Retour :
// - zero si nullopt est persiste comme choix manuel.
// ----------------------------------------------------------------------------
int TestManualUnassigned() {
    auto database = CreateTestDatabase(L"-unassigned");
    SessionCatalog catalog;
    PublishOne(catalog, "thr_unassigned", 42);
    SessionMetadataRepository metadata(database);
    ProjectAssignmentService service(catalog, metadata);

    auto assigned = service.AssignManual("thr_unassigned", std::nullopt);
    auto persisted = metadata.Get("thr_unassigned");
    if (!assigned || !persisted || !*persisted) {
        return 5;
    }
    return !(*persisted)->project_id && (*persisted)->assignment_source == AssignmentSource::Manual ? 0 : 6;
}

// ----------------------------------------------------------------------------
// Verifie le rollback si SQLite refuse l'assignation.
//
// Retour :
// - zero si le snapshot ancien est republie et l'erreur exposee.
// ----------------------------------------------------------------------------
int TestRollbackOnFailure() {
    auto database = CreateTestDatabase(L"-rollback");
    SessionCatalog catalog;
    PublishOne(catalog, "thr_rollback", std::nullopt);
    SessionMetadataRepository metadata(database);
    bool error_reported = false;
    ProjectAssignmentService service(catalog, metadata, [&](const StorageError&) {
        error_reported = true;
    });

    auto assigned = service.AssignManual("thr_rollback", 9999);
    auto snapshot = catalog.Current();
    if (assigned || !snapshot || !error_reported) {
        return 7;
    }
    return !snapshot->sessions.front().project_id && snapshot->sessions.front().assignment_source == AssignmentSource::Automatic ? 0 : 8;
}

// ----------------------------------------------------------------------------
// Execute les tests d'assignation.
//
// Retour :
// - zero si tous les scenarios passent.
// ----------------------------------------------------------------------------
int main() {
    if (const int result = TestAssignManual(); result != 0) {
        return result;
    }
    if (const int result = TestManualUnassigned(); result != 0) {
        return result;
    }
    if (const int result = TestRollbackOnFailure(); result != 0) {
        return result;
    }
    return 0;
}

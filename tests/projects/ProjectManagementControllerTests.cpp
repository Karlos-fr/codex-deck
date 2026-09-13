// ============================================================================
// Codex Deck - Tests du controller de gestion des projets
// ----------------------------------------------------------------------------
// Ce fichier valide create/rename/delete ainsi que la conservation du choix
// Unassigned manuel apres suppression par cle etrangere.
// ============================================================================

#include "projects/ProjectManagementController.h"
#include "storage/SchemaMigrator.h"
#include "storage/SessionMetadataRepository.h"

#include <windows.h>

// Probe Git deterministe simulant un depot connu.
class ProjectControllerGitProbe final : public IGitProjectProbe {
public:
    // Retourne une identite Git avec remote sans lancer de processus.
    std::expected<std::optional<GitProjectIdentity>, ProjectDetectionError> Inspect(
        const std::filesystem::path& cwd
    ) override {
        return std::optional<GitProjectIdentity>{GitProjectIdentity{cwd, "https://example.test/spotifyamp.git"}};
    }
};

// Execute le cycle de vie local complet d'un projet logique.
int main() {
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const auto path = std::filesystem::path(temp) / L"CodexDeckTests"
        / (L"project-controller-" + std::to_wstring(GetCurrentProcessId()) + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    if (!database) {
        return 1;
    }
    SchemaMigrator migrator;
    if (!migrator.Migrate(*database)) {
        return 2;
    }
    ProjectRepository repository(*database);
    ProjectControllerGitProbe git_probe;
    ProjectManagementController controller(repository, &git_probe);
    auto project = controller.CreateProject("SpotifyAmp", L"D:\\VibeCoding\\spotifyamp");
    if (!project || project->roots.size() != 1
        || project->git_remote != "https://example.test/spotifyamp.git") {
        return 3;
    }
    if (!controller.RenameProject(project->id, "SpotifyAmp Native")) {
        return 4;
    }
    SessionMetadataRepository metadata(*database);
    if (!metadata.UpsertCache("thr_manual", "Manual", L"D:\\VibeCoding\\spotifyamp", 1, SessionStatus::Idle)
        || !metadata.SetAssignment("thr_manual", project->id, AssignmentSource::Manual)) {
        return 5;
    }
    if (!controller.DeleteProject(project->id)) {
        return 6;
    }
    const auto projects = repository.List();
    const auto session = metadata.Get("thr_manual");
    if (!projects || !projects->empty() || !session || !*session) {
        return 7;
    }
    return !(*session)->project_id && (*session)->assignment_source == AssignmentSource::Manual ? 0 : 8;
}

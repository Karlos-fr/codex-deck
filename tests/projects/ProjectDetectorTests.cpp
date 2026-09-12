// ============================================================================
// Codex Deck - Tests de detection de projet
// ----------------------------------------------------------------------------
// Ce fichier valide la priorite manual, cwd root le plus long, remote Git et
// Unassigned sans lancer Git.
// ============================================================================

#include "projects/ProjectDetector.h"

namespace {

// ----------------------------------------------------------------------------
// Probe Git de test retournant une identite predefinie.
// ----------------------------------------------------------------------------
class FakeGitProbe final : public IGitProjectProbe {
public:
    // Identite retournee par le probe.
    std::optional<GitProjectIdentity> identity;

    // ------------------------------------------------------------------------
    // Retourne l'identite configuree.
    //
    // Parametres :
    // - cwd : chemin ignore par le fake.
    //
    // Retour :
    // - identite optionnelle.
    // ------------------------------------------------------------------------
    std::expected<std::optional<GitProjectIdentity>, ProjectDetectionError> Inspect(const std::filesystem::path& cwd) override {
        (void)cwd;
        return identity;
    }
};

// ----------------------------------------------------------------------------
// Cree un projet de test.
//
// Parametres :
// - id : identifiant.
// - root : racine optionnelle.
// - remote : remote optionnel.
//
// Retour :
// - projet hydrate.
// ----------------------------------------------------------------------------
Project MakeProject(ProjectId id, std::filesystem::path root, std::optional<std::string> remote = std::nullopt) {
    Project project{};
    project.id = id;
    project.name = "Project";
    if (!root.empty()) {
        project.roots.push_back(std::move(root));
    }
    project.git_remote = std::move(remote);
    return project;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie toutes les priorites de detection.
//
// Retour :
// - zero si les associations sont stables.
// ----------------------------------------------------------------------------
int main() {
    std::vector<Project> projects{
        MakeProject(1, "D:\\VibeCoding\\repo"),
        MakeProject(2, "D:\\VibeCoding\\repo\\subroot"),
        MakeProject(3, "", "https://github.com/example/remote.git"),
    };
    CodexThreadSummary thread{};
    thread.id = "thr_123";
    thread.cwd = "D:\\VibeCoding\\repo\\subroot\\src";

    FakeGitProbe probe;
    SessionMetadata manual{};
    manual.thread_id = "thr_123";
    manual.assignment_source = AssignmentSource::Manual;
    manual.project_id = ProjectId{1};
    if (DetectProject(thread, projects, manual, probe) != ProjectId{1}) {
        return 1;
    }

    manual.project_id.reset();
    if (DetectProject(thread, projects, manual, probe).has_value()) {
        return 2;
    }

    std::optional<SessionMetadata> no_metadata;
    if (DetectProject(thread, projects, no_metadata, probe) != ProjectId{2}) {
        return 3;
    }

    thread.cwd = "D:\\Other\\workspace";
    probe.identity = GitProjectIdentity{std::filesystem::path("D:\\Other\\workspace"), "https://github.com/example/remote.git"};
    if (DetectProject(thread, projects, no_metadata, probe) != ProjectId{3}) {
        return 4;
    }

    probe.identity.reset();
    if (DetectProject(thread, projects, no_metadata, probe).has_value()) {
        return 5;
    }
    return 0;
}

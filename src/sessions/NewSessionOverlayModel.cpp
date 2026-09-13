// ============================================================================
// Codex Deck - Implementation du modele nouvelle session
// ----------------------------------------------------------------------------
// Ce fichier applique le suivi automatique de la racine principale tant que
// l'utilisateur n'a pas choisi explicitement un autre workspace.
// ============================================================================

#include "NewSessionOverlayModel.h"

#include <algorithm>

namespace {

// Retourne la racine principale du projet demande.
std::filesystem::path PrimaryRoot(std::span<const Project> projects, std::optional<ProjectId> project_id) {
    if (!project_id) {
        return {};
    }
    const auto project = std::ranges::find_if(projects, [project_id](const Project& candidate) {
        return candidate.id == *project_id;
    });
    return project != projects.end() && !project->roots.empty() ? project->roots.front() : std::filesystem::path{};
}

}  // namespace

// Initialise le modele depuis le catalogue et le projet courant.
NewSessionOverlayModel BuildNewSessionOverlayModel(
    std::span<const Project> projects,
    std::optional<ProjectId> current_project
) {
    NewSessionOverlayModel model{};
    model.projects.assign(projects.begin(), projects.end());
    model.project_id = current_project;
    model.workspace = PrimaryRoot(projects, current_project);
    return model;
}

// Change le projet et suit sa racine si le workspace est encore automatique.
void SelectNewSessionProject(NewSessionOverlayModel& model, std::optional<ProjectId> project_id) {
    model.project_id = project_id;
    if (!model.workspace_manually_edited) {
        model.workspace = PrimaryRoot(model.projects, project_id);
    }
}

// Remplace le workspace et memorise le choix utilisateur.
void SetNewSessionWorkspace(NewSessionOverlayModel& model, std::filesystem::path workspace, bool user_edited) {
    model.workspace = std::move(workspace);
    model.workspace_manually_edited = user_edited;
}

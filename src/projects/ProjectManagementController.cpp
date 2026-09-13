// ============================================================================
// Codex Deck - Implementation du controller de gestion des projets
// ----------------------------------------------------------------------------
// Ce fichier valide les saisies et compose les operations du repository sans
// effectuer de travail sur les dossiers choisis.
// ============================================================================

#include "ProjectManagementController.h"

namespace {

// Construit une erreur de validation locale.
StorageError ValidationError(const char* message) {
    return StorageError{StorageErrorCode::ConstraintFailed, 0, message};
}

}  // namespace

// Cree un controller sur un repository non possede.
ProjectManagementController::ProjectManagementController(ProjectRepository& repository)
    : repository_(repository) {
}

// Cree un projet et sa racine principale de facon atomique logique.
std::expected<Project, StorageError> ProjectManagementController::CreateProject(
    std::string_view name,
    const std::filesystem::path& root
) {
    if (name.empty()) {
        return std::unexpected(ValidationError("Le nom du projet est vide"));
    }
    if (root.empty()) {
        return std::unexpected(ValidationError("La racine du projet est vide"));
    }
    auto project = repository_.Create(name);
    if (!project) {
        return std::unexpected(project.error());
    }
    if (auto added = repository_.AddRoot(project->id, root); !added) {
        [[maybe_unused]] const auto rollback = repository_.Delete(project->id);
        return std::unexpected(added.error());
    }
    project->roots.push_back(root);
    return project;
}

// Renomme un projet logique sans modifier son workspace.
std::expected<void, StorageError> ProjectManagementController::RenameProject(ProjectId project_id, std::string_view name) {
    if (name.empty()) {
        return std::unexpected(ValidationError("Le nom du projet est vide"));
    }
    return repository_.Rename(project_id, name);
}

// Supprime uniquement le projet logique local.
std::expected<void, StorageError> ProjectManagementController::DeleteProject(ProjectId project_id) {
    return repository_.Delete(project_id);
}

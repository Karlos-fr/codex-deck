// ============================================================================
// Codex Deck - Controller de gestion des projets
// ----------------------------------------------------------------------------
// Ce module orchestre les mutations de projets via le repository local. Il ne
// touche jamais aux fichiers du workspace ni au repository Git.
// ============================================================================

#pragma once

#include "GitProjectProbe.h"
#include "ProjectRepository.h"

// Controller synchrone destine a etre appele depuis un worker applicatif.
class ProjectManagementController {
public:
    // Cree un controller sur un repository non possede.
    explicit ProjectManagementController(ProjectRepository& repository, IGitProjectProbe* git_probe = nullptr);

    // Cree un projet et sa racine principale de facon atomique logique.
    std::expected<Project, StorageError> CreateProject(std::string_view name, const std::filesystem::path& root);

    // Renomme un projet logique sans modifier son workspace.
    std::expected<void, StorageError> RenameProject(ProjectId project_id, std::string_view name);

    // Supprime uniquement le projet logique local.
    std::expected<void, StorageError> DeleteProject(ProjectId project_id);

private:
    // Repository local non possede.
    ProjectRepository& repository_;

    // Probe Git optionnel utilise uniquement pour enrichir un projet cree.
    IGitProjectProbe* git_probe_ = nullptr;
};

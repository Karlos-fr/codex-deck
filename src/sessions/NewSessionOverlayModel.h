// ============================================================================
// Codex Deck - Modele de l'overlay nouvelle session
// ----------------------------------------------------------------------------
// Ce module gere les valeurs project/workspace/model/prompt sans dependance au
// rendu, au picker Windows ou au client Codex.
// ============================================================================

#pragma once

#include "../codex/CodexTypes.h"
#include "../projects/ProjectTypes.h"

#include <optional>
#include <span>

// Etat editable du dialogue global de creation de session.
struct NewSessionOverlayModel {
    // Projet choisi, ou Unassigned.
    std::optional<ProjectId> project_id;
    // Workspace choisi.
    std::filesystem::path workspace;
    // Modele choisi, ou valeur serveur par defaut.
    std::optional<std::string> model;
    // Prompt initial optionnel.
    std::wstring prompt;
    // Catalogue des projets proposes.
    std::vector<Project> projects;
    // Catalogue des modeles charge de facon asynchrone.
    std::vector<CodexModelInfo> models;
    // Indique que le workspace ne doit plus suivre le projet.
    bool workspace_manually_edited = false;
    // Champ actif : projet, workspace, modele ou prompt.
    std::size_t active_field = 0;
    // Indique une creation en cours.
    bool submitting = false;
    // Erreur courante affichee dans l'overlay.
    std::wstring error;
};

// Initialise le modele depuis le catalogue et le projet courant.
NewSessionOverlayModel BuildNewSessionOverlayModel(std::span<const Project> projects, std::optional<ProjectId> current_project);

// Change le projet et suit sa racine si le workspace est encore automatique.
void SelectNewSessionProject(NewSessionOverlayModel& model, std::optional<ProjectId> project_id);

// Remplace le workspace et memorise le choix utilisateur.
void SetNewSessionWorkspace(NewSessionOverlayModel& model, std::filesystem::path workspace, bool user_edited = true);

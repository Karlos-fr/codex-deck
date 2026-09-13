// ============================================================================
// Codex Deck - Modele de l'editeur de projet
// ----------------------------------------------------------------------------
// Ce module conserve la saisie transitoire de l'overlay projet sans connaitre
// Direct2D, SQLite ou le dialogue de selection de dossier.
// ============================================================================

#pragma once

#include "ProjectTypes.h"

#include <filesystem>
#include <optional>
#include <string>

// Mode d'utilisation de l'editeur de projet.
enum class ProjectEditorMode {
    Create,
    Rename,
    ConfirmDelete,
};

// Etat editable de l'overlay projet.
struct ProjectEditorModel {
    // Operation courante.
    ProjectEditorMode mode = ProjectEditorMode::Create;
    // Projet cible pour rename ou suppression.
    std::optional<ProjectId> project_id;
    // Nom saisi ou nom courant.
    std::wstring name;
    // Racine principale choisie.
    std::filesystem::path root;
    // Champ possedant le caret.
    std::size_t active_field = 0;
    // Indique une operation worker en cours.
    bool submitting = false;
    // Message d'erreur affiche sans fermer l'overlay.
    std::wstring error;
};

// ============================================================================
// Codex Deck - Commandes applicatives
// ----------------------------------------------------------------------------
// Ce module definit les commandes stables emises par navigation, palette et
// raccourcis, sans executer directement les actions Codex.
// ============================================================================

#pragma once

#include "../codex/CodexTypes.h"
#include "../projects/ProjectTypes.h"

#include <optional>
#include <filesystem>
#include <string>

// ----------------------------------------------------------------------------
// Type de commande applicative.
// ----------------------------------------------------------------------------
enum class DeckCommandKind {
    // Ouvrir ou reprendre un thread.
    OpenThread,

    // Creer une session globale.
    NewSession,

    // Creer une session dans le projet courant.
    NewSessionInCurrentProject,

    // Renommer le thread courant.
    RenameThread,

    // Archiver le thread courant.
    ArchiveThread,

    // Basculer le favori local.
    ToggleFavorite,

    // Ouvrir le workspace d'un projet.
    OpenWorkspace,

    // Detacher le Workbench courant.
    DetachWorkbench,

    // Ouvrir la Command Palette.
    OpenCommandPalette,

    // Ouvrir la recherche globale.
    OpenSearch,

    // Ouvrir une session runtime par son raccourci numerique.
    OpenRuntimeSlot,

    // Basculer vers la session runtime suivante.
    CycleRuntimeSession,

    // Charger la vue des threads archives.
    OpenArchive,

    // Restaurer un thread archive.
    RestoreArchivedThread,

    // Creer un projet logique local.
    NewProject,

    // Renommer le projet courant.
    RenameProject,

    // Supprimer le projet logique courant.
    DeleteProject,

    // Creer une session depuis un dossier choisi.
    OpenFolderAsSession,

    // Choisir le workspace du formulaire de session sans creer encore.
    PickSessionWorkspace,

    // Suivre le theme Windows.
    SetThemeSystem,

    // Forcer le theme clair.
    SetThemeLight,

    // Forcer le theme sombre.
    SetThemeDark,
};

// ----------------------------------------------------------------------------
// Commande applicative parametree.
// ----------------------------------------------------------------------------
struct DeckCommand {
    // Type de commande.
    DeckCommandKind kind = DeckCommandKind::OpenThread;

    // Projet cible optionnel.
    std::optional<ProjectId> project_id;

    // Thread cible optionnel.
    std::optional<CodexThreadId> thread_id;

    // Workspace cible optionnel d'une creation.
    std::optional<std::filesystem::path> cwd;

    // Modele optionnel d'une creation.
    std::optional<std::string> model;

    // Prompt initial optionnel d'une creation.
    std::optional<std::string> initial_prompt;

    // Libelle saisi pour une mutation de projet.
    std::optional<std::string> display_name;
};

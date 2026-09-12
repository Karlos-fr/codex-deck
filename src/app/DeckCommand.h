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
};

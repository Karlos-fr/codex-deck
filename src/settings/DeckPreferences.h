// ============================================================================
// Codex Deck - Preferences utilisateur
// ----------------------------------------------------------------------------
// Ce module definit les preferences persistantes independantes de la geometrie
// de fenetre et des etats de session.
// ============================================================================

#pragma once

#include "../theme/Theme.h"

// Preferences globales de Codex Deck.
struct DeckPreferences {
    // Theme demande par l'utilisateur.
    ThemeMode theme_mode = ThemeMode::System;
    // Notifications pour demandes d'approbation.
    bool notify_approvals = true;
    // Notifications pour erreurs.
    bool notify_errors = true;
    // Notifications pour fins de travail.
    bool notify_completions = true;
    // Force la reduction des mouvements.
    bool reduced_motion_override = false;
};

// ============================================================================
// Codex Deck - Reglages applicatifs minimaux
// ----------------------------------------------------------------------------
// Ce module declare l'etat de configuration local de la coquille native. Il ne
// persiste rien et reste separe du rendu, du stockage et du protocole Codex.
// ============================================================================

#pragma once

#include "../theme/Theme.h"

// ----------------------------------------------------------------------------
// Regroupe les reglages locaux necessaires au bootstrap natif.
// ----------------------------------------------------------------------------
struct DeckSettings {
    // Mode de theme demande par l'utilisateur.
    ThemeMode theme_mode = ThemeMode::System;
};

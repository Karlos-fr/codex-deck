// ============================================================================
// Codex Deck - Reglages applicatifs minimaux
// ----------------------------------------------------------------------------
// Ce module declare l'etat de configuration local de la coquille native. Il ne
// persiste rien et reste separe du rendu, du stockage et du protocole Codex.
// ============================================================================

#pragma once

// ----------------------------------------------------------------------------
// Regroupe les reglages locaux necessaires au bootstrap natif.
// ----------------------------------------------------------------------------
struct DeckSettings {
    // Indique si la fenetre doit adopter le theme sombre pendant le bootstrap.
    bool dark_theme = false;
};

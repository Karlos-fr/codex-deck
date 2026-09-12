// ============================================================================
// Codex Deck - Raccourcis clavier applicatifs
// ----------------------------------------------------------------------------
// Ce module traduit les accords clavier Win32 en commandes applicatives pures.
// Il ne depend ni du renderer, ni de Codex, ni du stockage.
// ============================================================================

#pragma once

#include "../app/DeckCommand.h"

#include <windows.h>

#include <optional>

// ----------------------------------------------------------------------------
// Decrit un accord clavier normalise.
// ----------------------------------------------------------------------------
struct KeyChord {
    // Touche virtuelle Win32.
    WPARAM virtual_key = 0;

    // Indique que Ctrl est enfonce.
    bool control = false;

    // Indique que Shift est enfonce.
    bool shift = false;
};

// ----------------------------------------------------------------------------
// Traduit un accord clavier en commande applicative.
//
// Parametres :
// - chord : accord normalise.
//
// Retour :
// - commande correspondante, ou rien si l'accord n'est pas gere.
// ----------------------------------------------------------------------------
std::optional<DeckCommandKind> TranslateShortcut(const KeyChord& chord);

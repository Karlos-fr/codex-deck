// ============================================================================
// Codex Deck - Compatibilite de localisation des menus
// ----------------------------------------------------------------------------
// Ce module conserve l'API minimale attendue par les composants owner-drawn
// issus de Codex Glass. Les textes de Deck sont fournis directement en UTF-16.
// ============================================================================

#pragma once

#include <string>

// Langue d'interface conservee pour la compatibilite des composants de menu.
enum class UiLanguage {
    French,
    English,
};

// ----------------------------------------------------------------------------
// Retourne un texte deja localise sans transformation supplementaire.
//
// Parametres :
// - text : texte UTF-16 courant.
// - previous_language : langue precedente, sans effet dans Codex Deck.
//
// Retour :
// - copie du texte fourni.
// ----------------------------------------------------------------------------
inline std::wstring RelocalizeText(const std::wstring& text, UiLanguage previous_language) {
    (void)previous_language;
    return text;
}

// ============================================================================
// Codex Deck - Etat de defilement
// ----------------------------------------------------------------------------
// Ce module stocke un decalage de scroll borne pour les listes a geometrie fixe,
// sans dependance au rendu ou a Win32.
// ============================================================================

#pragma once

#include <cstddef>

// ----------------------------------------------------------------------------
// Etat de defilement d'un axe.
// ----------------------------------------------------------------------------
struct ScrollState {
    // Decalage courant en DIPs.
    float offset = 0.0F;

    // Taille totale du contenu en DIPs.
    float content_extent = 0.0F;

    // Taille visible du viewport en DIPs.
    float viewport_extent = 0.0F;

    // ------------------------------------------------------------------------
    // Borne le decalage courant.
    // ------------------------------------------------------------------------
    void Clamp();

    // ------------------------------------------------------------------------
    // Applique un delta de scroll puis borne le resultat.
    //
    // Parametres :
    // - delta : variation en DIPs.
    // ------------------------------------------------------------------------
    void ScrollBy(float delta);

    // ------------------------------------------------------------------------
    // Rend une ligne visible si elle est hors viewport.
    //
    // Parametres :
    // - index : ligne logique.
    // - row_height : hauteur de ligne en DIPs.
    // ------------------------------------------------------------------------
    void EnsureVisible(std::size_t index, float row_height);
};

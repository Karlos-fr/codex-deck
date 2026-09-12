// ============================================================================
// Codex Deck - Geometrie de liste virtualisee
// ----------------------------------------------------------------------------
// Ce module calcule les lignes visibles d'une liste a hauteur fixe sans
// connaitre le rendu ni iterer sur le volume total.
// ============================================================================

#pragma once

#include <cstddef>

// ----------------------------------------------------------------------------
// Plage semi-ouverte de lignes visibles.
// ----------------------------------------------------------------------------
struct VisibleRange {
    // Premiere ligne logique a rendre.
    std::size_t first = 0;

    // Ligne suivant la derniere ligne logique a rendre.
    std::size_t last = 0;
};

// ----------------------------------------------------------------------------
// Calcule la plage visible d'une liste a hauteur fixe.
//
// Parametres :
// - item_count : nombre total de lignes.
// - row_height : hauteur d'une ligne en DIPs.
// - scroll_offset : decalage vertical en DIPs.
// - viewport_height : hauteur visible en DIPs.
// - overscan_rows : marge de lignes autour du viewport.
//
// Retour :
// - plage semi-ouverte bornee au nombre de lignes.
// ----------------------------------------------------------------------------
VisibleRange ComputeVisibleRange(
    std::size_t item_count,
    float row_height,
    float scroll_offset,
    float viewport_height,
    std::size_t overscan_rows
);

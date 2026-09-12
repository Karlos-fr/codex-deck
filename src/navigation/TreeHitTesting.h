// ============================================================================
// Codex Deck - Hit testing du Tree
// ----------------------------------------------------------------------------
// Ce module traduit une position pointeur en index logique de ligne sans
// connaitre le contenu des lignes ni declencher de rendu.
// ============================================================================

#pragma once

#include "../ui/ScrollState.h"

#include <d2d1.h>

#include <optional>

// ----------------------------------------------------------------------------
// Traduit un point en ligne logique du Tree.
//
// Parametres :
// - point : position pointeur en DIPs client.
// - rect : rectangle du Tree.
// - scroll : etat de defilement courant.
// - row_height : hauteur fixe d'une ligne.
// - row_count : nombre total de lignes.
//
// Retour :
// - index logique touche, ou nullopt hors zone.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestTreeRow(
    D2D1_POINT_2F point,
    const D2D1_RECT_F& rect,
    const ScrollState& scroll,
    float row_height,
    std::size_t row_count
);

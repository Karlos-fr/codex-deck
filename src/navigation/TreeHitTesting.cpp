// ============================================================================
// Codex Deck - Implementation du hit testing du Tree
// ----------------------------------------------------------------------------
// Ce fichier applique la geometrie fixe du Tree pour convertir une coordonnee
// ecran en ligne logique bornee.
// ============================================================================

#include "TreeHitTesting.h"

#include <cmath>

// ----------------------------------------------------------------------------
// Traduit un point en ligne logique du Tree.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestTreeRow(
    D2D1_POINT_2F point,
    const D2D1_RECT_F& rect,
    const ScrollState& scroll,
    float row_height,
    std::size_t row_count
) {
    if (row_height <= 0.0F || row_count == 0) {
        return std::nullopt;
    }
    if (point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom) {
        return std::nullopt;
    }

    const float logical_y = point.y - rect.top + scroll.offset;
    if (logical_y < 0.0F) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(std::floor(logical_y / row_height));
    if (index >= row_count) {
        return std::nullopt;
    }
    return index;
}

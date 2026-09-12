// ============================================================================
// Codex Deck - Implementation de geometrie virtualisee
// ----------------------------------------------------------------------------
// Ce fichier applique uniquement des calculs arithmetiques bornes pour produire
// la plage visible d'une liste a hauteur fixe.
// ============================================================================

#include "VirtualListLayout.h"

#include <algorithm>
#include <cmath>

// ----------------------------------------------------------------------------
// Calcule la plage visible d'une liste a hauteur fixe.
// ----------------------------------------------------------------------------
VisibleRange ComputeVisibleRange(
    std::size_t item_count,
    float row_height,
    float scroll_offset,
    float viewport_height,
    std::size_t overscan_rows
) {
    if (item_count == 0 || row_height <= 0.0F || viewport_height <= 0.0F) {
        return {};
    }

    const float safe_offset = std::max(0.0F, scroll_offset);
    const auto first_visible = static_cast<std::size_t>(std::floor(safe_offset / row_height));
    const auto visible_count = static_cast<std::size_t>(std::ceil(viewport_height / row_height));
    const std::size_t first = first_visible > overscan_rows ? first_visible - overscan_rows : 0;
    const std::size_t last = std::min(item_count, first_visible + visible_count + overscan_rows);
    return VisibleRange{first, std::max(first, last)};
}

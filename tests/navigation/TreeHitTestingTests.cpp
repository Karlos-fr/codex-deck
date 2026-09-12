// ============================================================================
// Codex Deck - Tests du hit testing du Tree
// ----------------------------------------------------------------------------
// Ce fichier valide la traduction pointeur vers ligne logique sans rendu.
// ============================================================================

#include "navigation/TreeHitTesting.h"

// ----------------------------------------------------------------------------
// Verifie la traduction d'un point vers une ligne virtualisee.
//
// Retour :
// - zero si les points dedans/dehors sont correctement geres.
// ----------------------------------------------------------------------------
int main() {
    const D2D1_RECT_F tree_rect = D2D1::RectF(0.0F, 0.0F, 300.0F, 600.0F);
    ScrollState scroll{};
    scroll.offset = 60.0F;
    scroll.content_extent = 3000.0F;
    scroll.viewport_extent = 600.0F;

    const auto row = HitTestTreeRow(D2D1::Point2F(10.0F, 15.0F), tree_rect, scroll, 30.0F, 100);
    if (!row || *row != 2) {
        return 1;
    }

    const auto outside = HitTestTreeRow(D2D1::Point2F(310.0F, 15.0F), tree_rect, scroll, 30.0F, 100);
    if (outside) {
        return 2;
    }

    const auto beyond_count = HitTestTreeRow(D2D1::Point2F(10.0F, 590.0F), tree_rect, scroll, 30.0F, 4);
    return beyond_count ? 3 : 0;
}

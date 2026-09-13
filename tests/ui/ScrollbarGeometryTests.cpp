// ============================================================================
// Codex Deck - Tests de geometrie de scrollbar
// ----------------------------------------------------------------------------
// Ce fichier valide le calcul pur de scrollbar utilise par la TreeView, sans
// Direct2D ni messages Win32.
// ============================================================================

#include "ui/ScrollbarGeometry.h"

#include <cmath>

namespace {

// ----------------------------------------------------------------------------
// Compare deux flottants avec tolerance.
//
// Parametres :
// - left : valeur gauche.
// - right : valeur droite.
//
// Retour :
// - true si les valeurs sont proches.
// ----------------------------------------------------------------------------
bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 0.01F;
}

}  // namespace

// ----------------------------------------------------------------------------
// Execute les validations de scrollbar.
//
// Retour :
// - zero si le pouce et le drag produisent les offsets attendus.
// ----------------------------------------------------------------------------
int main() {
    ScrollbarInput input{};
    input.top = 100.0F;
    input.bottom = 500.0F;
    input.right = 500.0F;
    input.content_extent = 2000.0F;
    input.viewport_extent = 300.0F;
    input.scroll_offset = 350.0F;
    input.width = 8.0F;
    input.minimum_thumb_height = 32.0F;
    const ScrollbarMetrics metrics = ComputeVerticalScrollbar(input);
    if (!metrics.visible || !NearlyEqual(metrics.thumb_top, 175.2F)) {
        return 1;
    }
    if (!HitTestVerticalScrollbar(metrics, 489.0F, 180.0F)) {
        return 2;
    }
    if (HitTestVerticalScrollbar(metrics, 460.0F, 180.0F)) {
        return 3;
    }
    if (!HitTestVerticalScrollbarTrack(metrics, 489.0F, 330.0F)) {
        return 4;
    }
    if (HitTestVerticalScrollbar(metrics, 489.0F, 330.0F)) {
        return 5;
    }

    const float dragged = ScrollOffsetFromThumbTop(metrics, metrics.thumb_top + 50.0F);
    return dragged > 590.0F && dragged < 630.0F ? 0 : 6;
}

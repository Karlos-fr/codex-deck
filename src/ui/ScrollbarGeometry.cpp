// ============================================================================
// Codex Deck - Implementation de geometrie de scrollbar
// ----------------------------------------------------------------------------
// Ce fichier garde le calcul de scrollbar deterministe et testable pour les
// vues virtualisees.
// ============================================================================

#include "ScrollbarGeometry.h"

#include <algorithm>

namespace {

// Marge verticale interne de piste.
constexpr float kTrackInset = 8.0F;

}  // namespace

// ----------------------------------------------------------------------------
// Calcule la geometrie d'une scrollbar verticale.
// ----------------------------------------------------------------------------
ScrollbarMetrics ComputeVerticalScrollbar(const ScrollbarInput& input) {
    ScrollbarMetrics metrics{};
    metrics.content_extent = std::max(0.0F, input.content_extent);
    metrics.viewport_extent = std::max(0.0F, input.viewport_extent);
    metrics.left = input.right - input.width - 4.0F;
    metrics.right = input.right - 4.0F;
    metrics.track_top = input.top + kTrackInset;
    metrics.track_bottom = input.bottom - kTrackInset;

    const float track_height = std::max(0.0F, metrics.track_bottom - metrics.track_top);
    if (metrics.content_extent <= metrics.viewport_extent || metrics.viewport_extent <= 0.0F || track_height <= 0.0F) {
        return metrics;
    }

    metrics.visible = true;
    const float thumb_height = std::clamp(
        track_height * (metrics.viewport_extent / metrics.content_extent),
        input.minimum_thumb_height,
        track_height
    );
    const float max_offset = std::max(1.0F, metrics.content_extent - metrics.viewport_extent);
    const float travel = std::max(0.0F, track_height - thumb_height);
    const float ratio = std::clamp(input.scroll_offset / max_offset, 0.0F, 1.0F);
    metrics.thumb_top = metrics.track_top + travel * ratio;
    metrics.thumb_bottom = metrics.thumb_top + thumb_height;
    return metrics;
}

// ----------------------------------------------------------------------------
// Indique si un point touche le pouce de scrollbar.
// ----------------------------------------------------------------------------
bool HitTestVerticalScrollbar(const ScrollbarMetrics& metrics, float x, float y) {
    return metrics.visible
        && x >= metrics.left
        && x <= metrics.right
        && y >= metrics.thumb_top
        && y <= metrics.thumb_bottom;
}

// ----------------------------------------------------------------------------
// Indique si un point touche la piste interactive de scrollbar.
// ----------------------------------------------------------------------------
bool HitTestVerticalScrollbarTrack(const ScrollbarMetrics& metrics, float x, float y) {
    return metrics.visible
        && x >= metrics.left
        && x <= metrics.right
        && y >= metrics.track_top
        && y <= metrics.track_bottom;
}

// ----------------------------------------------------------------------------
// Convertit un bord haut de pouce en offset de contenu.
// ----------------------------------------------------------------------------
float ScrollOffsetFromThumbTop(const ScrollbarMetrics& metrics, float thumb_top) {
    if (!metrics.visible) {
        return 0.0F;
    }
    const float thumb_height = metrics.thumb_bottom - metrics.thumb_top;
    const float travel = std::max(1.0F, metrics.track_bottom - metrics.track_top - thumb_height);
    const float max_offset = std::max(0.0F, metrics.content_extent - metrics.viewport_extent);
    const float clamped_top = std::clamp(thumb_top, metrics.track_top, metrics.track_top + travel);
    return ((clamped_top - metrics.track_top) / travel) * max_offset;
}

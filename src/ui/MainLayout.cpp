// ============================================================================
// Codex Deck - Implementation du layout principal
// ----------------------------------------------------------------------------
// Ce fichier garde une geometrie deterministe et testable pour la composition
// Tree + Workbench.
// ============================================================================

#include "MainLayout.h"

#include <algorithm>
#include <cmath>

namespace {

// Largeur minimale du Tree.
constexpr float kMinimumTreeWidth = 220.0F;

// Largeur maximale du Tree.
constexpr float kMaximumTreeWidth = 480.0F;

// Largeur visuelle du separateur.
constexpr float kSplitterWidth = 1.0F;

// Largeur de hit du separateur.
constexpr float kSplitterHitWidth = 8.0F;

}  // namespace

// ----------------------------------------------------------------------------
// Borne une largeur de Tree aux limites V1.
// ----------------------------------------------------------------------------
float ClampTreeWidth(float tree_width) {
    return std::clamp(tree_width, kMinimumTreeWidth, kMaximumTreeWidth);
}

// ----------------------------------------------------------------------------
// Calcule le layout principal.
// ----------------------------------------------------------------------------
MainLayoutRects ComputeMainLayout(
    SizeF client,
    float tree_width,
    float activity_height,
    float composer_height
) {
    const float width = std::max(0.0F, client.width);
    const float height = std::max(0.0F, client.height);
    const float tree = std::min(ClampTreeWidth(tree_width), width);
    const float activity = std::clamp(activity_height, 0.0F, height);
    const float composer = std::clamp(composer_height, 0.0F, std::max(0.0F, height - activity));

    MainLayoutRects rects{};
    rects.activity_bar = LayoutRect{0.0F, 0.0F, width, activity};
    rects.tree = LayoutRect{0.0F, activity, tree, height};
    rects.workbench = LayoutRect{tree, activity, width, height - composer};
    rects.composer = LayoutRect{tree, height - composer, width, height};
    rects.splitter = LayoutRect{
        std::max(0.0F, tree - kSplitterWidth * 0.5F),
        activity,
        std::min(width, tree + kSplitterWidth * 0.5F),
        height
    };
    return rects;
}

// ----------------------------------------------------------------------------
// Indique si une position horizontale touche le separateur principal.
// ----------------------------------------------------------------------------
bool HitTestMainSplitter(const MainLayoutRects& rects, float x) {
    const float center = (rects.splitter.left + rects.splitter.right) * 0.5F;
    return std::abs(x - center) <= kSplitterHitWidth * 0.5F;
}

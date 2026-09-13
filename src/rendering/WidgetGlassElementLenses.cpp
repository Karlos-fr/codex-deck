// ============================================================================
// Codex Glass - Implementation des lentilles des composants graphiques
// ----------------------------------------------------------------------------
// Ce module convertit les rectangles UI dynamiques en lentilles GlassEffect.
// Le pipeline de distorsion reste responsable du traitement CPU ou GPU.
// ============================================================================

#include "WidgetGlassElementLenses.h"

#include <algorithm>
#include <cmath>

// ----------------------------------------------------------------------------
// Ajoute une lentille dynamique autour d'un composant graphique.
// ----------------------------------------------------------------------------
void AppendWidgetGlassElementLens(
    std::vector<WidgetGlassEffectLens>& lenses,
    const D2D1_RECT_F& bounds,
    const GlassEffectRenderProfile& profile,
    float radius
) {
    const float width = bounds.right - bounds.left;
    const float height = bounds.bottom - bounds.top;
    if (lenses.size() >= kWidgetGlassElementLensLimit
        || width <= 2.0F
        || height <= 2.0F
        || profile.progress_lens_band <= 0.0F
        || std::fabs(profile.progress_lens_strength) <= 0.0001F) {
        return;
    }

    lenses.push_back(WidgetGlassEffectLens{
        bounds.left,
        bounds.top,
        bounds.right,
        bounds.bottom,
        std::clamp(radius, 0.0F, std::min(width, height) * 0.5F),
        profile.progress_lens_band,
        profile.progress_lens_strength,
        profile.element_lens_softness,
    });
}


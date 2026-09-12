// ============================================================================
// Codex Glass - Rendu lumineux de Pulse
// ----------------------------------------------------------------------------
// Ce fichier declare le halo Pulse des Effets Motion. Il consomme uniquement
// un etat temporel deja calcule et ne gere ni timeline, ni reglages persistants.
// ============================================================================

#pragma once

#include "../vibration/WidgetVibrationGlassEffect.h"

#include <d2d1.h>

// ----------------------------------------------------------------------------
// Dessine le halo Pulse autour du panneau et sa propagation interieure.
//
// Parametres :
// - render_target : cible Direct2D recevant les passes lumineuses.
// - panel_rect : contour logique du panneau a illuminer.
// - accent_color : couleur active du theme.
// - state : intensite et progression Pulse de la frame courante.
// ----------------------------------------------------------------------------
void DrawWidgetMotionPulse(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& panel_rect,
    D2D1_COLOR_F accent_color,
    const WidgetVibrationGlassEffectState& state
);

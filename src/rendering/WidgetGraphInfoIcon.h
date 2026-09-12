// ============================================================================
// Codex Glass - Icone d'information des vues graphiques
// ----------------------------------------------------------------------------
// Ce fichier declare le pictogramme vectoriel partage par les quatre onglets.
// Sa geometrie reste independante des metriques typographiques DirectWrite.
// ============================================================================

#pragma once

#include <d2d1.h>

// Rayon commun du cercle d'information en DIPs.
constexpr float kWidgetGraphInfoRadius = 5.0F;

// ----------------------------------------------------------------------------
// Dessine un i vectoriel centre dans sa bulle circulaire.
//
// Parametres :
// - render_target : cible Direct2D recevant le pictogramme.
// - brush : brosse appliquee au cercle, au point et a la barre.
// - bounds : zone partagee avec le hit-test du bouton d'information.
// ----------------------------------------------------------------------------
void DrawWidgetGraphInfoIcon(
    ID2D1RenderTarget* render_target,
    ID2D1Brush* brush,
    const D2D1_RECT_F& bounds
);

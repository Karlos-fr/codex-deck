// ============================================================================
// Codex Glass - Composition Direct2D de Wave
// ----------------------------------------------------------------------------
// Ce module recompose le rendu final du widget par bandes de texture afin de
// produire une vague globale. Il ne gere ni timeline ni capture GlassEffect.
// ============================================================================

#pragma once

#include "../vibration/WidgetVibrationGlassEffect.h"

#include <d2d1.h>

// ----------------------------------------------------------------------------
// Calcule le deplacement horizontal d'une bande pour l'etat Wave courant.
//
// Parametres :
// - coordinate : position horizontale de la bande en DIPs.
// - extent : largeur totale du widget en DIPs.
// - wave : etat transitoire du moteur Motion.
//
// Retour :
// - deplacement horizontal signe en DIPs.
// ----------------------------------------------------------------------------
float EvaluateWidgetMotionWaveStripOffset(
    float coordinate,
    float extent,
    const WidgetVibrationGlassEffectState& wave
);

// ----------------------------------------------------------------------------
// Recompose un bitmap complet avec la deformation Wave courante.
//
// Parametres :
// - target : cible Direct2D finale associee a la fenetre.
// - source : bitmap hors ecran contenant tout le widget.
// - size : taille de la cible en DIPs.
// - wave : etat transitoire du moteur Motion.
// ----------------------------------------------------------------------------
void DrawWidgetMotionWaveComposition(
    ID2D1RenderTarget* target,
    ID2D1Bitmap* source,
    D2D1_SIZE_F size,
    const WidgetVibrationGlassEffectState& wave
);

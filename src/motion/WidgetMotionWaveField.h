// ============================================================================
// Codex Glass - Champ de deformation Wave
// ----------------------------------------------------------------------------
// Ce fichier declare le prototype mathematique pur d'une vague d'eau. Le
// renderer adapte ensuite son resultat aux pixels de la capture GlassEffect.
// ============================================================================

#pragma once

#include "WidgetMotionEffectsSettings.h"

// Regroupe le decalage de sampling calcule pour un point.
struct WidgetMotionWaveSample {
    // Decalage horizontal en pixels.
    float offset_x = 0.0F;

    // Decalage vertical en pixels.
    float offset_y = 0.0F;
};

// Calcule le champ Wave pour un point, une taille et une progression donnes.
// Parametres : position, dimensions, progression et reglages Wave.
// Retour : decalage de sampling bidimensionnel en pixels.
WidgetMotionWaveSample EvaluateWidgetMotionWaveField(
    float x,
    float y,
    float width,
    float height,
    float progress,
    const WidgetMotionWaveSettings& settings
);

// ============================================================================
// Codex Glass - Lentilles Glass des composants graphiques
// ----------------------------------------------------------------------------
// Ce module construit les zones de refraction associees aux composants UI.
// Il ne calcule pas leur layout et ne dessine aucune ressource Direct2D.
// ============================================================================

#pragma once

#include "WidgetRenderTypes.h"
#include "../glass/WidgetGlassEffectDistortion.h"

#include <d2d1.h>

#include <cstddef>
#include <vector>

// Nombre maximal de composants gardes par frame pour contenir le cout GPU.
constexpr size_t kWidgetGlassElementLensLimit = 64U;

// ----------------------------------------------------------------------------
// Ajoute une lentille dynamique autour d'un composant graphique.
//
// Parametres :
// - lenses : collection de la frame courante a completer.
// - bounds : rectangle reel du composant en DIPs.
// - profile : profil portant la largeur et la force choisies par l'utilisateur.
// - radius : rayon visuel du composant en DIPs.
//
// Effet de bord :
// - ignore les zones vides, les effets nuls et les composants excedentaires.
// ----------------------------------------------------------------------------
void AppendWidgetGlassElementLens(
    std::vector<WidgetGlassEffectLens>& lenses,
    const D2D1_RECT_F& bounds,
    const GlassEffectRenderProfile& profile,
    float radius
);

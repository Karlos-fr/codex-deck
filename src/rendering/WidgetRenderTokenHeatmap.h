// ============================================================================
// Codex Glass - Rendu de la heatmap quotidienne de tokens
// ----------------------------------------------------------------------------
// Ce fichier declare la vue Activite. Il consomme les agregats quotidiens et
// la geometrie partagee sans acceder au scanner ni a SQLite.
// ============================================================================

#pragma once

#include "WidgetGraphInteraction.h"
#include "WidgetRenderGraph.h"
#include "../tokens/TokenUsageTypes.h"

// ----------------------------------------------------------------------------
// Dessine une rangee compacte des dernieres tranches d'activite Tokens.
//
// Parametres :
// - context : ressources et couleurs partagees avec la vue Activite.
// - snapshot : agregats locaux contenant les tranches de cinq minutes.
// - rect : rectangle reserve a la rangee de cellules.
// ----------------------------------------------------------------------------
void DrawCompactTokenActivityStrip(
    const WidgetRenderGraphContext& context,
    const TokenUsageSnapshot& snapshot,
    const D2D1_RECT_F& rect
);

// ----------------------------------------------------------------------------
// Dessine la heatmap quotidienne responsive et ses interactions.
//
// Parametres :
// - context : ressources Direct2D et DirectWrite partagees.
// - layout : geometrie calculee pour la taille courante.
// - snapshot : agregats quotidiens locaux.
// - interaction : cellule et bulle d'information survolees.
// - granularity : jours historiques ou tranches de cinq minutes.
// ----------------------------------------------------------------------------
void DrawTokenUsageHeatmap(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenUsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange granularity
);

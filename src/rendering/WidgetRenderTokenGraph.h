// ============================================================================
// Codex Glass - Rendu du graphe horaire de tokens
// ----------------------------------------------------------------------------
// Ce fichier declare les barres de tokens, les onglets et l'infobulle. Il
// choisit une granularite de cinq minutes ou horaire dans un snapshot agrege.
// ============================================================================

#pragma once

#include "WidgetGraphInteraction.h"
#include "WidgetRenderGraph.h"
#include "../tokens/TokenUsageTypes.h"

// ----------------------------------------------------------------------------
// Dessine les barres de la plage selectionnee et l'infobulle de survol.
// ----------------------------------------------------------------------------
void DrawTokenUsageGraph(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenUsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange range
);

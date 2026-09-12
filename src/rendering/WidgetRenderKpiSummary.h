// ============================================================================
// Codex Glass - Rendu du Bilan de consommation
// ----------------------------------------------------------------------------
// Ce fichier declare le rendu Direct2D des quatre cartes KPI. Les calculs des
// agregats restent confines au module TokenKpiSummary.
// ============================================================================

#pragma once

#include "WidgetGraphInteraction.h"
#include "WidgetRenderGraph.h"
#include "../tokens/TokenUsageTypes.h"

#include <cstdint>
#include <vector>

// ----------------------------------------------------------------------------
// Dessine la courbe compacte de tokens partagee par le Bilan et les modes
// d'affichage orientes.
//
// Parametres :
// - context : ressources Direct2D partagees.
// - values : valeurs chronologiques a normaliser dans la zone disponible.
// - rect : rectangle de destination de la courbe.
// ----------------------------------------------------------------------------
void DrawWidgetTokenSparkline(
    const WidgetRenderGraphContext& context,
    const std::vector<std::uint64_t>& values,
    const D2D1_RECT_F& rect
);

// ----------------------------------------------------------------------------
// Dessine le Bilan de consommation pour la plage selectionnee.
//
// Parametres :
// - context : ressources Direct2D et DirectWrite partagees.
// - layout : geometrie des cartes et de l'en-tete.
// - snapshot : agregats locaux reconstruits depuis les sessions Codex.
// - interaction : etat de survol du hint de la page.
// - range : plage temporelle du Bilan.
// ----------------------------------------------------------------------------
void DrawWidgetKpiSummary(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenUsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange range
);

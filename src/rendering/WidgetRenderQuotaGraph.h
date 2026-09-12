// ============================================================================
// Codex Glass - Rendu du graphe de quotas
// ----------------------------------------------------------------------------
// Ce fichier declare la courbe du quota hebdomadaire restant sur la plage choisie.
// Il consomme l'historique SQLite sans modifier sa persistance.
// ============================================================================

#pragma once

#include "WidgetRenderGraph.h"
#include "WidgetGraphFormatting.h"
#include "WidgetGraphInteraction.h"
#include "WidgetGraphLayout.h"
#include "../animation/WidgetQuotaGraphAnimation.h"
#include "../usage/UsageHistoryStore.h"
#include "../usage/UsageSnapshot.h"

#include <string>

// ----------------------------------------------------------------------------
// Dessine la page Quotas sur trente jours et ses interactions.
//
// Parametres :
// - context : ressources Direct2D et DirectWrite du graphe.
// - layout : geometrie partagee avec la vue Tokens.
// - history_store : historique SQLite a lire sans le modifier.
// - snapshot : dernier etat du provider utilise pour distinguer les etats UI.
// - interaction : position survolee et etat de la bulle d'information.
// - graph_range : plage temporelle choisie dans le menu.
// - animation : gel ou translation applique a la serie courante.
// ----------------------------------------------------------------------------
void DrawQuotaGraph(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const UsageHistoryStore& history_store,
    const UsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange graph_range,
    const WidgetQuotaGraphAnimationFrame& animation
);

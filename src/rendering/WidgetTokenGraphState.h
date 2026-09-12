// ============================================================================
// Codex Glass - Etats de contenu des graphes de tokens
// ----------------------------------------------------------------------------
// Ce fichier centralise la selection des etats UI des vues horaire et heatmap.
// Il ne dessine rien et permet de conserver un historique pendant un refresh.
// ============================================================================

#pragma once

#include "../tokens/TokenUsageTypes.h"
#include "../settings/AppSettings.h"

// ----------------------------------------------------------------------------
// Liste les contenus mutuellement exclusifs affiches par un graphe de tokens.
// ----------------------------------------------------------------------------
enum class WidgetTokenGraphState {
    Data,
    Loading,
    NoSessions,
    Building,
    Error,
};

// ----------------------------------------------------------------------------
// Resout l'etat de la vue horaire a partir du snapshot courant.
//
// Parametres :
// - snapshot : donnees locales et fraicheur du dernier scan.
// - range : plage determinant la serie de cinq minutes ou horaire.
//
// Retour :
// - etat de contenu, les donnees non nulles restant prioritaires pendant un
//   refresh ou une erreur temporaire.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveHourlyTokenGraphState(
    const TokenUsageSnapshot& snapshot,
    GraphRange range
);

// ----------------------------------------------------------------------------
// Resout l'etat de la heatmap d'activite a partir du snapshot courant.
//
// Parametres :
// - snapshot : donnees locales et fraicheur du dernier scan.
// - granularity : serie quotidienne ou par tranches de cinq minutes.
//
// Retour :
// - etat de contenu, les donnees non nulles restant prioritaires pendant un
//   refresh ou une erreur temporaire.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveTokenHeatmapState(
    const TokenUsageSnapshot& snapshot,
    GraphRange granularity
);

// ----------------------------------------------------------------------------
// Resout l'etat de la heatmap quotidienne avec la granularite historique.
//
// Parametres :
// - snapshot : donnees locales et fraicheur du dernier scan.
//
// Retour :
// - etat de contenu de la heatmap quotidienne.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveTokenHeatmapState(const TokenUsageSnapshot& snapshot);

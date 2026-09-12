// ============================================================================
// Codex Glass - Etats de contenu des graphes de tokens
// ----------------------------------------------------------------------------
// Ce fichier applique une politique commune aux deux granularites locales afin
// qu'un refresh ne remplace jamais un graphe utile par un ecran de chargement.
// ============================================================================

#include "WidgetTokenGraphState.h"

#include <algorithm>

namespace {

// ----------------------------------------------------------------------------
// Resout un etat commun une fois connue la presence de donnees non nulles.
//
// Parametres :
// - snapshot : etat de collecte courant.
// - has_data : indique qu'au moins un agregat contient des tokens.
//
// Retour :
// - etat d'affichage stable du graphe.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveState(
    const TokenUsageSnapshot& snapshot,
    bool has_data
) {
    if (has_data) {
        return WidgetTokenGraphState::Data;
    }
    if (snapshot.freshness == TokenUsageFreshness::Loading) {
        return WidgetTokenGraphState::Loading;
    }
    if (snapshot.freshness == TokenUsageFreshness::Error) {
        return WidgetTokenGraphState::Error;
    }
    if (snapshot.freshness == TokenUsageFreshness::Empty || !snapshot.available) {
        return WidgetTokenGraphState::NoSessions;
    }
    return WidgetTokenGraphState::Building;
}

} // namespace

// ----------------------------------------------------------------------------
// Resout l'etat de la vue horaire a partir du snapshot courant.
//
// Parametres :
// - snapshot : donnees horaires et fraicheur du scan.
//
// Retour :
// - etat de contenu a dessiner.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveHourlyTokenGraphState(
    const TokenUsageSnapshot& snapshot,
    GraphRange range
) {
    if (range == GraphRange::Hours1) {
        return ResolveState(snapshot, std::any_of(
            snapshot.five_minute.begin(),
            snapshot.five_minute.end(),
            [](const TokenFiveMinuteUsage& bucket) {
                return bucket.counts.Total() > 0;
            }
        ));
    }
    return ResolveState(snapshot, std::any_of(
        snapshot.hourly.begin(),
        snapshot.hourly.end(),
        [](const TokenHourlyUsage& hour) {
            return hour.counts.Total() > 0;
        }
    ));
}

// ----------------------------------------------------------------------------
// Resout l'etat de la heatmap d'activite a partir du snapshot courant.
//
// Parametres :
// - snapshot : donnees locales et fraicheur du scan.
// - granularity : serie quotidienne ou par tranches de cinq minutes.
//
// Retour :
// - etat de contenu a dessiner.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveTokenHeatmapState(
    const TokenUsageSnapshot& snapshot,
    GraphRange granularity
) {
    if (granularity == GraphRange::Minutes5) {
        return ResolveState(snapshot, std::any_of(
            snapshot.five_minute.begin(),
            snapshot.five_minute.end(),
            [](const TokenFiveMinuteUsage& bucket) {
                return bucket.counts.Total() > 0;
            }
        ));
    }
    return ResolveState(snapshot, std::any_of(
        snapshot.daily.begin(),
        snapshot.daily.end(),
        [](const TokenDailyUsage& day) {
            return day.counts.Total() > 0;
        }
    ));
}

// ----------------------------------------------------------------------------
// Resout l'etat de la heatmap quotidienne avec la granularite historique.
//
// Parametres :
// - snapshot : donnees locales et fraicheur du scan.
//
// Retour :
// - etat de contenu a dessiner.
// ----------------------------------------------------------------------------
WidgetTokenGraphState ResolveTokenHeatmapState(const TokenUsageSnapshot& snapshot) {
    return ResolveTokenHeatmapState(snapshot, GraphRange::Days30);
}

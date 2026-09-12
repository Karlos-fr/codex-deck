// ============================================================================
// Codex Glass - Donnees calculees des graphes
// ----------------------------------------------------------------------------
// Ce fichier declare les calculs purs partages par les rendus Quotas et Tokens.
// Il ne dessine rien et ne modifie ni SQLite ni les snapshots sources.
// ============================================================================

#pragma once

#include "../usage/UsageHistoryStore.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// ----------------------------------------------------------------------------
// Represente une valeur de quota hebdomadaire restant dans la serie graphique.
// ----------------------------------------------------------------------------
struct QuotaGraphSample {
    // Instant du releve conserve pour le positionnement et le tooltip.
    std::chrono::system_clock::time_point sampled_at{};

    // Pourcentage restant, absent lorsque Codex n'a pas fourni la valeur.
    std::optional<double> remaining_percent;

    // Date de reset associee au releve lorsqu'elle est disponible.
    std::optional<std::chrono::system_clock::time_point> reset_at;
};

// ----------------------------------------------------------------------------
// Calcule le maximum et le pas lisible de l'axe vertical des tokens.
//
// Parametres :
// - maximum : plus grande consommation observee dans la plage visible.
//
// Retour :
// - maximum de l'axe et pas lisible adapte a l'ordre de grandeur.
// ----------------------------------------------------------------------------
std::pair<std::uint64_t, std::uint64_t> CalculateTokenAxisScale(std::uint64_t maximum);

// ----------------------------------------------------------------------------
// Selectionne les buckets dont le libelle doit apparaitre sur l'axe Tokens.
//
// Parametres :
// - count : nombre total de buckets visibles.
// - maximum_label_count : densite maximale souhaitee avant espacement.
//
// Retour :
// - indices ordonnes, dernier bucket inclus sans voisin trop rapproche.
// ----------------------------------------------------------------------------
std::vector<std::size_t> BuildTokenAxisLabelIndices(
    std::size_t count,
    std::size_t maximum_label_count = 6
);

// ----------------------------------------------------------------------------
// Construit la serie Quotas ordonnee en conservant les valeurs absentes.
//
// Parametres :
// - samples : releves historiques a convertir.
//
// Retour :
// - serie chronologique de pourcentages hebdomadaires restants.
// ----------------------------------------------------------------------------
std::vector<QuotaGraphSample> BuildQuotaGraphSeries(
    const std::vector<UsageHistorySample>& samples
);

// ============================================================================
// Codex Glass - Synthese KPI des tokens
// ----------------------------------------------------------------------------
// Ce fichier cumule les buckets de cinq minutes, horaires ou quotidiens selon
// la plage demandee. Le formatage et le rendu restent dans la couche graphique.
// ============================================================================

#include "TokenKpiSummary.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace {

// ----------------------------------------------------------------------------
// Additionne deux compteurs non signes en saturant a leur maximum.
//
// Parametres :
// - left : valeur deja cumulee.
// - right : valeur a ajouter.
//
// Retour :
// - somme exacte ou maximum representable en cas de depassement.
// ----------------------------------------------------------------------------
std::uint64_t SaturatingAdd(std::uint64_t left, std::uint64_t right) {
    return left > std::numeric_limits<std::uint64_t>::max() - right
        ? std::numeric_limits<std::uint64_t>::max()
        : left + right;
}

// ----------------------------------------------------------------------------
// Ajoute les compteurs d'un bucket a un agregat du Bilan.
//
// Parametres :
// - target : agregat modifie sur place.
// - counts : categories de tokens a ajouter.
// - request_count : nombre de requetes a ajouter.
// ----------------------------------------------------------------------------
void AppendAggregate(
    TokenKpiAggregate& target,
    const TokenUsageCounts& counts,
    std::uint64_t request_count
) {
    target.counts.input_tokens = SaturatingAdd(
        target.counts.input_tokens,
        counts.input_tokens
    );
    target.counts.cached_input_tokens = SaturatingAdd(
        target.counts.cached_input_tokens,
        counts.cached_input_tokens
    );
    target.counts.output_tokens = SaturatingAdd(
        target.counts.output_tokens,
        counts.output_tokens
    );
    target.counts.reasoning_output_tokens = SaturatingAdd(
        target.counts.reasoning_output_tokens,
        counts.reasoning_output_tokens
    );
    target.request_count = SaturatingAdd(target.request_count, request_count);
}

// ----------------------------------------------------------------------------
// Construit une synthese depuis une serie chronologique de buckets homogenes.
//
// Parametres :
// - buckets : source ordonnee du plus ancien au plus recent.
// - requested_count : nombre de buckets constituant une periode.
//
// Retour :
// - agregats de la periode courante et de la precedente avec micro-series.
// ----------------------------------------------------------------------------
template <typename TBucket>
TokenKpiSummary BuildSummaryFromBuckets(
    const std::vector<TBucket>& buckets,
    std::size_t requested_count
) {
    TokenKpiSummary summary{};
    const std::size_t current_count = std::min(requested_count, buckets.size());
    const std::size_t current_start = buckets.size() - current_count;
    summary.token_series.reserve(current_count);
    summary.request_series.reserve(current_count);
    for (std::size_t index = current_start; index < buckets.size(); ++index) {
        const TBucket& bucket = buckets[index];
        AppendAggregate(summary.current, bucket.counts, bucket.request_count);
        summary.token_series.push_back(bucket.counts.Total());
        summary.request_series.push_back(bucket.request_count);
    }

    summary.previous_available = current_count == requested_count
        && current_start >= requested_count;
    if (!summary.previous_available) {
        return summary;
    }
    const std::size_t previous_start = current_start - requested_count;
    for (std::size_t index = previous_start; index < current_start; ++index) {
        const TBucket& bucket = buckets[index];
        AppendAggregate(summary.previous, bucket.counts, bucket.request_count);
    }
    return summary;
}

} // namespace

// ----------------------------------------------------------------------------
// Calcule les indicateurs du Bilan pour une plage prise en charge.
// ----------------------------------------------------------------------------
TokenKpiSummary BuildTokenKpiSummary(
    const TokenUsageSnapshot& snapshot,
    GraphRange range
) {
    switch (range) {
    case GraphRange::Hours1:
        return BuildSummaryFromBuckets(snapshot.five_minute, 12U);
    case GraphRange::Hours5:
        return BuildSummaryFromBuckets(snapshot.hourly, 5U);
    case GraphRange::Hours24:
        return BuildSummaryFromBuckets(snapshot.hourly, 24U);
    case GraphRange::Days30:
        return BuildSummaryFromBuckets(snapshot.daily, 30U);
    case GraphRange::Days7:
    default:
        return BuildSummaryFromBuckets(snapshot.daily, 7U);
    }
}

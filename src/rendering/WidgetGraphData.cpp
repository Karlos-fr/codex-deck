// ============================================================================
// Codex Glass - Donnees calculees des graphes
// ----------------------------------------------------------------------------
// Ce fichier calcule les echelles et series testables sans aucune ressource de
// rendu. Les modules Direct2D consomment uniquement les resultats produits ici.
// ============================================================================

#include "WidgetGraphData.h"

#include <algorithm>
#include <limits>

namespace {

// Nombre cible de divisions verticales pour conserver des libelles lisibles.
constexpr std::uint64_t kTokenAxisTargetDivisions = 4ULL;

// Pas minimal utilise lorsqu'aucun token n'est encore visible.
constexpr std::uint64_t kTokenAxisMinimumStep = 100ULL;

// Retourne le premier pas lisible de la serie 1, 2, 5 superieur a la demande.
std::uint64_t NiceTokenAxisStep(std::uint64_t requested) {
    std::uint64_t magnitude = 1ULL;
    while (magnitude <= requested / 10ULL) {
        magnitude *= 10ULL;
    }
    const std::uint64_t normalized = (requested / magnitude)
        + (requested % magnitude == 0 ? 0ULL : 1ULL);
    const std::uint64_t multiplier = normalized <= 1ULL ? 1ULL
        : (normalized <= 2ULL ? 2ULL : (normalized <= 5ULL ? 5ULL : 10ULL));
    return magnitude > std::numeric_limits<std::uint64_t>::max() / multiplier
        ? std::numeric_limits<std::uint64_t>::max()
        : magnitude * multiplier;
}

} // namespace

// ----------------------------------------------------------------------------
// Calcule le maximum et le pas lisible de l'axe vertical des tokens.
// ----------------------------------------------------------------------------
std::pair<std::uint64_t, std::uint64_t> CalculateTokenAxisScale(std::uint64_t maximum) {
    const std::uint64_t requested_step = maximum == 0
        ? kTokenAxisMinimumStep
        : std::max(
            1ULL,
            (maximum / kTokenAxisTargetDivisions)
                + (maximum % kTokenAxisTargetDivisions == 0 ? 0ULL : 1ULL)
        );
    const std::uint64_t tick_step = NiceTokenAxisStep(requested_step);
    const std::uint64_t divisions = maximum == 0
        ? kTokenAxisTargetDivisions
        : std::max(
            1ULL,
            (maximum / tick_step) + (maximum % tick_step == 0 ? 0ULL : 1ULL)
        );
    const std::uint64_t axis_maximum = divisions > std::numeric_limits<std::uint64_t>::max() / tick_step
        ? std::numeric_limits<std::uint64_t>::max()
        : divisions * tick_step;
    return {axis_maximum, tick_step};
}

// ----------------------------------------------------------------------------
// Selectionne des graduations regulieres en reservant la derniere position.
// ----------------------------------------------------------------------------
std::vector<std::size_t> BuildTokenAxisLabelIndices(
    std::size_t count,
    std::size_t maximum_label_count
) {
    if (count == 0 || maximum_label_count == 0) {
        return {};
    }
    const std::size_t label_step = count <= maximum_label_count
        ? 1U
        : ((count + maximum_label_count - 1U) / maximum_label_count);
    std::vector<std::size_t> indices;
    indices.reserve(std::min(count, maximum_label_count + 1U));
    for (std::size_t index = 0; index < count; index += label_step) {
        indices.push_back(index);
    }
    const std::size_t last_index = count - 1U;
    if (indices.back() != last_index) {
        if (indices.size() > 1U && last_index - indices.back() < label_step) {
            indices.pop_back();
        }
        indices.push_back(last_index);
    }
    return indices;
}

// ----------------------------------------------------------------------------
// Construit la serie Quotas ordonnee en conservant les valeurs absentes.
// ----------------------------------------------------------------------------
std::vector<QuotaGraphSample> BuildQuotaGraphSeries(
    const std::vector<UsageHistorySample>& samples
) {
    std::vector<UsageHistorySample> ordered_samples = samples;
    std::stable_sort(
        ordered_samples.begin(),
        ordered_samples.end(),
        [](const UsageHistorySample& left, const UsageHistorySample& right) {
            return left.sampled_at < right.sampled_at;
        }
    );

    std::vector<QuotaGraphSample> series;
    series.reserve(ordered_samples.size());
    for (const UsageHistorySample& sample : ordered_samples) {
        QuotaGraphSample value{};
        value.sampled_at = sample.sampled_at;
        value.reset_at = sample.weekly_reset_at;
        if (sample.weekly_used_percent.has_value()) {
            value.remaining_percent = std::clamp(
                100.0 - *sample.weekly_used_percent,
                0.0,
                100.0
            );
        }
        series.push_back(value);
    }
    return series;
}

// ============================================================================
// Codex Glass - Animation de mise a jour du graphique Quotas
// ----------------------------------------------------------------------------
// Ce fichier implemente le gel des samples et l'interpolation de leur fenetre
// temporelle. La duree reste locale afin de ne pas exposer de detail visuel au
// controleur applicatif.
// ============================================================================

#include "WidgetQuotaGraphAnimation.h"

#include <algorithm>
#include <chrono>

namespace {

// Duree totale de la translation douce apres un rafraichissement reussi.
constexpr std::chrono::milliseconds kQuotaGraphAnimationDuration{420};

// ----------------------------------------------------------------------------
// Calcule une progression cubique qui ralentit progressivement a l'arrivee.
//
// Parametres :
// - progress : progression lineaire entre zero et un.
//
// Retour :
// - progression adoucie entre zero et un.
// ----------------------------------------------------------------------------
float EaseOutCubic(float progress) {
    const float remaining = 1.0F - std::clamp(progress, 0.0F, 1.0F);
    return 1.0F - (remaining * remaining * remaining);
}

} // namespace

// ----------------------------------------------------------------------------
// Fige les samples a la date de debut du rafraichissement.
// ----------------------------------------------------------------------------
void WidgetQuotaGraphAnimation::Hold(
    std::chrono::system_clock::time_point maximum_sampled_at,
    std::chrono::system_clock::time_point range_end
) {
    maximum_sampled_at_ = maximum_sampled_at;
    previous_range_end_ = range_end;
    target_range_end_ = range_end;
    started_at_ = TimePoint{};
    started_ = false;
}

// ----------------------------------------------------------------------------
// Libere la serie et demarre sa translation d'entree.
// ----------------------------------------------------------------------------
void WidgetQuotaGraphAnimation::Start(
    std::chrono::system_clock::time_point range_end,
    TimePoint now
) {
    maximum_sampled_at_.reset();
    target_range_end_ = range_end;
    started_at_ = now;
    started_ = target_range_end_ > previous_range_end_;
}

// ----------------------------------------------------------------------------
// Supprime tout gel ou animation encore actif.
// ----------------------------------------------------------------------------
void WidgetQuotaGraphAnimation::Clear() {
    maximum_sampled_at_.reset();
    previous_range_end_ = std::chrono::system_clock::time_point{};
    target_range_end_ = std::chrono::system_clock::time_point{};
    started_at_ = TimePoint{};
    started_ = false;
}

// ----------------------------------------------------------------------------
// Calcule la frame de transition correspondant a un instant.
// ----------------------------------------------------------------------------
WidgetQuotaGraphAnimationFrame WidgetQuotaGraphAnimation::Frame(TimePoint now) const {
    WidgetQuotaGraphAnimationFrame frame{};
    if (!started_) {
        frame.maximum_sampled_at = maximum_sampled_at_;
        if (maximum_sampled_at_.has_value()) {
            frame.range_end = previous_range_end_;
        }
        return frame;
    }
    if (!IsActive(now)) {
        return frame;
    }

    const float progress = std::clamp(
        std::chrono::duration<float>(now - started_at_).count()
            / std::chrono::duration<float>(kQuotaGraphAnimationDuration).count(),
        0.0F,
        1.0F
    );
    const float eased_progress = EaseOutCubic(progress);
    const double range_seconds = std::chrono::duration<double>(
        target_range_end_ - previous_range_end_
    ).count();
    frame.range_end = previous_range_end_ + std::chrono::duration_cast<
        std::chrono::system_clock::duration
    >(std::chrono::duration<double>{range_seconds * eased_progress});
    frame.active = true;
    return frame;
}

// ----------------------------------------------------------------------------
// Indique si la translation necessite encore des images intermediaires.
// ----------------------------------------------------------------------------
bool WidgetQuotaGraphAnimation::IsActive(TimePoint now) const {
    return started_
        && now >= started_at_
        && now - started_at_ < kQuotaGraphAnimationDuration;
}

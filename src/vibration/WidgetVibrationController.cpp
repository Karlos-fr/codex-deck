// ============================================================================
// Codex Glass - Implementation du controleur de vibration
// ----------------------------------------------------------------------------
// Ce fichier compare les releves d'usage successifs et applique le cooldown
// avant d'autoriser un declenchement de vibration.
// ============================================================================

#include "WidgetVibrationController.h"

#include <algorithm>

namespace {

// ----------------------------------------------------------------------------
// Indique si le releve contient au moins une limite comparable.
//
// Parametres :
// - snapshot : releve d'usage a verifier.
//
// Retour :
// - true si une fenetre standard ou supplementaire est exploitable.
// - false sinon.
// ----------------------------------------------------------------------------
bool HasUsableQuotaData(const UsageSnapshot& snapshot) {
    if (snapshot.freshness != UsageFreshness::Fresh) {
        return false;
    }
    if (snapshot.five_hour_available || snapshot.weekly_available) {
        return true;
    }
    return std::any_of(
        snapshot.additional_rate_limits.begin(),
        snapshot.additional_rate_limits.end(),
        [](const ProviderAdditionalRateLimit& limit) {
            return limit.primary.available || limit.secondary.available;
        }
    );
}

// ----------------------------------------------------------------------------
// Calcule le pourcentage restant a partir du pourcentage consomme.
//
// Parametres :
// - used_percent : pourcentage utilise lu dans le releve.
//
// Retour :
// - pourcentage restant borne entre 0 et 100.
// ----------------------------------------------------------------------------
double RemainingPercent(double used_percent) {
    return 100.0 - std::clamp(used_percent, 0.0, 100.0);
}

// ----------------------------------------------------------------------------
// Calcule la baisse de quota restant entre deux releves.
//
// Parametres :
// - previous_used_percent : pourcentage utilise precedent.
// - current_used_percent : pourcentage utilise courant.
//
// Retour :
// - baisse du quota restant, ou valeur negative si le quota est remonte.
// ----------------------------------------------------------------------------
double RemainingDropPercent(double previous_used_percent, double current_used_percent) {
    return RemainingPercent(previous_used_percent) - RemainingPercent(current_used_percent);
}

// ----------------------------------------------------------------------------
// Indique si deux fenetres disponibles franchissent le seuil configure.
//
// Parametres :
// - previous_available : disponibilite de la valeur precedente.
// - previous_used_percent : pourcentage utilise precedent.
// - current_available : disponibilite de la valeur courante.
// - current_used_percent : pourcentage utilise courant.
// - threshold : seuil minimum de baisse du quota restant.
//
// Retour :
// - true si les deux valeurs sont comparables et franchissent le seuil.
// ----------------------------------------------------------------------------
bool WindowReachesUsageDropThreshold(
    bool previous_available,
    double previous_used_percent,
    bool current_available,
    double current_used_percent,
    double threshold
) {
    return previous_available
        && current_available
        && RemainingDropPercent(previous_used_percent, current_used_percent) >= threshold;
}

// ----------------------------------------------------------------------------
// Indique si une fenetre provider supplementaire franchit le seuil configure.
//
// Parametres :
// - previous : fenetre precedente.
// - current : fenetre courante.
// - threshold : seuil minimum de baisse du quota restant.
//
// Retour :
// - true si la fenetre est comparable et franchit le seuil.
// ----------------------------------------------------------------------------
bool AdditionalWindowReachesUsageDropThreshold(
    const ProviderRateLimitWindow& previous,
    const ProviderRateLimitWindow& current,
    double threshold
) {
    return WindowReachesUsageDropThreshold(
        previous.available,
        previous.used_percent,
        current.available,
        current.used_percent,
        threshold
    );
}

// ----------------------------------------------------------------------------
// Indique si une fenetre disponible vient de repasser a cent pour cent restant.
//
// Parametres :
// - previous_available : disponibilite de la valeur precedente.
// - previous_used_percent : pourcentage utilise precedent.
// - current_available : disponibilite de la valeur courante.
// - current_used_percent : pourcentage utilise courant.
//
// Retour :
// - true si le quota etait entame puis est entierement disponible.
// ----------------------------------------------------------------------------
bool WindowReachesQuotaReset(
    bool previous_available,
    double previous_used_percent,
    bool current_available,
    double current_used_percent
) {
    return previous_available
        && current_available
        && RemainingPercent(previous_used_percent) < 100.0
        && RemainingPercent(current_used_percent) >= 100.0;
}

// ----------------------------------------------------------------------------
// Indique si une baisse atteint le seuil configure.
//
// Parametres :
// - previous : releve precedent.
// - current : releve courant.
// - threshold_percent : seuil minimum de baisse.
//
// Retour :
// - true si au moins une fenetre de quota franchit le seuil.
// - false sinon.
// ----------------------------------------------------------------------------
bool ReachesUsageDropThreshold(
    const UsageSnapshot& previous,
    const UsageSnapshot& current,
    int threshold_percent
) {
    const double threshold = static_cast<double>(threshold_percent);
    if (WindowReachesUsageDropThreshold(
            previous.five_hour_available,
            previous.five_hour_used_percent,
            current.five_hour_available,
            current.five_hour_used_percent,
            threshold
        )
        || WindowReachesUsageDropThreshold(
            previous.weekly_available,
            previous.weekly_used_percent,
            current.weekly_available,
            current.weekly_used_percent,
            threshold
        )) {
        return true;
    }

    for (const ProviderAdditionalRateLimit& current_limit : current.additional_rate_limits) {
        const auto previous_limit = std::find_if(
            previous.additional_rate_limits.begin(),
            previous.additional_rate_limits.end(),
            [&current_limit](const ProviderAdditionalRateLimit& candidate) {
                return candidate.id == current_limit.id;
            }
        );
        if (previous_limit == previous.additional_rate_limits.end()) {
            continue;
        }
        if (AdditionalWindowReachesUsageDropThreshold(
                previous_limit->primary,
                current_limit.primary,
                threshold
            )
            || AdditionalWindowReachesUsageDropThreshold(
                previous_limit->secondary,
                current_limit.secondary,
                threshold
            )) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Indique si au moins une fenetre de quota vient d'etre reinitialisee.
//
// Parametres :
// - previous : releve precedent.
// - current : releve courant.
//
// Retour :
// - true si une fenetre standard ou supplementaire repasse a cent pour cent.
// ----------------------------------------------------------------------------
bool ReachesQuotaReset(
    const UsageSnapshot& previous,
    const UsageSnapshot& current
) {
    if (WindowReachesQuotaReset(
            previous.five_hour_available,
            previous.five_hour_used_percent,
            current.five_hour_available,
            current.five_hour_used_percent
        )
        || WindowReachesQuotaReset(
            previous.weekly_available,
            previous.weekly_used_percent,
            current.weekly_available,
            current.weekly_used_percent
        )) {
        return true;
    }

    for (const ProviderAdditionalRateLimit& current_limit : current.additional_rate_limits) {
        const auto previous_limit = std::find_if(
            previous.additional_rate_limits.begin(),
            previous.additional_rate_limits.end(),
            [&current_limit](const ProviderAdditionalRateLimit& candidate) {
                return candidate.id == current_limit.id;
            }
        );
        if (previous_limit == previous.additional_rate_limits.end()) {
            continue;
        }
        if (WindowReachesQuotaReset(
                previous_limit->primary.available,
                previous_limit->primary.used_percent,
                current_limit.primary.available,
                current_limit.primary.used_percent
            )
            || WindowReachesQuotaReset(
                previous_limit->secondary.available,
                previous_limit->secondary.used_percent,
                current_limit.secondary.available,
                current_limit.secondary.used_percent
            )) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Indique si le cooldown autorise un nouveau declenchement.
//
// Parametres :
// - last_triggered_at : dernier declenchement connu.
// - now : instant courant.
// - minimum_interval_seconds : delai minimal demande.
//
// Retour :
// - true si le cooldown est expire.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsCooldownElapsed(
    const std::optional<std::chrono::system_clock::time_point>& last_triggered_at,
    std::chrono::system_clock::time_point now,
    int minimum_interval_seconds
) {
    if (!last_triggered_at.has_value()) {
        return true;
    }

    return now - *last_triggered_at >= std::chrono::seconds{minimum_interval_seconds};
}

}  // namespace

// ----------------------------------------------------------------------------
// Observe un releve avec les reglages communs des Effets Motion.
// ----------------------------------------------------------------------------
bool WidgetVibrationController::ObserveSnapshot(
    const WidgetMotionEffectsSettings& settings,
    const UsageSnapshot& snapshot,
    std::chrono::system_clock::time_point now
) {
    const WidgetMotionEffectsSettings normalized = NormalizeWidgetMotionEffectsSettings(settings);
    if (!HasUsableQuotaData(snapshot)) {
        return false;
    }

    const std::optional<UsageSnapshot> previous_snapshot = last_snapshot_;
    last_snapshot_ = snapshot;
    if (!normalized.enabled || !previous_snapshot.has_value()) {
        return false;
    }
    const bool usage_drop_triggered = normalized.trigger_on_usage_drop
        && ReachesUsageDropThreshold(
            *previous_snapshot,
            snapshot,
            normalized.usage_drop_threshold_percent
        );
    const bool quota_reset_triggered = normalized.trigger_on_quota_reset
        && ReachesQuotaReset(*previous_snapshot, snapshot);
    if (!usage_drop_triggered && !quota_reset_triggered) {
        return false;
    }
    if (!IsCooldownElapsed(last_triggered_at_, now, normalized.minimum_interval_seconds)) {
        return false;
    }

    last_triggered_at_ = now;
    return true;
}

// ----------------------------------------------------------------------------
// Oublie l'historique local du controleur.
// ----------------------------------------------------------------------------
void WidgetVibrationController::Reset() {
    last_snapshot_.reset();
    last_triggered_at_.reset();
}

// ============================================================================
// Codex Deck - Implementation des animations visuelles Tree
// ----------------------------------------------------------------------------
// Ce fichier garde les courbes de micro-interactions deterministes et
// reutilisables par le renderer.
// ============================================================================

#include "TreeVisualAnimation.h"

#include <algorithm>
#include <cmath>

// ----------------------------------------------------------------------------
// Rapproche une valeur visuelle de sa cible.
// ----------------------------------------------------------------------------
float ApproachVisualValue(float current, float target, float step) {
    const float bounded_step = std::max(0.0F, step);
    if (current < target) {
        return std::min(target, current + bounded_step);
    }
    if (current > target) {
        return std::max(target, current - bounded_step);
    }
    return current;
}

// ----------------------------------------------------------------------------
// Calcule l'offset horizontal d'un titre selectionne tronque.
// ----------------------------------------------------------------------------
float ComputeMarqueeOffset(float available_width, float natural_width, float phase) {
    const float overflow = natural_width - available_width;
    if (available_width <= 0.0F || overflow <= 0.0F) {
        return 0.0F;
    }
    return std::clamp(phase, 0.0F, overflow);
}

// ----------------------------------------------------------------------------
// Calcule la largeur de layout requise pour reveler un titre en marquee.
// ----------------------------------------------------------------------------
float ComputeMarqueeLayoutWidth(float available_width, float natural_width, bool marquee_active) {
    if (!marquee_active || natural_width <= available_width) {
        return std::max(1.0F, available_width);
    }
    return std::max(1.0F, natural_width + 1.0F);
}

// ----------------------------------------------------------------------------
// Avance une progression avec acceleration et deceleration amorties.
// ----------------------------------------------------------------------------
DampedVisualState AdvanceDampedVisualState(
    DampedVisualState state,
    float target,
    float elapsed_seconds,
    float smooth_time_seconds,
    float maximum_speed
) {
    const float delta = std::max(0.0F, elapsed_seconds);
    const float smooth_time = std::max(0.01F, smooth_time_seconds);
    const float speed_limit = std::max(0.0F, maximum_speed);
    if (delta <= 0.0F) {
        return state;
    }

    const float omega = 2.0F / smooth_time;
    const float scaled_delta = omega * delta;
    const float decay = 1.0F / (1.0F + scaled_delta + 0.48F * scaled_delta * scaled_delta + 0.235F * scaled_delta * scaled_delta * scaled_delta);
    const float original_value = state.value;
    const float original_target = target;
    const float maximum_displacement = speed_limit * smooth_time;
    const float displacement = std::clamp(
        state.value - target,
        -maximum_displacement,
        maximum_displacement
    );
    target = state.value - displacement;
    const float temporary = (state.velocity + omega * displacement) * delta;
    state.velocity = (state.velocity - omega * temporary) * decay;
    state.value = target + (displacement + temporary) * decay;

    const bool passed_target = ((original_target - original_value) > 0.0F) == (state.value > original_target);
    if (passed_target) {
        state.value = original_target;
        state.velocity = 0.0F;
    }

    constexpr float kSettledDistance = 0.0005F;
    constexpr float kSettledVelocity = 0.002F;
    if (std::fabs(state.value - original_target) <= kSettledDistance
        && std::fabs(state.velocity) <= kSettledVelocity) {
        state.value = original_target;
        state.velocity = 0.0F;
    }
    return state;
}

// ----------------------------------------------------------------------------
// Avance un marquee et freine son mouvement avant le retour a droite.
// ----------------------------------------------------------------------------
MarqueeAnimationState AdvanceMarqueeAnimation(
    MarqueeAnimationState state,
    bool active,
    float elapsed_seconds,
    float smooth_time_seconds,
    float coast_projection_seconds,
    float maximum_speed
) {
    if (active) {
        state.was_active = true;
        state.coasting = false;
        state.visual = AdvanceDampedVisualState(
            state.visual,
            state.maximum_offset,
            elapsed_seconds,
            smooth_time_seconds,
            maximum_speed
        );
        return state;
    }

    if (state.was_active) {
        state.was_active = false;
        if (state.visual.velocity > 0.002F && state.visual.value < state.maximum_offset) {
            state.coasting = true;
            state.coast_target = std::clamp(
                state.visual.value + state.visual.velocity * std::max(0.0F, coast_projection_seconds),
                state.visual.value,
                state.maximum_offset
            );
        }
    }

    if (state.coasting) {
        state.visual = AdvanceDampedVisualState(
            state.visual,
            state.coast_target,
            elapsed_seconds,
            std::max(0.01F, smooth_time_seconds * 0.5F),
            maximum_speed
        );
        if (state.visual.value == state.coast_target && state.visual.velocity == 0.0F) {
            state.coasting = false;
        }
        return state;
    }

    state.visual = AdvanceDampedVisualState(
        state.visual,
        0.0F,
        elapsed_seconds,
        smooth_time_seconds,
        maximum_speed
    );
    return state;
}

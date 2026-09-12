// ============================================================================
// Codex Glass - Champ de deformation Wave
// ----------------------------------------------------------------------------
// Ce fichier implemente une vague directionnelle ou radiale avec front,
// train de cretes et amortissement spatial, sans dependance graphique.
// ============================================================================

#include "WidgetMotionWaveField.h"

#include <algorithm>
#include <cmath>

namespace {

// Valeur locale de deux pi utilisee pour la phase de la vague.
constexpr float kTwoPi = 6.28318530718F;

// Amplitude maximale du relief du front relativement a la longueur d'onde.
constexpr float kMaximumFrontUndulationRatio = 0.32F;

// Periode transversale du relief relativement a la longueur d'onde.
constexpr float kFrontUndulationPeriodRatio = 1.65F;

// Nombre de lobes utilises pour onduler un front radial.
constexpr float kRadialFrontLobeCount = 6.0F;

// Interpole progressivement entre deux bornes.
float SmoothStep(float edge0, float edge1, float value) {
    const float span = std::max(0.001F, edge1 - edge0);
    const float t = std::clamp((value - edge0) / span, 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
}

}  // namespace

// Calcule le champ Wave pour une position, des dimensions et des reglages donnes.
WidgetMotionWaveSample EvaluateWidgetMotionWaveField(
    float x,
    float y,
    float width,
    float height,
    float progress,
    const WidgetMotionWaveSettings& settings
) {
    if (!settings.enabled || width <= 1.0F || height <= 1.0F || progress < 0.0F || progress > 1.0F) {
        return WidgetMotionWaveSample{};
    }

    float normal_x = 1.0F;
    float normal_y = 0.0F;
    switch (settings.direction) {
    case WidgetMotionWaveDirection::Left: normal_x = -1.0F; break;
    case WidgetMotionWaveDirection::Up: normal_x = 0.0F; normal_y = -1.0F; break;
    case WidgetMotionWaveDirection::Down: normal_x = 0.0F; normal_y = 1.0F; break;
    case WidgetMotionWaveDirection::Radial: break;
    case WidgetMotionWaveDirection::Right: default: break;
    }

    const float wavelength = std::clamp(
        72.0F * static_cast<float>(settings.wavelength_percent) / 100.0F,
        18.0F,
        216.0F
    );
    float coordinate = 0.0F;
    float front_undulation_unit = 0.0F;
    float travel_length = width;
    if (settings.direction == WidgetMotionWaveDirection::Radial) {
        const float center_x = width * 0.5F;
        const float center_y = height * 0.5F;
        const float delta_x = x - center_x;
        const float delta_y = y - center_y;
        const float distance = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));
        if (distance > 0.001F) {
            normal_x = delta_x / distance;
            normal_y = delta_y / distance;
        }
        coordinate = distance;
        front_undulation_unit = std::sin(
            std::atan2(delta_y, delta_x) * kRadialFrontLobeCount
        );
        travel_length = std::sqrt((center_x * center_x) + (center_y * center_y));
    } else {
        const float projection = (x * normal_x) + (y * normal_y);
        const float minimum_projection = std::min(0.0F, width * normal_x)
            + std::min(0.0F, height * normal_y);
        const float maximum_projection = std::max(0.0F, width * normal_x)
            + std::max(0.0F, height * normal_y);
        coordinate = projection - minimum_projection;
        const float transverse = (-x * normal_y) + (y * normal_x);
        const float undulation_period = std::max(
            1.0F,
            wavelength * kFrontUndulationPeriodRatio
        );
        front_undulation_unit = std::sin(transverse * kTwoPi / undulation_period);
        travel_length = std::max(1.0F, maximum_projection - minimum_projection);
    }

    const float speed = static_cast<float>(settings.speed_percent) / 100.0F;
    const float front = std::clamp(progress * speed, 0.0F, 1.35F) * (travel_length + wavelength);
    const float front_undulation = front_undulation_unit
        * wavelength
        * kMaximumFrontUndulationRatio
        * static_cast<float>(settings.front_undulation_percent) / 100.0F;
    const float behind_front = front + front_undulation - coordinate;
    const float train_length = wavelength * static_cast<float>(std::clamp(settings.wave_count, 1, 6));
    if (behind_front < 0.0F || behind_front > train_length) {
        return WidgetMotionWaveSample{};
    }

    const float damping = static_cast<float>(std::clamp(settings.damping_percent, 0, 100)) / 100.0F;
    const float phase = behind_front * kTwoPi / wavelength;
    const float attack = SmoothStep(0.0F, wavelength * 0.18F, behind_front);
    const float tail = 1.0F - SmoothStep(train_length * 0.58F, train_length, behind_front);
    const float spatial = std::pow(
        std::max(0.0F, 1.0F - (behind_front / std::max(1.0F, train_length))),
        0.35F + (2.4F * damping)
    );
    const float amplitude = 4.5F
        * static_cast<float>(settings.intensity_percent) / 100.0F
        * static_cast<float>(settings.refraction_percent) / 100.0F;
    const float amount = std::sin(phase) * attack * tail * spatial * amplitude;
    return WidgetMotionWaveSample{normal_x * amount, normal_y * amount};
}

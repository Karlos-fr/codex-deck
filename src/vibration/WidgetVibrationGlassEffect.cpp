// ============================================================================
// Codex Glass - Implementation du moteur de vibration GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier transforme une intensite amortie en petites variations de
// refraction, sans declencher de capture ni modifier l'opacite globale.
// ============================================================================

#include "WidgetVibrationGlassEffect.h"
#include "../motion/WidgetMotionCurves.h"

#include <algorithm>
#include <cmath>

namespace {

// Renfort maximal de refraction globale en pixels.
constexpr float kMaximumRefractionBoost = 2.2F;

// Renfort maximal de vague de bord en pixels.
constexpr float kMaximumEdgeWaveStrength = 1.4F;

// Renfort maximal des lentilles autour des barres de progression en pixels.
constexpr float kMaximumProgressLensShake = 2.6F;

// Renfort maximal de teinte GlassEffect.
constexpr float kMaximumTintOpacityBoost = 0.035F;

// Opacite maximale du contour GlassEffect pendant une vibration.
constexpr float kMaximumGlassBorderPulse = 0.10F;

// Opacite maximale du pulse de contour.
constexpr float kMaximumBorderPulse = 0.42F;

// ----------------------------------------------------------------------------
// Convertit une intensite quelconque en flottant borne.
//
// Parametres :
// - intensity : intensite source.
//
// Retour :
// - intensite entre 0 et 1.
// ----------------------------------------------------------------------------
float NormalizedIntensity(double intensity) {
    return static_cast<float>(std::clamp(intensity, 0.0, 1.0));
}

}  // namespace

// ----------------------------------------------------------------------------
// Indique si un etat de vibration GlassEffect est actif.
//
// Parametres :
// - state : etat a tester.
//
// Retour :
// - true si au moins une variation temporaire est active.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsWidgetVibrationGlassEffectActive(const WidgetVibrationGlassEffectState& state) {
    return state.refraction_boost > 0.0F
        || state.edge_wave_strength > 0.0F
        || state.progress_lens_shake > 0.0F
        || state.tint_opacity_boost > 0.0F
        || state.border_pulse > 0.0F
        || state.wave_refraction > 0.0F;
}

// ----------------------------------------------------------------------------
// Compose les trois Effets Motion pour leurs progressions independantes.
// ----------------------------------------------------------------------------
WidgetVibrationGlassEffectState WidgetVibrationGlassEffect::Evaluate(
    const WidgetMotionEffectsSettings& settings,
    GlassEffectMode glass_effect_mode,
    double vibration_progress,
    double vibration_intensity,
    double pulse_progress,
    double wave_progress
) const {
    (void)vibration_progress;
    const WidgetMotionEffectsSettings normalized = NormalizeWidgetMotionEffectsSettings(settings);
    if (!normalized.enabled) {
        return WidgetVibrationGlassEffectState{};
    }

    WidgetVibrationGlassEffectState state{};
    if (normalized.vibration.enabled && glass_effect_mode == GlassEffectMode::Enabled) {
        const float intensity = NormalizedIntensity(vibration_intensity);
        const float refraction_scale = static_cast<float>(normalized.vibration.refraction_percent) / 100.0F;
        state.refraction_boost = kMaximumRefractionBoost * intensity * refraction_scale;
        state.edge_wave_strength = kMaximumEdgeWaveStrength * intensity * refraction_scale;
        state.progress_lens_shake = kMaximumProgressLensShake * intensity * refraction_scale;
        state.tint_opacity_boost = kMaximumTintOpacityBoost * intensity * refraction_scale;
        state.border_pulse = kMaximumGlassBorderPulse * intensity;
    }

    if (normalized.pulse.enabled && pulse_progress < 1.0) {
        const float pulse = static_cast<float>(WidgetMotionPulseEnvelope(
            pulse_progress,
            normalized.pulse.repetitions,
            normalized.pulse.softness_percent
        ));
        state.border_pulse = std::clamp(
            state.border_pulse + (kMaximumBorderPulse * pulse
                * static_cast<float>(normalized.pulse.intensity_percent) / 100.0F),
            0.0F,
            0.55F
        );
        state.pulse_extent = static_cast<float>(normalized.pulse.extent_percent) / 100.0F;
        state.pulse_active = true;
        state.pulse_phase = static_cast<float>(WidgetMotionPulseCycleProgress(
            pulse_progress,
            normalized.pulse.repetitions
        ));
    }

    if (normalized.wave.enabled && wave_progress < 1.0) {
        const float temporal_fade = static_cast<float>(std::pow(
            std::max(0.0F, 1.0F - static_cast<float>(wave_progress)),
            0.15F + (0.85F * static_cast<float>(normalized.wave.damping_percent) / 100.0F)
        ));
        state.wave_position = static_cast<float>(std::clamp(wave_progress, 0.0, 1.0));
        state.wave_direction = normalized.wave.direction;
        state.wave_wavelength = static_cast<float>(normalized.wave.wavelength_percent);
        state.wave_count = normalized.wave.wave_count;
        state.wave_damping = static_cast<float>(normalized.wave.damping_percent) / 100.0F;
        state.wave_refraction = 9.0F * temporal_fade
            * static_cast<float>(normalized.wave.intensity_percent) / 100.0F
            * static_cast<float>(normalized.wave.refraction_percent) / 100.0F;
        state.wave_speed = static_cast<float>(normalized.wave.speed_percent) / 100.0F;
        state.wave_front_undulation = static_cast<float>(
            normalized.wave.front_undulation_percent
        ) / 100.0F;
    }

    return state;
}

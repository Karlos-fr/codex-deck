// ============================================================================
// Codex Glass - Moteur de vibration visuelle GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare les parametres temporaires appliques au rendu GlassEffect
// pendant une vibration du widget.
// ============================================================================

#pragma once

#include "../glass/WidgetGlassSettings.h"
#include "../motion/WidgetMotionEffectsSettings.h"

// ----------------------------------------------------------------------------
// Regroupe les variations temporaires envoyees au renderer GlassEffect.
// ----------------------------------------------------------------------------
struct WidgetVibrationGlassEffectState {
    // Renfort temporaire de refraction globale.
    float refraction_boost = 0.0F;

    // Renfort temporaire de vague sur les bords du panneau.
    float edge_wave_strength = 0.0F;

    // Renfort temporaire des lentilles autour des barres de progression.
    float progress_lens_shake = 0.0F;

    // Renfort temporaire de la teinte GlassEffect.
    float tint_opacity_boost = 0.0F;

    // Intensite temporaire du pulse de contour.
    float border_pulse = 0.0F;

    // Etendue du pulse depuis le contour vers l'interieur.
    float pulse_extent = 0.0F;

    // Indique que le style Pulse, distinct du halo de Vibration, est actif.
    bool pulse_active = false;

    // Progression locale de la pulsation courante entre zero et un.
    float pulse_phase = 0.0F;

    // Position normalisee du front de deformation Wave.
    float wave_position = 0.0F;

    // Direction de propagation de la deformation Wave.
    WidgetMotionWaveDirection wave_direction = WidgetMotionWaveDirection::Right;

    // Longueur d'onde relative de la deformation Wave.
    float wave_wavelength = 100.0F;

    // Nombre d'ondes du train Wave.
    int wave_count = 1;

    // Amortissement de Wave entre zero et un.
    float wave_damping = 0.0F;

    // Force de refraction propre a Wave.
    float wave_refraction = 0.0F;

    // Vitesse relative de propagation de Wave.
    float wave_speed = 1.0F;

    // Ondulation transversale du front Wave entre zero et un.
    float wave_front_undulation = 0.0F;
};

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
bool IsWidgetVibrationGlassEffectActive(const WidgetVibrationGlassEffectState& state);

// ----------------------------------------------------------------------------
// Convertit l'intensite de timeline en variations de rendu GlassEffect.
// ----------------------------------------------------------------------------
class WidgetVibrationGlassEffect {
public:
    // ------------------------------------------------------------------------
    // Compose les trois Effets Motion pour leurs progressions independantes.
    // ------------------------------------------------------------------------
    WidgetVibrationGlassEffectState Evaluate(
        const WidgetMotionEffectsSettings& settings,
        GlassEffectMode glass_effect_mode,
        double vibration_progress,
        double vibration_intensity,
        double pulse_progress,
        double wave_progress
    ) const;
};

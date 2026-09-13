// ============================================================================
// Codex Glass - Distortion GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare le traitement de refraction applique a une frame GlassEffect
// avant son rendu Direct2D.
// ============================================================================

#pragma once

#include "WidgetGlassEffectFrame.h"
#include "WidgetGlassSettings.h"

#include <vector>

// ----------------------------------------------------------------------------
// Zone locale qui agit comme une petite lentille de verre glass_effecte.
// ----------------------------------------------------------------------------
struct WidgetGlassEffectLens {
    // Coordonnees du rectangle de lentille dans la frame capturee.
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;

    // Rayon du rectangle arrondi en pixels.
    float radius = 0.0F;

    // Largeur de la zone de refraction autour du bord en pixels.
    float band = 0.0F;

    // Zoom signe en pixels : positif pour grossir, negatif pour reduire.
    float strength = 0.0F;

    // Douceur normalisee de la transition autour du contour.
    float softness = 1.0F;
};

// ----------------------------------------------------------------------------
// Reglages numeriques de la refraction GlassEffect.
// ----------------------------------------------------------------------------
struct WidgetGlassEffectDistortionSettings {
    // Rayon du verre principal en pixels.
    float glass_corner_radius = 26.0F;

    // Largeur de la zone de refraction du verre principal en pixels.
    float edge_band = 58.0F;

    // Force de refraction du verre principal en pixels.
    float edge_strength = 18.0F;

    // Decalage chromatique du verre principal en pixels.
    float chromatic_shift = 1.35F;

    // Decalage chromatique des lentilles locales en pixels.
    float lens_chromatic_shift = 0.85F;
};

// ----------------------------------------------------------------------------
// Reglages numeriques des deformations animees GlassEffect.
// ----------------------------------------------------------------------------
struct WidgetGlassEffectAnimationSettings {
    // Canal anime generique reutilise par les trois effets cumulables.
    struct Channel {
        // Indique si ce canal contribue a la deformation finale.
        bool enabled = false;
        // Amplitude de deformation en pixels.
        float amplitude = 0.0F;
        // Vitesse temporelle du canal.
        float speed = 0.0F;
        // Longueur d'onde en pixels.
        float wavelength = 96.0F;
        // Amplitude des perturbations secondaires.
        float noise = 0.0F;
        // Fluidite propre au canal Liquid.
        float fluidity = 1.0F;
        // Densite propre au canal Rain.
        float density = 1.0F;
        // Rayon maximal des anneaux Rain.
        float ring_radius = 58.0F;
        // Duree relative du fondu Rain.
        float fade = 1.0F;
    };

    // Reglages runtime de Calm Water.
    Channel calm_water{};

    // Reglages runtime de Liquid.
    Channel liquid{};

    // Reglages runtime de Rain.
    Channel rain{};

    // Temps courant de l'animation en secondes.
    float time_seconds = 0.0F;

    // Active une vague Motion transitoire composee avec l'animation Glass.
    bool motion_wave_enabled = false;

    // Progression normalisee du front de vague Motion.
    float motion_wave_progress = 0.0F;

    // Direction cartesienne horizontale du front de vague.
    float motion_wave_direction_x = 1.0F;

    // Direction cartesienne verticale du front de vague.
    float motion_wave_direction_y = 0.0F;

    // Indique une propagation radiale depuis le centre.
    bool motion_wave_radial = false;

    // Nombre de cretes successives dans le train de vagues.
    int motion_wave_count = 1;

    // Amortissement spatial du train de vagues entre zero et un.
    float motion_wave_damping = 0.0F;

    // Amplitude de refraction Motion en pixels.
    float motion_wave_refraction = 0.0F;

    // Vitesse relative du front de vague Motion.
    float motion_wave_speed = 1.0F;

    // Longueur d'onde de la Wave Motion transitoire.
    float motion_wave_wavelength = 96.0F;

    // Ondulation transversale du front Wave entre zero et un.
    float motion_wave_front_undulation = 0.0F;
};

// ----------------------------------------------------------------------------
// Transforme les trois animations persistantes en reglages runtime cumulables.
// ----------------------------------------------------------------------------
WidgetGlassEffectAnimationSettings GlassEffectAnimationSettingsForSettings(
    const GlassEffectSettings& settings,
    float time_seconds
);

// ----------------------------------------------------------------------------
// Applique une deformation de type verre glass_effecte a une frame capturee.
//
// Parametres :
// - source : frame BGRA capturee.
// - lenses : zones locales supplementaires a refracter.
// - settings : reglages de refraction a appliquer.
// - animation : reglages optionnels de deformation animee.
//
// Retour :
// - frame BGRA deformee, avec generation conservee.
// ----------------------------------------------------------------------------
WidgetGlassEffectFrame DistortGlassEffectFrame(
    const WidgetGlassEffectFrame& source,
    const std::vector<WidgetGlassEffectLens>& lenses = {},
    const WidgetGlassEffectDistortionSettings& settings = {},
    const WidgetGlassEffectAnimationSettings& animation = {}
);

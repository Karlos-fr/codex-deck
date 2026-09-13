// ============================================================================
// Codex Glass - Types partages du rendu
// ----------------------------------------------------------------------------
// Ce fichier declare les petites structures transverses utilisees par plusieurs
// futurs modules de rendu, sans posseder de ressources Direct2D natives.
// ============================================================================

#pragma once

#include "../glass/WidgetGlassEffectDistortion.h"

#include <d2d1.h>

// ----------------------------------------------------------------------------
// Regroupe les parametres visuels d'un preset GlassEffect.
// ----------------------------------------------------------------------------
struct GlassEffectRenderProfile {
    // Opacite du fond capture.
    float background_opacity = 0.72F;

    // Decalage en DIPs utilise pour adoucir le fond capture.
    float soft_sample_offset = 1.2F;

    // Opacite de la teinte posee au-dessus du fond capture.
    float tint_opacity = 0.48F;

    // Opacite du bruit discret pose sur le fond GlassEffect.
    float noise_opacity = 0.045F;

    // Largeur de refraction autour des barres en mode GlassEffect.
    float progress_lens_band = 12.0F;

    // Force de refraction autour des barres en mode GlassEffect.
    float progress_lens_strength = 8.0F;

    // Reglages de la deformation GlassEffect.
    WidgetGlassEffectDistortionSettings distortion{};

    // Douceur normalisee de la transition des lentilles locales.
    float element_lens_softness = 1.0F;
};

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
// Regroupe les couleurs Direct2D derivees des reglages du widget.
// ----------------------------------------------------------------------------
struct Palette {
    // Couleur du fond global de la fenetre.
    D2D1_COLOR_F window_background;

    // Couleur du panneau principal.
    D2D1_COLOR_F panel_background;

    // Couleur du contour du panneau et du graphe.
    D2D1_COLOR_F border;

    // Couleur du titre.
    D2D1_COLOR_F title_text;

    // Couleur des textes principaux.
    D2D1_COLOR_F body_text;

    // Couleur des textes secondaires.
    D2D1_COLOR_F muted_text;

    // Couleur de fond des barres de progression.
    D2D1_COLOR_F progress_background;

    // Couleur d'accent representant l'usage restant.
    D2D1_COLOR_F remaining_accent;

    // Couleur des courbes du graphe historique.
    D2D1_COLOR_F history_curve;

    // Couleur des controles actifs et des effets lumineux d'interface.
    D2D1_COLOR_F active_control;
};

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

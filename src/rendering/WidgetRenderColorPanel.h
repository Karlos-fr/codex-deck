// ============================================================================
// Codex Glass - Rendu du panneau couleurs
// ----------------------------------------------------------------------------
// Ce fichier declare le dessin de l'extension laterale de personnalisation des
// couleurs. Le layout et le hit-test restent dans `src/color/WidgetColorPanel`.
// ============================================================================

#pragma once

#include "WidgetRenderGlassEffect.h"
#include "WidgetRenderTypes.h"
#include "../color/WidgetColorPanel.h"
#include "../settings/AppSettings.h"

#include <d2d1.h>
#include <dwrite.h>

#include <vector>

// ----------------------------------------------------------------------------
// Regroupe les ressources Direct2D/DirectWrite necessaires au panneau couleurs.
// ----------------------------------------------------------------------------
struct WidgetRenderColorPanelContext {
    // Cible Direct2D courante.
    ID2D1RenderTarget* render_target = nullptr;

    // Cache GlassEffect utilise pour dessiner le fond capture.
    WidgetRenderGlassEffectCache* glass_effect_cache = nullptr;

    // Brosse de fond de secours.
    ID2D1Brush* panel_background_brush = nullptr;

    // Brosse de contour.
    ID2D1Brush* border_brush = nullptr;

    // Brosse de texte principal.
    ID2D1Brush* body_text_brush = nullptr;

    // Brosse de texte secondaire.
    ID2D1Brush* muted_text_brush = nullptr;

    // Format DirectWrite des libelles.
    IDWriteTextFormat* color_panel_label_text_format = nullptr;

    // Format DirectWrite des boutons.
    IDWriteTextFormat* color_panel_button_text_format = nullptr;

    // Format DirectWrite du titre compact.
    IDWriteTextFormat* body_text_format = nullptr;
};

// ----------------------------------------------------------------------------
// Dessine le panneau lateral de personnalisation des couleurs.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - size : taille client du render target.
// - settings : reglages contenant les couleurs courantes.
// - interaction : etat souris courant du panneau.
// - glass_effect_frame : frame GlassEffect optionnelle.
// - lenses : lentilles GlassEffect deja construites.
// - glass_effect_profile : profil GlassEffect courant.
// - glass_effect_animation : deformation animee GlassEffect courante.
// - palette : palette Direct2D active.
// ----------------------------------------------------------------------------
void DrawColorPanel(
    const WidgetRenderColorPanelContext& context,
    D2D1_SIZE_F size,
    const AppSettings& settings,
    const WidgetColorPanelInteraction& interaction,
    const WidgetGlassEffectFrame* glass_effect_frame,
    const std::vector<WidgetGlassEffectLens>& lenses,
    const GlassEffectRenderProfile& glass_effect_profile,
    const WidgetGlassEffectAnimationSettings& glass_effect_animation,
    const Palette& palette
);

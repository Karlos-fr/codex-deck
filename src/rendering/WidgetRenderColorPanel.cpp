// ============================================================================
// Codex Glass - Implementation du rendu du panneau couleurs
// ----------------------------------------------------------------------------
// Ce fichier dessine les carres de couleur et les boutons du panneau lateral,
// sans traiter les evenements souris ni calculer le layout.
// ============================================================================

#include "WidgetRenderColorPanel.h"

#include "WidgetButton.h"
#include "WidgetRenderConstants.h"
#include "WidgetRenderPalette.h"
#include "../color/WidgetColorTools.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <cwchar>
#include <wrl/client.h>

namespace {

// ----------------------------------------------------------------------------
// Dessine un texte DirectWrite dans le rectangle donne.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - text : texte Unicode a dessiner.
// - layout_rect : rectangle de destination en DIPs.
// - format : format DirectWrite a utiliser.
// - brush : brosse Direct2D a utiliser.
// ----------------------------------------------------------------------------
void DrawColorPanelTextLine(
    const WidgetRenderColorPanelContext& context,
    const wchar_t* text,
    const D2D1_RECT_F& layout_rect,
    IDWriteTextFormat* format,
    ID2D1Brush* brush
) {
    context.render_target->DrawTextW(
        text,
        static_cast<UINT32>(wcslen(text)),
        format,
        layout_rect,
        brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
}

// ----------------------------------------------------------------------------
// Retourne l'etat visuel d'un bouton du panneau couleurs.
//
// Parametres :
// - action : action representee par le bouton.
// - interaction : etat souris courant.
//
// Retour :
// - etat visuel a dessiner.
// ----------------------------------------------------------------------------
WidgetButtonVisualState ColorPanelButtonState(
    WidgetColorPanelActionType action,
    const WidgetColorPanelInteraction& interaction
) {
    const WidgetColorPanelHitTestResult target{action, WidgetColorField::Background};
    if (IsSameWidgetColorPanelHit(interaction.pressed, target)
        && IsSameWidgetColorPanelHit(interaction.hovered, target)) {
        return WidgetButtonVisualState::Pressed;
    }

    if (IsSameWidgetColorPanelHit(interaction.hovered, target)) {
        return WidgetButtonVisualState::Hovered;
    }

    return WidgetButtonVisualState::Normal;
}

// ----------------------------------------------------------------------------
// Retourne l'etat visuel d'un carre couleur du panneau.
//
// Parametres :
// - field : champ couleur represente.
// - interaction : etat souris courant.
//
// Retour :
// - etat visuel a dessiner.
// ----------------------------------------------------------------------------
WidgetButtonVisualState ColorPanelSwatchState(
    WidgetColorField field,
    const WidgetColorPanelInteraction& interaction
) {
    const WidgetColorPanelHitTestResult target{WidgetColorPanelActionType::PickColor, field};
    if (IsSameWidgetColorPanelHit(interaction.pressed, target)
        && IsSameWidgetColorPanelHit(interaction.hovered, target)) {
        return WidgetButtonVisualState::Pressed;
    }

    if (IsSameWidgetColorPanelHit(interaction.hovered, target)) {
        return WidgetButtonVisualState::Hovered;
    }

    return WidgetButtonVisualState::Normal;
}

}

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
) {
    const WidgetColorPanelLayout layout = BuildWidgetColorPanelLayout(size);
    if (glass_effect_frame != nullptr && context.glass_effect_cache != nullptr) {
        context.glass_effect_cache->DrawBackground(
            context.render_target,
            context.panel_background_brush,
            *glass_effect_frame,
            layout.panel_rect,
            size,
            palette,
            lenses,
            glass_effect_profile,
            glass_effect_animation
        );
    } else {
        context.render_target->FillRectangle(layout.panel_rect, context.panel_background_brush);
    }
    context.render_target->DrawLine(
        D2D1::Point2F(layout.panel_rect.left, layout.panel_rect.top),
        D2D1::Point2F(layout.panel_rect.left, layout.panel_rect.bottom),
        context.border_brush,
        1.0F
    );

    const D2D1_RECT_F title_rect = D2D1::RectF(
        layout.panel_rect.left + 14.0F,
        layout.panel_rect.top + 6.0F,
        layout.close_button_rect.left - 8.0F,
        layout.panel_rect.top + 30.0F
    );
    DrawColorPanelTextLine(context, T(IDS_MENU_COLORS).c_str(), title_rect, context.body_text_format, context.body_text_brush);

    for (size_t index = 0; index < layout.color_swatch_rects.size(); ++index) {
        const WidgetColorField field = WidgetColorFieldFromPanelIndex(index);
        const D2D1_ROUNDED_RECT swatch_rect = D2D1::RoundedRect(
            layout.color_swatch_rects[index],
            kColorPanelSwatchRadius,
            kColorPanelSwatchRadius
        );
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> swatch_brush;
        if (SUCCEEDED(context.render_target->CreateSolidColorBrush(
                ColorRefToD2DColor(GetWidgetColorField(settings.colors, field)),
                swatch_brush.GetAddressOf()
            ))) {
            context.render_target->FillRoundedRectangle(swatch_rect, swatch_brush.Get());
        }

        const WidgetButtonVisualState swatch_state = ColorPanelSwatchState(field, interaction);
        if (swatch_state != WidgetButtonVisualState::Normal) {
            const float overlay_alpha = swatch_state == WidgetButtonVisualState::Pressed ? 0.20F : 0.12F;
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> overlay_brush;
            if (SUCCEEDED(context.render_target->CreateSolidColorBrush(
                    D2D1::ColorF(1.0F, 1.0F, 1.0F, overlay_alpha),
                    overlay_brush.GetAddressOf()
                ))) {
                context.render_target->FillRoundedRectangle(swatch_rect, overlay_brush.Get());
            }
        }
        context.render_target->DrawRoundedRectangle(
            swatch_rect,
            context.border_brush,
            swatch_state == WidgetButtonVisualState::Normal ? 1.0F : 1.4F
        );

        const std::wstring label = WidgetColorFieldLabel(field);
        DrawColorPanelTextLine(
            context,
            label.c_str(),
            layout.color_label_rects[index],
            context.color_panel_label_text_format,
            context.muted_text_brush
        );
    }

    DrawWidgetTextButton(
        context.render_target,
        layout.random_button_rect,
        T(IDS_COLOR_RANDOM).c_str(),
        context.color_panel_button_text_format,
        context.body_text_brush,
        context.border_brush,
        ColorPanelButtonState(WidgetColorPanelActionType::Randomize, interaction)
    );
    DrawWidgetCloseButton(
        context.render_target,
        layout.close_button_rect,
        context.body_text_brush,
        context.border_brush,
        ColorPanelButtonState(WidgetColorPanelActionType::Close, interaction)
    );
}

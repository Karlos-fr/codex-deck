// ============================================================================
// Codex Glass - Implementation des boutons dessines
// ----------------------------------------------------------------------------
// Ce fichier centralise le rendu Direct2D des petits boutons custom du widget.
// ============================================================================

#include "WidgetButton.h"

#include <cwchar>

namespace {

// Rayon des coins des boutons compacts.
constexpr float kWidgetButtonCornerRadius = 4.0F;

// Epaisseur du contour des boutons.
constexpr float kWidgetButtonBorderWidth = 1.0F;

// Epaisseur de l'icone de fermeture.
constexpr float kCloseIconStrokeWidth = 1.25F;

// ----------------------------------------------------------------------------
// Retourne la couleur de fond adaptee a l'etat du bouton.
//
// Parametres :
// - state : etat visuel courant.
//
// Retour :
// - couleur Direct2D transparente ou legerement visible.
// ----------------------------------------------------------------------------
D2D1_COLOR_F ButtonBackgroundColor(WidgetButtonVisualState state) {
    switch (state) {
    case WidgetButtonVisualState::Pressed:
        return D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.16F);
    case WidgetButtonVisualState::Hovered:
        return D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.09F);
    case WidgetButtonVisualState::Normal:
    default:
        return D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.035F);
    }
}

// ----------------------------------------------------------------------------
// Retourne un rectangle decale pour l'etat presse.
//
// Parametres :
// - bounds : rectangle source.
// - state : etat visuel courant.
//
// Retour :
// - rectangle eventuellement decale.
// ----------------------------------------------------------------------------
D2D1_RECT_F ButtonStateRect(D2D1_RECT_F bounds, WidgetButtonVisualState state) {
    if (state == WidgetButtonVisualState::Pressed) {
        bounds.left += 0.5F;
        bounds.top += 0.5F;
        bounds.right += 0.5F;
        bounds.bottom += 0.5F;
    }

    return bounds;
}

// ----------------------------------------------------------------------------
// Dessine le fond et le contour arrondis d'un bouton.
//
// Parametres :
// - render_target : cible Direct2D active.
// - bounds : rectangle du bouton.
// - border_brush : brosse du contour.
// - state : etat visuel courant.
// ----------------------------------------------------------------------------
void DrawWidgetButtonFrame(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& bounds,
    ID2D1Brush* border_brush,
    WidgetButtonVisualState state
) {
    if (render_target == nullptr || border_brush == nullptr) {
        return;
    }

    ID2D1SolidColorBrush* background_brush = nullptr;
    if (SUCCEEDED(render_target->CreateSolidColorBrush(ButtonBackgroundColor(state), &background_brush))) {
        const D2D1_ROUNDED_RECT rounded_rect{
            ButtonStateRect(bounds, state),
            kWidgetButtonCornerRadius,
            kWidgetButtonCornerRadius,
        };
        render_target->FillRoundedRectangle(rounded_rect, background_brush);
        render_target->DrawRoundedRectangle(rounded_rect, border_brush, kWidgetButtonBorderWidth);
        background_brush->Release();
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// Dessine un bouton texte compact.
// ----------------------------------------------------------------------------
void DrawWidgetTextButton(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& bounds,
    const wchar_t* text,
    IDWriteTextFormat* text_format,
    ID2D1Brush* text_brush,
    ID2D1Brush* border_brush,
    WidgetButtonVisualState state
) {
    if (render_target == nullptr || text == nullptr || text_format == nullptr || text_brush == nullptr) {
        return;
    }

    const D2D1_RECT_F state_bounds = ButtonStateRect(bounds, state);
    DrawWidgetButtonFrame(render_target, bounds, border_brush, state);
    render_target->DrawTextW(
        text,
        static_cast<UINT32>(wcslen(text)),
        text_format,
        state_bounds,
        text_brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
}

// ----------------------------------------------------------------------------
// Dessine un bouton de fermeture compact avec une croix vectorielle.
// ----------------------------------------------------------------------------
void DrawWidgetCloseButton(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& bounds,
    ID2D1Brush* icon_brush,
    ID2D1Brush* border_brush,
    WidgetButtonVisualState state
) {
    if (render_target == nullptr || icon_brush == nullptr) {
        return;
    }

    const D2D1_RECT_F state_bounds = ButtonStateRect(bounds, state);
    DrawWidgetButtonFrame(render_target, bounds, border_brush, state);

    const float center_x = (state_bounds.left + state_bounds.right) * 0.5F;
    const float center_y = (state_bounds.top + state_bounds.bottom) * 0.5F;
    const float radius = 3.6F;
    render_target->DrawLine(
        D2D1::Point2F(center_x - radius, center_y - radius),
        D2D1::Point2F(center_x + radius, center_y + radius),
        icon_brush,
        kCloseIconStrokeWidth
    );
    render_target->DrawLine(
        D2D1::Point2F(center_x + radius, center_y - radius),
        D2D1::Point2F(center_x - radius, center_y + radius),
        icon_brush,
        kCloseIconStrokeWidth
    );
}

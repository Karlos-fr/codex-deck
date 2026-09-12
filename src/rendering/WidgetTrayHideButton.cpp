// ============================================================================
// Codex Glass - Implementation du bouton de masquage dans le tray
// ----------------------------------------------------------------------------
// Ce fichier dessine un controle vectoriel discret et son hint localise. Il ne
// connait ni le HWND du widget ni la logique applicative de visibilite.
// ============================================================================

#include "WidgetTrayHideButton.h"

#include "WidgetRenderConstants.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <wrl/client.h>

namespace {

// Taille de la zone cliquable du bouton en DIPs.
constexpr float kTrayButtonSize = 18.0F;

// Marge du bouton par rapport aux bords du widget en mode standard.
constexpr float kTrayButtonEdgeMargin = 5.0F;

// Marge legerement reduite pour exploiter le bandeau compact du mode minimal.
constexpr float kMinimalTrayButtonEdgeMargin = 4.0F;

// Facteur de reduction du controle dans les modes horizontal et vertical.
constexpr float kOrientedTrayButtonScale = 0.75F;

// Rayon du fond affiche au survol en DIPs.
constexpr float kTrayButtonCornerRadius = 4.0F;

// Hauteur du hint en DIPs.
constexpr float kTrayButtonHintHeight = 24.0F;

// Espacement entre le bouton et son hint en DIPs.
constexpr float kTrayButtonHintGap = 5.0F;

// Epaisseur de l'icone vectorielle en DIPs.
constexpr float kTrayButtonIconStrokeWidth = 1.1F;

// Retourne la couleur du fond interactif selon l'etat du bouton.
//
// Parametres :
// - interaction : etat visuel courant.
//
// Retour :
// - couleur blanche translucide adaptee au survol ou a la pression.
D2D1_COLOR_F TrayButtonBackgroundColor(const WidgetTrayHideButtonInteraction& interaction) {
    return interaction.pressed && interaction.hovered
        ? D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.13F)
        : D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.07F);
}

// Dessine l'icone compacte representant une descente vers le tray.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - bounds : rectangle du bouton.
// - pressed : indique si l'icone doit etre legerement decalee.
void DrawTrayIcon(
    const WidgetTrayHideButtonRenderContext& context,
    const D2D1_RECT_F& bounds,
    bool pressed
) {
    const float scale = (bounds.right - bounds.left) / kTrayButtonSize;
    const float offset = pressed ? 0.5F * scale : 0.0F;
    const float center_x = ((bounds.left + bounds.right) * 0.5F) + offset;
    const float center_y = ((bounds.top + bounds.bottom) * 0.5F) + offset;
    context.render_target->DrawLine(
        D2D1::Point2F(center_x, center_y - (4.0F * scale)),
        D2D1::Point2F(center_x, center_y + scale),
        context.muted_text_brush,
        kTrayButtonIconStrokeWidth * scale
    );
    context.render_target->DrawLine(
        D2D1::Point2F(center_x - (2.3F * scale), center_y - scale),
        D2D1::Point2F(center_x, center_y + (1.3F * scale)),
        context.muted_text_brush,
        kTrayButtonIconStrokeWidth * scale
    );
    context.render_target->DrawLine(
        D2D1::Point2F(center_x + (2.3F * scale), center_y - scale),
        D2D1::Point2F(center_x, center_y + (1.3F * scale)),
        context.muted_text_brush,
        kTrayButtonIconStrokeWidth * scale
    );
    context.render_target->DrawLine(
        D2D1::Point2F(center_x - (4.0F * scale), center_y + (4.0F * scale)),
        D2D1::Point2F(center_x + (4.0F * scale), center_y + (4.0F * scale)),
        context.muted_text_brush,
        kTrayButtonIconStrokeWidth * scale
    );
}

// Dessine le hint localise sous le bouton.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - rect : rectangle de destination du hint.
void DrawTrayButtonHint(
    const WidgetTrayHideButtonRenderContext& context,
    const D2D1_RECT_F& rect
) {
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 4.0F, 4.0F);
    context.render_target->FillRoundedRectangle(rounded, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(rounded, context.border_brush, 1.0F);

    const DWRITE_TEXT_ALIGNMENT previous_alignment = context.caption_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = context.caption_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    const std::wstring hint = T(IDS_WIDGET_HIDE_TO_TRAY_HINT);
    context.render_target->DrawTextW(
        hint.c_str(),
        static_cast<UINT32>(hint.size()),
        context.caption_text_format,
        rect,
        context.muted_text_brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(previous_alignment);
    context.caption_text_format->SetParagraphAlignment(previous_paragraph);
    context.caption_text_format->SetWordWrapping(previous_wrapping);
}

} // namespace

// Calcule la geometrie du bouton depuis la taille et le mode du widget.
WidgetTrayHideButtonLayout BuildWidgetTrayHideButtonLayout(
    D2D1_SIZE_F size,
    WidgetDisplayMode display_mode,
    float hint_text_width
) {
    const bool oriented_mode = display_mode == WidgetDisplayMode::Horizontal
        || display_mode == WidgetDisplayMode::Vertical;
    const float button_size = oriented_mode
        ? kTrayButtonSize * kOrientedTrayButtonScale
        : kTrayButtonSize;
    const float edge_margin = display_mode == WidgetDisplayMode::Minimal
        ? kMinimalTrayButtonEdgeMargin
        : (oriented_mode
            ? kMinimalTrayButtonEdgeMargin * kOrientedTrayButtonScale
            : kTrayButtonEdgeMargin);
    const float top = edge_margin;
    const float right = std::max(edge_margin + button_size, size.width - edge_margin);
    WidgetTrayHideButtonLayout layout{};
    layout.button_rect = D2D1::RectF(
        right - button_size,
        top,
        right,
        top + button_size
    );
    const float hint_right = layout.button_rect.right;
    const float hint_width = std::max(
        0.0F,
        hint_text_width + (kWidgetHintHorizontalPadding * 2.0F)
    );
    const float hint_left = std::max(6.0F, hint_right - hint_width);
    layout.hint_rect = D2D1::RectF(
        hint_left,
        layout.button_rect.bottom + kTrayButtonHintGap,
        hint_right,
        layout.button_rect.bottom + kTrayButtonHintGap + kTrayButtonHintHeight
    );
    return layout;
}

// Indique si un point Direct2D appartient au bouton.
bool HitTestWidgetTrayHideButton(
    const WidgetTrayHideButtonLayout& layout,
    D2D1_POINT_2F point
) {
    return point.x >= layout.button_rect.left
        && point.x <= layout.button_rect.right
        && point.y >= layout.button_rect.top
        && point.y <= layout.button_rect.bottom;
}

// Dessine le bouton discret et son hint eventuel.
void DrawWidgetTrayHideButton(
    const WidgetTrayHideButtonRenderContext& context,
    const WidgetTrayHideButtonLayout& layout,
    const WidgetTrayHideButtonInteraction& interaction
) {
    if (context.render_target == nullptr
        || context.panel_background_brush == nullptr
        || context.border_brush == nullptr
        || context.muted_text_brush == nullptr
        || context.caption_text_format == nullptr) {
        return;
    }

    if (interaction.hovered) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> background_brush;
        if (SUCCEEDED(context.render_target->CreateSolidColorBrush(
                TrayButtonBackgroundColor(interaction),
                background_brush.GetAddressOf()
            ))) {
            context.render_target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    layout.button_rect,
                    kTrayButtonCornerRadius
                        * ((layout.button_rect.right - layout.button_rect.left) / kTrayButtonSize),
                    kTrayButtonCornerRadius
                        * ((layout.button_rect.right - layout.button_rect.left) / kTrayButtonSize)
                ),
                background_brush.Get()
            );
        }
    }

    context.muted_text_brush->SetOpacity(interaction.hovered ? 0.92F : 0.46F);
    DrawTrayIcon(context, layout.button_rect, interaction.pressed && interaction.hovered);
    context.muted_text_brush->SetOpacity(1.0F);
    if (interaction.hovered && !interaction.pressed) {
        DrawTrayButtonHint(context, layout.hint_rect);
    }
}

// ============================================================================
// Codex Glass - Controles partages des graphes
// ----------------------------------------------------------------------------
// Ce fichier dessine le controle segmente jointif avec les couleurs du theme.
// Les renderers de donnees n'en dependent pas.
// ============================================================================

#include "WidgetRenderGraphControls.h"

#include "WidgetGraphFormatting.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <cmath>
#include <wrl/client.h>

namespace {

// Rayon du contour exterieur du controle segmente.
constexpr float kSegmentedControlRadius = 5.0F;

// Epaisseur du contour unique du controle segmente.
constexpr float kSegmentedControlStrokeWidth = 1.0F;

// Opacite de la couleur personnalisee sur le segment actif.
constexpr float kActiveSegmentOpacity = 0.88F;

// Opacite de la couleur personnalisee sur un segment inactif survole.
constexpr float kHoveredSegmentOpacity = 0.14F;

// Opacite de la couleur personnalisee pendant l'appui souris.
constexpr float kPressedSegmentOpacity = 1.0F;

// Rayon des coins du selecteur de plage.
constexpr float kRangeSelectorRadius = 3.0F;

// Opacite du fond au survol du selecteur.
constexpr float kRangeSelectorHoverOpacity = 0.12F;

// Rayon du fond interactif du bouton de capture.
constexpr float kCaptureButtonRadius = 4.0F;

// Epaisseur du pictogramme de capture.
constexpr float kCaptureIconStrokeWidth = 1.0F;

// Retourne la couleur du fond du bouton selon son etat.
D2D1_COLOR_F CaptureButtonBackgroundColor(const WidgetGraphInteraction& interaction) {
    return interaction.pressed_graph_capture_button
        && interaction.hovered_graph_capture_button
        ? D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.13F)
        : D2D1::ColorF(1.0F, 1.0F, 1.0F, 0.07F);
}

// Dessine le pictogramme vectoriel compact d'une capture d'ecran.
void DrawCaptureIcon(
    const WidgetRenderGraphContext& context,
    const D2D1_RECT_F& bounds,
    bool pressed
) {
    const float offset = pressed ? 0.5F : 0.0F;
    const float center_x = ((bounds.left + bounds.right) * 0.5F) + offset;
    const float center_y = ((bounds.top + bounds.bottom) * 0.5F) + offset;
    const D2D1_ROUNDED_RECT camera = D2D1::RoundedRect(
        D2D1::RectF(center_x - 4.5F, center_y - 3.2F, center_x + 4.5F, center_y + 3.8F),
        1.4F,
        1.4F
    );
    context.render_target->DrawRoundedRectangle(
        camera,
        context.muted_text_brush,
        kCaptureIconStrokeWidth
    );
    context.render_target->DrawLine(
        D2D1::Point2F(center_x - 2.4F, center_y - 3.2F),
        D2D1::Point2F(center_x - 1.2F, center_y - 4.5F),
        context.muted_text_brush,
        kCaptureIconStrokeWidth
    );
    context.render_target->DrawLine(
        D2D1::Point2F(center_x - 1.2F, center_y - 4.5F),
        D2D1::Point2F(center_x + 1.2F, center_y - 4.5F),
        context.muted_text_brush,
        kCaptureIconStrokeWidth
    );
    context.render_target->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(center_x, center_y + 0.2F), 1.8F, 1.8F),
        context.muted_text_brush,
        kCaptureIconStrokeWidth
    );
}

// ----------------------------------------------------------------------------
// Retourne le libelle tres court utilise uniquement dans le selecteur compact.
//
// Parametres :
// - range : plage temporelle actuellement active.
//
// Retour :
// - libelle abrege localise, sans modifier celui du menu deroulant.
// ----------------------------------------------------------------------------
std::wstring CompactGraphRangeLabel(GraphRange range, WidgetGraphPage page) {
    if (page == WidgetGraphPage::Activity && range == GraphRange::Days30) {
        return T(IDS_GRAPH_ACTIVITY_RANGE_DAYS);
    }
    switch (range) {
    case GraphRange::Days7:
        return T(IDS_GRAPH_RANGE_COMPACT_7D);
    case GraphRange::Days30:
        return T(IDS_GRAPH_RANGE_COMPACT_30D);
    default:
        return FormatGraphRangeLabel(range);
    }
}

// ----------------------------------------------------------------------------
// Convertit une composante sRGB en luminance lineaire.
//
// Parametres :
// - component : composante normalisee entre zero et un.
//
// Retour :
// - composante lineaire utilisee pour le calcul de contraste.
// ----------------------------------------------------------------------------
float LinearColorComponent(float component) {
    return component <= 0.04045F
        ? component / 12.92F
        : std::pow((component + 0.055F) / 1.055F, 2.4F);
}

// ----------------------------------------------------------------------------
// Calcule la luminance relative d'une couleur Direct2D.
//
// Parametres :
// - color : couleur dont le contraste doit etre estime.
//
// Retour :
// - luminance relative comprise entre zero et un.
// ----------------------------------------------------------------------------
float RelativeLuminance(const D2D1_COLOR_F& color) {
    return (0.2126F * LinearColorComponent(color.r))
        + (0.7152F * LinearColorComponent(color.g))
        + (0.0722F * LinearColorComponent(color.b));
}

// ----------------------------------------------------------------------------
// Calcule le contraste relatif entre deux couleurs.
//
// Parametres :
// - first : premiere couleur.
// - second : seconde couleur.
//
// Retour :
// - ratio de contraste superieur ou egal a un.
// ----------------------------------------------------------------------------
float ContrastRatio(const D2D1_COLOR_F& first, const D2D1_COLOR_F& second) {
    const float first_luminance = RelativeLuminance(first);
    const float second_luminance = RelativeLuminance(second);
    const float lighter = std::max(first_luminance, second_luminance);
    const float darker = std::min(first_luminance, second_luminance);
    return (lighter + 0.05F) / (darker + 0.05F);
}

// ----------------------------------------------------------------------------
// Choisit la brosse de texte la plus lisible sur le segment actif.
//
// Parametres :
// - context : couleurs courantes du graphe.
//
// Retour :
// - brosse principale ou de fond offrant le meilleur contraste.
// ----------------------------------------------------------------------------
ID2D1Brush* ActiveSegmentTextBrush(const WidgetRenderGraphContext& context) {
    const D2D1_COLOR_F accent = context.history_curve_brush->GetColor();
    const D2D1_COLOR_F body = context.body_text_brush->GetColor();
    const D2D1_COLOR_F background = context.panel_background_brush->GetColor();
    return ContrastRatio(accent, body) >= ContrastRatio(accent, background)
        ? static_cast<ID2D1Brush*>(context.body_text_brush)
        : static_cast<ID2D1Brush*>(context.panel_background_brush);
}

// ----------------------------------------------------------------------------
// Remplit un segment en conservant les coins exterieurs arrondis.
//
// Parametres :
// - context : cible et brosse d'accent du theme.
// - layout : geometrie du controle segmente.
// - segment_rect : segment a remplir.
// - opacity : opacite temporaire de la couleur d'accent.
// ----------------------------------------------------------------------------
void FillSegment(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const D2D1_RECT_F& segment_rect,
    float opacity
) {
    context.history_curve_brush->SetOpacity(opacity);
    context.render_target->PushAxisAlignedClip(segment_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context.render_target->FillRoundedRectangle(
        D2D1::RoundedRect(layout.segmented_control_rect, kSegmentedControlRadius, kSegmentedControlRadius),
        context.history_curve_brush
    );
    context.render_target->PopAxisAlignedClip();
    context.history_curve_brush->SetOpacity(1.0F);
}

// ----------------------------------------------------------------------------
// Dessine un libelle centre sans conserver les alignements temporaires.
//
// Parametres :
// - context : ressources de rendu.
// - text : libelle a dessiner.
// - rect : rectangle du segment.
// - brush : brosse du texte.
// ----------------------------------------------------------------------------
void DrawCenteredLabel(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    ID2D1Brush* brush
) {
    const auto old_text = context.caption_text_format->GetTextAlignment();
    const auto old_paragraph = context.caption_text_format->GetParagraphAlignment();
    const auto old_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    context.render_target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), context.caption_text_format,
        rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(old_text);
    context.caption_text_format->SetParagraphAlignment(old_paragraph);
    context.caption_text_format->SetWordWrapping(old_wrapping);
}

} // namespace

// ----------------------------------------------------------------------------
// Dessine le controle segmente unique sous la zone de graphe.
// ----------------------------------------------------------------------------
void DrawWidgetGraphSegmentedControl(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    WidgetGraphPage active_page,
    const WidgetGraphInteraction& interaction
) {
    const auto control = D2D1::RoundedRect(
        layout.segmented_control_rect,
        kSegmentedControlRadius,
        kSegmentedControlRadius
    );
    context.render_target->FillRoundedRectangle(control, context.panel_background_brush);
    const auto draw_segment = [&context, &layout, &interaction, active_page](
        const D2D1_RECT_F& rect,
        WidgetGraphPage page,
        const std::wstring& label
    ) {
        const bool active = active_page == page;
        const bool hovered = interaction.hovered_graph_tab == page;
        const bool pressed = interaction.pressed_graph_tab == page;
        if (active || hovered || pressed) {
            FillSegment(
                context,
                layout,
                rect,
                pressed ? kPressedSegmentOpacity
                    : (active ? kActiveSegmentOpacity : kHoveredSegmentOpacity)
            );
        }
        DrawCenteredLabel(
            context,
            label,
            rect,
            (active || pressed) ? ActiveSegmentTextBrush(context)
                : (hovered ? context.body_text_brush : context.muted_text_brush)
        );
    };
    draw_segment(layout.quotas_tab_rect, WidgetGraphPage::Quotas, T(IDS_GRAPH_TAB_QUOTAS));
    draw_segment(layout.tokens_tab_rect, WidgetGraphPage::Tokens, T(IDS_GRAPH_TAB_TOKENS));
    draw_segment(layout.activity_tab_rect, WidgetGraphPage::Activity, T(IDS_GRAPH_TAB_ACTIVITY));
    draw_segment(layout.summary_tab_rect, WidgetGraphPage::Summary, T(IDS_GRAPH_TAB_SUMMARY));
    context.render_target->DrawRoundedRectangle(control, context.border_brush, kSegmentedControlStrokeWidth);
    for (const float divider : {
            layout.quotas_tab_rect.right,
            layout.tokens_tab_rect.right,
            layout.activity_tab_rect.right,
        }) {
        context.render_target->DrawLine(
            D2D1::Point2F(divider, layout.segmented_control_rect.top),
            D2D1::Point2F(divider, layout.segmented_control_rect.bottom),
            context.border_brush,
            kSegmentedControlStrokeWidth
        );
    }
}

// ----------------------------------------------------------------------------
// Dessine le selecteur compact de la plage de la vue active.
//
// Parametres :
// - context : cible, formats et brosses du graphe.
// - layout : geometrie contenant le rectangle du controle.
// - range : plage dont le libelle doit etre affiche.
// - interaction : etat de survol courant.
// ----------------------------------------------------------------------------
void DrawWidgetGraphRangeSelector(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    GraphRange range,
    WidgetGraphPage page,
    const WidgetGraphInteraction& interaction
) {
    const D2D1_ROUNDED_RECT control = D2D1::RoundedRect(
        layout.range_selector_rect,
        kRangeSelectorRadius,
        kRangeSelectorRadius
    );
    context.render_target->FillRoundedRectangle(control, context.panel_background_brush);
    if (interaction.hovered_graph_range_selector) {
        context.history_curve_brush->SetOpacity(kRangeSelectorHoverOpacity);
        context.render_target->FillRoundedRectangle(control, context.history_curve_brush);
        context.history_curve_brush->SetOpacity(1.0F);
    }
    context.render_target->DrawRoundedRectangle(control, context.border_brush, 1.0F);

    const D2D1_RECT_F label_rect = D2D1::RectF(
        layout.range_selector_rect.left + 3.0F,
        layout.range_selector_rect.top,
        layout.range_selector_rect.right - 12.0F,
        layout.range_selector_rect.bottom
    );
    DrawCenteredLabel(
        context,
        CompactGraphRangeLabel(range, page),
        label_rect,
        interaction.hovered_graph_range_selector
            ? static_cast<ID2D1Brush*>(context.body_text_brush)
            : static_cast<ID2D1Brush*>(context.muted_text_brush)
    );

    const float center_x = layout.range_selector_rect.right - 7.0F;
    const float center_y = (layout.range_selector_rect.top + layout.range_selector_rect.bottom) * 0.5F;
    ID2D1Brush* chevron_brush = interaction.hovered_graph_range_selector
        ? static_cast<ID2D1Brush*>(context.body_text_brush)
        : static_cast<ID2D1Brush*>(context.muted_text_brush);
    context.render_target->DrawLine(
        D2D1::Point2F(center_x - 2.0F, center_y - 1.0F),
        D2D1::Point2F(center_x, center_y + 1.0F),
        chevron_brush,
        1.0F
    );
    context.render_target->DrawLine(
        D2D1::Point2F(center_x, center_y + 1.0F),
        D2D1::Point2F(center_x + 2.0F, center_y - 1.0F),
        chevron_brush,
        1.0F
    );
}

// ----------------------------------------------------------------------------
// Dessine le bouton discret qui copie la vue active dans le presse-papiers.
// ----------------------------------------------------------------------------
void DrawWidgetGraphCaptureButton(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
) {
    if (interaction.hovered_graph_capture_button) {
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> background_brush;
        if (SUCCEEDED(context.render_target->CreateSolidColorBrush(
                CaptureButtonBackgroundColor(interaction),
                background_brush.GetAddressOf()
            ))) {
            context.render_target->FillRoundedRectangle(
                D2D1::RoundedRect(
                    layout.capture_button_rect,
                    kCaptureButtonRadius,
                    kCaptureButtonRadius
                ),
                background_brush.Get()
            );
        }
    }

    context.muted_text_brush->SetOpacity(
        interaction.hovered_graph_capture_button ? 0.92F : 0.46F
    );
    DrawCaptureIcon(
        context,
        layout.capture_button_rect,
        interaction.pressed_graph_capture_button
            && interaction.hovered_graph_capture_button
    );
    context.muted_text_brush->SetOpacity(1.0F);

    if (!interaction.hovered_graph_capture_button
        || interaction.pressed_graph_capture_button) {
        return;
    }
    const D2D1_ROUNDED_RECT hint = D2D1::RoundedRect(layout.capture_hint_rect, 4.0F, 4.0F);
    context.render_target->FillRoundedRectangle(hint, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(hint, context.border_brush, 1.0F);
    DrawCenteredLabel(
        context,
        T(IDS_GRAPH_CAPTURE_HINT),
        layout.capture_hint_rect,
        context.muted_text_brush
    );
}

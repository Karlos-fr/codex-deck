// ============================================================================
// Codex Glass - Rendu du graphe horaire de tokens
// ----------------------------------------------------------------------------
// Ce fichier dessine une serie verticale stable, l'heure courante accentuee et
// une infobulle contrainte aux bords du graphe.
// ============================================================================

#include "WidgetRenderTokenGraph.h"

#include "WidgetGraphData.h"
#include "WidgetGraphFormatting.h"
#include "WidgetGraphInfoIcon.h"
#include "WidgetTokenGraphState.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <cmath>
#include <wrl/client.h>

namespace {

// Rayon discret des barres verticales.
constexpr float kTokenBarRadius = 2.0F;

// Marge interne de l'infobulle.
constexpr float kTooltipPadding = 6.0F;

// Espace interieur horizontal des libelles d'axes.
constexpr float kAxisLabelPadding = 3.0F;

// Marge interieure uniforme des hints explicatifs.
constexpr float kHintPadding = 7.0F;

// Hauteur des pointes reliant les infobulles a leur cible.
constexpr float kTooltipPointerHeight = 6.0F;

// Demi-largeur de la base triangulaire des pointes d'infobulle.
constexpr float kTooltipPointerHalfWidth = 5.0F;

// Marge horizontale compacte autour du texte d'une valeur survolee.
constexpr float kDataTooltipHorizontalPadding = 7.0F;

// Marge verticale compacte autour du texte d'une valeur survolee.
constexpr float kDataTooltipVerticalPadding = 3.0F;

// Opacite commune des libelles et de la ligne d'abscisse.
constexpr float kAxisForegroundOpacity = 1.0F;

// Opacite des lignes de grille pointillees.
constexpr float kAxisGridOpacity = 0.22F;

// ----------------------------------------------------------------------------
// Dessine un texte centre sans conserver les changements du format DirectWrite.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - text : texte a dessiner.
// - rect : rectangle de destination.
// - brush : brosse appliquee au texte.
// ----------------------------------------------------------------------------
void DrawCenteredText(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    ID2D1Brush* brush
) {
    const DWRITE_TEXT_ALIGNMENT previous_alignment = context.caption_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = context.caption_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    context.render_target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), context.caption_text_format,
        rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(previous_alignment);
    context.caption_text_format->SetParagraphAlignment(previous_paragraph);
    context.caption_text_format->SetWordWrapping(previous_wrapping);
}

// ----------------------------------------------------------------------------
// Dessine un titre aligne a gauche avec le format des libelles de quotas.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - text : texte a dessiner.
// - rect : rectangle de destination.
// - brush : brosse appliquee au texte.
// ----------------------------------------------------------------------------
void DrawLeadingText(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    ID2D1Brush* brush
) {
    const DWRITE_TEXT_ALIGNMENT previous_alignment = context.title_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = context.title_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping = context.title_text_format->GetWordWrapping();
    context.title_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.title_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    context.title_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    context.render_target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), context.title_text_format,
        rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.title_text_format->SetTextAlignment(previous_alignment);
    context.title_text_format->SetParagraphAlignment(previous_paragraph);
    context.title_text_format->SetWordWrapping(previous_wrapping);
}

// ----------------------------------------------------------------------------
// Dessine une pointe triangulaire reliant une infobulle a sa cible.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - base_left : extremite gauche de la base du triangle.
// - tip : pointe dirigee vers la cible.
// - base_right : extremite droite de la base du triangle.
// ----------------------------------------------------------------------------
void DrawTooltipPointer(
    const WidgetRenderGraphContext& context,
    D2D1_POINT_2F base_left,
    D2D1_POINT_2F tip,
    D2D1_POINT_2F base_right
) {
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(context.d2d_factory->CreatePathGeometry(geometry.GetAddressOf()))) {
        return;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.GetAddressOf()))) {
        return;
    }
    sink->BeginFigure(base_left, D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(tip);
    sink->AddLine(base_right);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) {
        return;
    }
    context.render_target->FillGeometry(geometry.Get(), context.panel_background_brush);
    context.render_target->DrawLine(
        base_left,
        base_right,
        context.panel_background_brush,
        2.0F
    );
    context.render_target->DrawLine(base_left, tip, context.border_brush, 1.0F);
    context.render_target->DrawLine(tip, base_right, context.border_brush, 1.0F);
}

// ----------------------------------------------------------------------------
// Mesure exactement un texte secondaire avec DirectWrite.
//
// Parametres :
// - context : ressources DirectWrite du graphe.
// - text : texte dont l'emprise doit etre calculee.
//
// Retour :
// - largeur et hauteur du texte, ou des valeurs nulles en cas d'echec.
// ----------------------------------------------------------------------------
D2D1_SIZE_F MeasureCaptionText(
    const WidgetRenderGraphContext& context,
    const std::wstring& text
) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
    if (FAILED(context.dwrite_factory->CreateTextLayout(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            context.caption_text_format,
            1000.0F,
            100.0F,
            text_layout.GetAddressOf()))) {
        return D2D1::SizeF(0.0F, 0.0F);
    }
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(text_layout->GetMetrics(&metrics))) {
        return D2D1::SizeF(0.0F, 0.0F);
    }
    return D2D1::SizeF(metrics.widthIncludingTrailingWhitespace, metrics.height);
}

// ----------------------------------------------------------------------------
// Mesure l'emprise d'un texte secondaire apres retour automatique a la ligne.
//
// Parametres :
// - context : ressources DirectWrite du graphe.
// - text : texte dont la hauteur doit etre calculee.
// - width : largeur disponible pour le texte sans les marges.
//
// Retour :
// - largeur de la ligne la plus longue et hauteur totale du texte mis en page.
// ----------------------------------------------------------------------------
D2D1_SIZE_F MeasureWrappedCaptionText(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    float width
) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
    if (FAILED(context.dwrite_factory->CreateTextLayout(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            context.caption_text_format,
            std::max(1.0F, width),
            1000.0F,
            text_layout.GetAddressOf()))) {
        return D2D1::SizeF(0.0F, 0.0F);
    }
    text_layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    DWRITE_TEXT_METRICS metrics{};
    return SUCCEEDED(text_layout->GetMetrics(&metrics))
        ? D2D1::SizeF(metrics.width, metrics.height)
        : D2D1::SizeF(0.0F, 0.0F);
}

// ----------------------------------------------------------------------------
// Dessine une ligne horizontale pointillee sans ressource persistante.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - left : debut horizontal de la ligne.
// - right : fin horizontale de la ligne.
// - y : position verticale de la ligne.
// ----------------------------------------------------------------------------
void DrawHorizontalDashes(
    const WidgetRenderGraphContext& context,
    float left,
    float right,
    float y
) {
    // Largeur d'un tiret horizontal de la grille.
    constexpr float kDashWidth = 3.0F;
    // Espacement entre deux tirets horizontaux de la grille.
    constexpr float kDashGap = 4.0F;
    for (float x = left; x < right; x += kDashWidth + kDashGap) {
        context.render_target->DrawLine(
            D2D1::Point2F(x, y),
            D2D1::Point2F(std::min(x + kDashWidth, right), y),
            context.muted_text_brush,
            0.7F
        );
    }
}

// ----------------------------------------------------------------------------
// Dessine le titre, la bulle d'information et son hint au survol.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - layout : geometrie partagee de la vue.
// - interaction : etat de survol de la bulle.
// ----------------------------------------------------------------------------
void DrawTokenGraphHeading(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
) {
    DrawLeadingText(context, T(IDS_GRAPH_TOKENS_TITLE), layout.title_rect, context.body_text_brush);
    if (context.show_header_controls) {
        context.muted_text_brush->SetOpacity(interaction.hovered_graph_info ? 1.0F : 0.72F);
        DrawWidgetGraphInfoIcon(
            context.render_target,
            context.muted_text_brush,
            layout.help_button_rect
        );
        context.muted_text_brush->SetOpacity(1.0F);
    }
    if (!context.show_header_controls || !interaction.hovered_graph_info) {
        return;
    }

    const std::wstring hint = T(IDS_GRAPH_TOKENS_SOURCE_HINT);
    D2D1_RECT_F hint_rect = layout.hint_tooltip_rect;
    const D2D1_SIZE_F wrapped_size = MeasureWrappedCaptionText(
        context,
        hint,
        (hint_rect.right - hint_rect.left) - (kHintPadding * 2.0F)
    );
    if (wrapped_size.width > 0.0F && wrapped_size.height > 0.0F) {
        hint_rect.right = std::min(
            layout.hint_tooltip_rect.right,
            hint_rect.left + std::ceil(wrapped_size.width) + (kHintPadding * 2.0F)
        );
        hint_rect.bottom = std::min(
            layout.hint_tooltip_rect.bottom,
            hint_rect.top + wrapped_size.height + (kHintPadding * 2.0F)
        );
    }
    const D2D1_ROUNDED_RECT tooltip = D2D1::RoundedRect(hint_rect, 4.0F, 4.0F);
    context.render_target->FillRoundedRectangle(tooltip, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(tooltip, context.border_brush, 1.0F);
    const float help_center_x = (
        layout.help_button_rect.left + layout.help_button_rect.right
    ) * 0.5F;
    const float pointer_center = std::clamp(
        help_center_x,
        hint_rect.left + 10.0F,
        hint_rect.right - 10.0F
    );
    DrawTooltipPointer(
        context,
        D2D1::Point2F(pointer_center - kTooltipPointerHalfWidth, hint_rect.top),
        D2D1::Point2F(pointer_center, hint_rect.top - kTooltipPointerHeight),
        D2D1::Point2F(pointer_center + kTooltipPointerHalfWidth, hint_rect.top)
    );
    const D2D1_RECT_F text_rect = D2D1::RectF(
        hint_rect.left + kHintPadding,
        hint_rect.top + kHintPadding,
        hint_rect.right - kHintPadding,
        hint_rect.bottom - kHintPadding
    );
    const DWRITE_TEXT_ALIGNMENT previous_alignment = context.caption_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = context.caption_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    context.render_target->DrawTextW(
        hint.c_str(), static_cast<UINT32>(hint.size()), context.caption_text_format,
        text_rect, context.body_text_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(previous_alignment);
    context.caption_text_format->SetParagraphAlignment(previous_paragraph);
    context.caption_text_format->SetWordWrapping(previous_wrapping);
}

// ----------------------------------------------------------------------------
// Dessine les axes localises selon l'echelle calculee.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - layout : geometrie partagee de la vue.
// - hours : heures affichees dans l'ordre chronologique.
// - axis_maximum : valeur placee au sommet de l'axe.
// - tick_step : intervalle entre deux graduations.
// - range : plage determinant le format des libelles temporels.
// ----------------------------------------------------------------------------
void DrawTokenAxes(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const std::vector<TokenHourlyUsage>& hours,
    std::uint64_t axis_maximum,
    std::uint64_t tick_step,
    GraphRange range
) {
    context.muted_text_brush->SetOpacity(kAxisGridOpacity);
    for (std::uint64_t value = 0; value <= axis_maximum; value += tick_step) {
        const float ratio = static_cast<float>(value) / static_cast<float>(axis_maximum);
        const float y = layout.plot_rect.bottom
            - (ratio * (layout.plot_rect.bottom - layout.plot_rect.top));
        DrawHorizontalDashes(context, layout.plot_rect.left, layout.plot_rect.right, y);
        const D2D1_RECT_F label_rect = D2D1::RectF(
            layout.y_axis_rect.left,
            y - 8.0F,
            layout.y_axis_rect.right - kAxisLabelPadding,
            y + 8.0F
        );
        context.muted_text_brush->SetOpacity(kAxisForegroundOpacity);
        DrawCenteredText(context, FormatGraphTokenAxisValue(value), label_rect, context.muted_text_brush);
        context.muted_text_brush->SetOpacity(kAxisGridOpacity);
        if (axis_maximum - value < tick_step) {
            break;
        }
    }
    context.muted_text_brush->SetOpacity(kAxisForegroundOpacity);
    context.render_target->DrawLine(
        D2D1::Point2F(layout.plot_rect.left, layout.plot_rect.bottom),
        D2D1::Point2F(layout.plot_rect.right, layout.plot_rect.bottom),
        context.muted_text_brush,
        0.8F
    );

    if (hours.empty()) {
        context.muted_text_brush->SetOpacity(1.0F);
        return;
    }
    const std::size_t count = std::min(hours.size(), layout.token_bar_rects.size());
    const float slot_width = count > 0
        ? (layout.plot_rect.right - layout.plot_rect.left) / static_cast<float>(count)
        : 0.0F;
    const std::size_t label_step = count <= 6
        ? 1U
        : static_cast<std::size_t>(std::ceil(static_cast<double>(count) / 6.0));
    const float label_half_width = std::max(
        1.0F,
        ((slot_width * static_cast<float>(label_step)) * 0.5F) - 2.0F
    );
    for (const std::size_t index : BuildTokenAxisLabelIndices(count)) {
        const float center = (
            layout.token_bar_rects[index].left + layout.token_bar_rects[index].right
        ) * 0.5F;
        const D2D1_RECT_F label_rect = D2D1::RectF(
            std::max(layout.x_axis_rect.left, center - label_half_width),
            layout.x_axis_rect.top,
            std::min(layout.x_axis_rect.right, center + label_half_width),
            layout.x_axis_rect.bottom
        );
        DrawCenteredText(
            context,
            FormatTokenBucketAxis(hours[index].bucket_start, range),
            label_rect,
            context.muted_text_brush
        );
    }
    context.muted_text_brush->SetOpacity(1.0F);
}

// ----------------------------------------------------------------------------
// Dessine l'infobulle compacte au-dessus de la barre survolee.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - layout : geometrie partagee de la vue.
// - hour : consommation horaire a afficher.
// - bar : barre utilisee pour centrer l'infobulle.
// ----------------------------------------------------------------------------
void DrawTokenTooltip(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenHourlyUsage& hour,
    const D2D1_RECT_F& bar,
    GraphRange range
) {
    const std::wstring text = FormatTokenBucketTooltip(
        hour.bucket_start,
        range,
        hour.counts.Total()
    );
    const D2D1_SIZE_F text_size = MeasureCaptionText(context, text);
    const float available_width = std::max(1.0F, layout.plot_rect.right - layout.plot_rect.left);
    const float width = std::min(
        available_width,
        std::max(1.0F, text_size.width + (kDataTooltipHorizontalPadding * 2.0F))
    );
    const float height = std::max(
        1.0F,
        text_size.height + (kDataTooltipVerticalPadding * 2.0F)
    );
    const float center = (bar.left + bar.right) * 0.5F;
    const float left = std::clamp(center - (width * 0.5F), layout.plot_rect.left, layout.plot_rect.right - width);
    const float bottom = std::max(
        layout.plot_rect.top + height + 2.0F,
        bar.top - kTooltipPointerHeight - 2.0F
    );
    const D2D1_RECT_F rect = D2D1::RectF(left, bottom - height, left + width, bottom);
    context.render_target->FillRoundedRectangle(D2D1::RoundedRect(rect, 3.0F, 3.0F), context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(D2D1::RoundedRect(rect, 3.0F, 3.0F), context.border_brush, 1.0F);
    const float pointer_center = std::clamp(center, rect.left + 9.0F, rect.right - 9.0F);
    DrawTooltipPointer(
        context,
        D2D1::Point2F(pointer_center - kTooltipPointerHalfWidth, rect.bottom),
        D2D1::Point2F(pointer_center, rect.bottom + kTooltipPointerHeight),
        D2D1::Point2F(pointer_center + kTooltipPointerHalfWidth, rect.bottom)
    );
    const D2D1_RECT_F text_rect = D2D1::RectF(
        rect.left + kDataTooltipHorizontalPadding,
        rect.top + kDataTooltipVerticalPadding,
        rect.right - kDataTooltipHorizontalPadding,
        rect.bottom - kDataTooltipVerticalPadding
    );
    DrawCenteredText(context, text, text_rect, context.body_text_brush);
}

} // namespace

// ----------------------------------------------------------------------------
// Dessine les barres de la plage horaire et l'infobulle de survol.
// ----------------------------------------------------------------------------
void DrawTokenUsageGraph(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenUsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange range
) {
    const auto draw_state = [&context, &layout, &interaction](unsigned int resource_id) {
        DrawCenteredText(context, T(resource_id), layout.plot_rect, context.muted_text_brush);
        DrawTokenGraphHeading(context, layout, interaction);
    };
    const WidgetTokenGraphState state = ResolveHourlyTokenGraphState(snapshot, range);
    if (state == WidgetTokenGraphState::Loading) {
        draw_state(IDS_GRAPH_LOADING);
        return;
    }
    if (state == WidgetTokenGraphState::Error) {
        draw_state(IDS_GRAPH_TOKENS_UNAVAILABLE);
        return;
    }
    if (state == WidgetTokenGraphState::NoSessions) {
        DrawCenteredText(context, T(IDS_GRAPH_NO_LOCAL_SESSIONS), layout.plot_rect, context.muted_text_brush);
        DrawTokenGraphHeading(context, layout, interaction);
        return;
    }
    if (state == WidgetTokenGraphState::Building) {
        draw_state(IDS_GRAPH_HISTORY_BUILDING);
        return;
    }
    std::vector<TokenHourlyUsage> source_buckets;
    if (range == GraphRange::Hours1) {
        source_buckets.reserve(snapshot.five_minute.size());
        for (const TokenFiveMinuteUsage& bucket : snapshot.five_minute) {
            source_buckets.push_back(TokenHourlyUsage{
                bucket.bucket_start,
                bucket.counts,
                bucket.request_count,
                bucket.models,
            });
        }
    } else {
        source_buckets = snapshot.hourly;
    }
    const std::size_t count = std::min(source_buckets.size(), layout.token_bar_rects.size());
    const std::size_t first_hour = source_buckets.size() - count;
    std::vector<TokenHourlyUsage> displayed_hours;
    displayed_hours.reserve(count);
    std::uint64_t maximum = 0;
    for (std::size_t index = 0; index < count; ++index) {
        displayed_hours.push_back(source_buckets[first_hour + index]);
        maximum = std::max(maximum, displayed_hours.back().counts.Total());
    }
    const auto [axis_maximum, tick_step] = CalculateTokenAxisScale(maximum);
    DrawTokenAxes(context, layout, displayed_hours, axis_maximum, tick_step, range);

    std::optional<D2D1_RECT_F> hovered_bar_rect;
    for (std::size_t index = 0; index < count; ++index) {
        const bool hovered = interaction.hovered_token_bar == index;
        const bool current_hour = index + 1 == count;
        const std::uint64_t total = displayed_hours[index].counts.Total();
        const double ratio = axis_maximum > 0
            ? static_cast<double>(total) / static_cast<double>(axis_maximum)
            : 0.0;
        D2D1_RECT_F bar = layout.token_bar_rects[index];
        const float height = ratio > 0.0
            ? std::max(2.0F, static_cast<float>((bar.bottom - bar.top) * ratio))
            : 0.0F;
        bar.top = bar.bottom - height;
        context.history_curve_brush->SetOpacity(hovered ? 1.0F : (current_hour ? 0.82F : 0.54F));
        if (height > 0.0F) {
            context.render_target->FillRoundedRectangle(
                D2D1::RoundedRect(bar, kTokenBarRadius, kTokenBarRadius),
                context.history_curve_brush
            );
        } else {
            context.render_target->DrawLine(
                D2D1::Point2F(bar.left, bar.bottom - 0.5F),
                D2D1::Point2F(bar.right, bar.bottom - 0.5F),
                context.history_curve_brush,
                1.0F
            );
        }
        if (hovered) {
            hovered_bar_rect = bar;
        }
    }
    context.history_curve_brush->SetOpacity(1.0F);
    if (!interaction.hovered_graph_info
        && interaction.hovered_token_bar.has_value()
        && *interaction.hovered_token_bar < count) {
        DrawTokenTooltip(
            context,
            layout,
            displayed_hours[*interaction.hovered_token_bar],
            hovered_bar_rect.value_or(layout.token_bar_rects[*interaction.hovered_token_bar]),
            range
        );
    }
    DrawTokenGraphHeading(context, layout, interaction);
}

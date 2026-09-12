// ============================================================================
// Codex Glass - Rendu du Bilan de consommation
// ----------------------------------------------------------------------------
// Ce fichier dessine quatre cartes compactes, deux sparklines, les tendances
// comparees et une jauge de cache sans modifier les donnees collectees.
// ============================================================================

#include "WidgetRenderKpiSummary.h"

#include "WidgetGraphInfoIcon.h"
#include "WidgetTokenGraphState.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../tokens/TokenKpiSummary.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace {

// Rayon des cartes du Bilan en DIPs.
constexpr float kSummaryCardRadius = 6.0F;

// Marge interieure horizontale des cartes du Bilan en DIPs.
constexpr float kSummaryCardPadding = 8.0F;

// Opacite du voile d'accent pose sur chaque carte.
constexpr float kSummaryCardAccentOpacity = 0.045F;

// Opacite de la sparkline secondaire.
constexpr float kSummarySparklineOpacity = 0.68F;

// Epaisseur de la sparkline en DIPs.
constexpr float kSummarySparklineStrokeWidth = 1.35F;

// Rayon de la jauge circulaire du cache en DIPs.
constexpr float kSummaryGaugeRadius = 12.0F;

// Epaisseur de la jauge circulaire du cache en DIPs.
constexpr float kSummaryGaugeStrokeWidth = 2.0F;

// Marge interne du hint explicatif en DIPs.
constexpr float kSummaryHintPadding = 7.0F;

// Valeur de pi utilisee pour construire l'arc Direct2D.
constexpr float kSummaryPi = 3.14159265358979323846F;

// ----------------------------------------------------------------------------
// Dessine un texte en preservant les alignements du format partage.
//
// Parametres :
// - context : cible de dessin courante.
// - text : chaine a afficher.
// - rect : rectangle de mise en page.
// - format : format DirectWrite temporairement ajuste.
// - brush : brosse du texte.
// - alignment : alignement horizontal souhaite.
// ----------------------------------------------------------------------------
void DrawSummaryText(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    IDWriteTextFormat* format,
    ID2D1Brush* brush,
    DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING
) {
    const DWRITE_TEXT_ALIGNMENT previous_alignment = format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping = format->GetWordWrapping();
    format->SetTextAlignment(alignment);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    context.render_target->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format,
        rect,
        brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    format->SetTextAlignment(previous_alignment);
    format->SetParagraphAlignment(previous_paragraph);
    format->SetWordWrapping(previous_wrapping);
}

// ----------------------------------------------------------------------------
// Remplace le separateur decimal par la virgule en interface francaise.
//
// Parametres :
// - text : valeur deja formatee avec un point decimal eventuel.
//
// Retour :
// - texte adapte a la langue active.
// ----------------------------------------------------------------------------
std::wstring LocalizeDecimalSeparator(std::wstring text) {
    if (ActiveUiLanguage() == UiLanguage::French) {
        std::replace(text.begin(), text.end(), L'.', L',');
    }
    return text;
}

// ----------------------------------------------------------------------------
// Formate un compteur en notation compacte sans masquer les petites valeurs.
//
// Parametres :
// - value : compteur entier a afficher.
//
// Retour :
// - entier ou valeur abregee avec suffixe k ou M.
// ----------------------------------------------------------------------------
std::wstring FormatCompactCount(std::uint64_t value) {
    if (value < 1'000ULL) {
        return std::to_wstring(value);
    }
    const double divisor = value >= 1'000'000ULL ? 1'000'000.0 : 1'000.0;
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(1)
        << (static_cast<double>(value) / divisor);
    return LocalizeDecimalSeparator(stream.str())
        + (value >= 1'000'000ULL ? L" M" : L" k");
}

// ----------------------------------------------------------------------------
// Formate une tendance par rapport a la periode precedente.
//
// Parametres :
// - current : valeur de la periode courante.
// - previous : valeur de la periode precedente.
// - comparison_available : indique que la periode precedente est complete.
//
// Retour :
// - pourcentage signe accompagne d'une fleche, ou aucune valeur non calculable.
// ----------------------------------------------------------------------------
std::optional<std::wstring> FormatTrend(
    std::uint64_t current,
    std::uint64_t previous,
    bool comparison_available
) {
    if (!comparison_available || previous == 0) {
        return std::nullopt;
    }
    const double percent = ((static_cast<double>(current) - static_cast<double>(previous))
        / static_cast<double>(previous)) * 100.0;
    const long rounded = std::lround(std::clamp(percent, -999.0, 999.0));
    if (rounded == 0) {
        return std::wstring{L"0 %"};
    }
    return std::wstring{rounded > 0 ? L"\x2191 " : L"\x2193 "}
        + std::to_wstring(std::labs(rounded)) + L" %";
}

// ----------------------------------------------------------------------------
// Dessine le fond et le contour communs d'une carte KPI.
//
// Parametres :
// - context : ressources graphiques partagees.
// - rect : rectangle exterieur de la carte.
// ----------------------------------------------------------------------------
void DrawSummaryCardBackground(
    const WidgetRenderGraphContext& context,
    const D2D1_RECT_F& rect
) {
    const D2D1_ROUNDED_RECT card = D2D1::RoundedRect(
        rect,
        kSummaryCardRadius,
        kSummaryCardRadius
    );
    context.render_target->FillRoundedRectangle(card, context.panel_background_brush);
    context.history_curve_brush->SetOpacity(kSummaryCardAccentOpacity);
    context.render_target->FillRoundedRectangle(card, context.history_curve_brush);
    context.history_curve_brush->SetOpacity(1.0F);
    context.render_target->DrawRoundedRectangle(card, context.border_brush, 1.0F);
}

// ----------------------------------------------------------------------------
// Dessine une micro-courbe chronologique dans une carte.
//
// Parametres :
// - context : factory, cible et brosse d'accent.
// - values : valeurs chronologiques a normaliser localement.
// - rect : zone reservee a droite de la carte.
// ----------------------------------------------------------------------------
void DrawSparkline(
    const WidgetRenderGraphContext& context,
    const std::vector<std::uint64_t>& values,
    const D2D1_RECT_F& rect
) {
    if (values.size() < 2 || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }
    const std::uint64_t maximum = *std::max_element(values.begin(), values.end());
    if (maximum == 0) {
        return;
    }
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (FAILED(context.d2d_factory->CreatePathGeometry(geometry.GetAddressOf()))) {
        return;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.GetAddressOf()))) {
        return;
    }
    const float step = (rect.right - rect.left) / static_cast<float>(values.size() - 1U);
    const auto point_for = [&values, &rect, maximum, step](std::size_t index) {
        const float ratio = static_cast<float>(
            static_cast<double>(values[index]) / static_cast<double>(maximum)
        );
        return D2D1::Point2F(
            rect.left + (step * static_cast<float>(index)),
            rect.bottom - (ratio * (rect.bottom - rect.top))
        );
    };
    sink->BeginFigure(point_for(0U), D2D1_FIGURE_BEGIN_HOLLOW);
    for (std::size_t index = 1; index < values.size(); ++index) {
        sink->AddLine(point_for(index));
    }
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    if (FAILED(sink->Close())) {
        return;
    }
    context.history_curve_brush->SetOpacity(kSummarySparklineOpacity);
    context.render_target->DrawGeometry(
        geometry.Get(),
        context.history_curve_brush,
        kSummarySparklineStrokeWidth
    );
    context.history_curve_brush->SetOpacity(1.0F);
}

// ----------------------------------------------------------------------------
// Dessine une jauge circulaire proportionnelle au taux de cache.
//
// Parametres :
// - context : factory, cible et brosses partagees.
// - center : centre de la jauge.
// - ratio : progression comprise entre zero et un.
// ----------------------------------------------------------------------------
void DrawCacheGauge(
    const WidgetRenderGraphContext& context,
    D2D1_POINT_2F center,
    double ratio
) {
    const double normalized = std::clamp(ratio, 0.0, 1.0);
    context.border_brush->SetOpacity(0.72F);
    context.render_target->DrawEllipse(
        D2D1::Ellipse(center, kSummaryGaugeRadius, kSummaryGaugeRadius),
        context.border_brush,
        kSummaryGaugeStrokeWidth
    );
    context.border_brush->SetOpacity(1.0F);
    if (normalized <= 0.0) {
        return;
    }
    context.history_curve_brush->SetOpacity(0.9F);
    if (normalized >= 0.999) {
        context.render_target->DrawEllipse(
            D2D1::Ellipse(center, kSummaryGaugeRadius, kSummaryGaugeRadius),
            context.history_curve_brush,
            kSummaryGaugeStrokeWidth
        );
        context.history_curve_brush->SetOpacity(1.0F);
        return;
    }
    const float angle = static_cast<float>(normalized) * 2.0F * kSummaryPi;
    const D2D1_POINT_2F start = D2D1::Point2F(center.x, center.y - kSummaryGaugeRadius);
    const D2D1_POINT_2F end = D2D1::Point2F(
        center.x + (std::sin(angle) * kSummaryGaugeRadius),
        center.y - (std::cos(angle) * kSummaryGaugeRadius)
    );
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    if (SUCCEEDED(context.d2d_factory->CreatePathGeometry(geometry.GetAddressOf()))) {
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (SUCCEEDED(geometry->Open(sink.GetAddressOf()))) {
            sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddArc(D2D1::ArcSegment(
                end,
                D2D1::SizeF(kSummaryGaugeRadius, kSummaryGaugeRadius),
                0.0F,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                normalized > 0.5 ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
            ));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            if (SUCCEEDED(sink->Close())) {
                context.render_target->DrawGeometry(
                    geometry.Get(),
                    context.history_curve_brush,
                    kSummaryGaugeStrokeWidth
                );
            }
        }
    }
    context.history_curve_brush->SetOpacity(1.0F);
}

// ----------------------------------------------------------------------------
// Dessine une carte avec valeur, tendance optionnelle et sparkline optionnelle.
//
// Parametres :
// - context : ressources de dessin partagees.
// - rect : rectangle de la carte.
// - label : libelle court de l'indicateur.
// - value : valeur principale deja formatee.
// - trend : comparaison eventuelle avec la periode precedente.
// - series : micro-serie eventuelle a dessiner a droite.
// ----------------------------------------------------------------------------
void DrawStandardSummaryCard(
    const WidgetRenderGraphContext& context,
    const D2D1_RECT_F& rect,
    const std::wstring& label,
    const std::wstring& value,
    const std::optional<std::wstring>& trend,
    const std::vector<std::uint64_t>* series
) {
    DrawSummaryCardBackground(context, rect);
    DrawSummaryText(
        context,
        label,
        D2D1::RectF(
            rect.left + kSummaryCardPadding,
            rect.top + 2.0F,
            rect.right - kSummaryCardPadding,
            rect.top + 19.0F
        ),
        context.caption_text_format,
        context.muted_text_brush
    );
    const bool sparkline_visible = series != nullptr && rect.right - rect.left >= 120.0F;
    const float graphic_left = sparkline_visible ? rect.right - 58.0F : rect.right;
    DrawSummaryText(
        context,
        value,
        D2D1::RectF(
            rect.left + kSummaryCardPadding,
            rect.top + 18.0F,
            graphic_left - (sparkline_visible ? 3.0F : kSummaryCardPadding),
            rect.bottom - 3.0F
        ),
        context.title_text_format,
        context.body_text_brush
    );
    if (trend.has_value()) {
        DrawSummaryText(
            context,
            *trend,
            D2D1::RectF(
                rect.right - 55.0F,
                rect.top + 2.0F,
                rect.right - kSummaryCardPadding,
                rect.top + 19.0F
            ),
            context.caption_text_format,
            context.history_curve_brush,
            DWRITE_TEXT_ALIGNMENT_TRAILING
        );
    }
    if (sparkline_visible) {
        DrawSparkline(
            context,
            *series,
            D2D1::RectF(
                rect.right - 55.0F,
                rect.top + 23.0F,
                rect.right - kSummaryCardPadding,
                rect.bottom - 9.0F
            )
        );
    }
}

// ----------------------------------------------------------------------------
// Dessine l'en-tete du Bilan et son hint explicatif au survol.
//
// Parametres :
// - context : ressources de texte et de dessin.
// - layout : rectangles du titre, de l'aide et du hint.
// - interaction : etat de survol de la bulle d'information.
// ----------------------------------------------------------------------------
void DrawSummaryHeading(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
) {
    DrawSummaryText(
        context,
        T(IDS_GRAPH_SUMMARY_TITLE),
        layout.title_rect,
        context.title_text_format,
        context.body_text_brush
    );
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
    const D2D1_ROUNDED_RECT hint = D2D1::RoundedRect(
        layout.hint_tooltip_rect,
        4.0F,
        4.0F
    );
    context.render_target->FillRoundedRectangle(hint, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(hint, context.border_brush, 1.0F);
    const DWRITE_TEXT_ALIGNMENT previous_alignment = context.caption_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = context.caption_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    const std::wstring text = T(IDS_GRAPH_SUMMARY_SOURCE_HINT);
    context.render_target->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        context.caption_text_format,
        D2D1::RectF(
            layout.hint_tooltip_rect.left + kSummaryHintPadding,
            layout.hint_tooltip_rect.top + kSummaryHintPadding,
            layout.hint_tooltip_rect.right - kSummaryHintPadding,
            layout.hint_tooltip_rect.bottom - kSummaryHintPadding
        ),
        context.body_text_brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(previous_alignment);
    context.caption_text_format->SetParagraphAlignment(previous_paragraph);
    context.caption_text_format->SetWordWrapping(previous_wrapping);
}

// ----------------------------------------------------------------------------
// Dessine un etat centre lorsque les donnees locales ne sont pas exploitables.
//
// Parametres :
// - context : ressources de texte partagees.
// - layout : zone centrale du Bilan.
// - resource_id : identifiant localise du message.
// ----------------------------------------------------------------------------
void DrawSummaryState(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    unsigned int resource_id
) {
    DrawSummaryText(
        context,
        T(resource_id),
        layout.summary_grid_rect,
        context.caption_text_format,
        context.muted_text_brush,
        DWRITE_TEXT_ALIGNMENT_CENTER
    );
}

} // namespace

// ----------------------------------------------------------------------------
// Dessine la courbe compacte de tokens partagee par les differentes vues.
// ----------------------------------------------------------------------------
void DrawWidgetTokenSparkline(
    const WidgetRenderGraphContext& context,
    const std::vector<std::uint64_t>& values,
    const D2D1_RECT_F& rect
) {
    DrawSparkline(context, values, rect);
}

// ----------------------------------------------------------------------------
// Dessine le Bilan de consommation pour la plage selectionnee.
// ----------------------------------------------------------------------------
void DrawWidgetKpiSummary(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenUsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange range
) {
    const WidgetTokenGraphState state = range == GraphRange::Days7
            || range == GraphRange::Days30
        ? ResolveTokenHeatmapState(snapshot, GraphRange::Days30)
        : ResolveHourlyTokenGraphState(snapshot, range);
    if (state != WidgetTokenGraphState::Data) {
        const unsigned int resource_id = state == WidgetTokenGraphState::Loading
            ? IDS_GRAPH_LOADING
            : (state == WidgetTokenGraphState::Error
                ? IDS_GRAPH_TOKENS_UNAVAILABLE
                : (state == WidgetTokenGraphState::NoSessions
                    ? IDS_GRAPH_NO_LOCAL_SESSIONS
                    : IDS_GRAPH_HISTORY_BUILDING));
        DrawSummaryState(context, layout, resource_id);
        DrawSummaryHeading(context, layout, interaction);
        return;
    }

    const TokenKpiSummary summary = BuildTokenKpiSummary(snapshot, range);
    const std::uint64_t total_tokens = summary.current.counts.Total();
    const std::uint64_t previous_tokens = summary.previous.counts.Total();
    const std::uint64_t average_tokens = summary.current.request_count == 0
        ? 0
        : static_cast<std::uint64_t>(std::llround(
            static_cast<double>(total_tokens)
                / static_cast<double>(summary.current.request_count)
        ));
    const double cache_ratio = summary.current.counts.input_tokens == 0
        ? 0.0
        : static_cast<double>(summary.current.counts.cached_input_tokens)
            / static_cast<double>(summary.current.counts.input_tokens);

    DrawStandardSummaryCard(
        context,
        layout.summary_card_rects[0],
        T(IDS_GRAPH_KPI_TOKENS),
        FormatCompactCount(total_tokens),
        FormatTrend(total_tokens, previous_tokens, summary.previous_available),
        &summary.token_series
    );
    DrawStandardSummaryCard(
        context,
        layout.summary_card_rects[1],
        T(IDS_GRAPH_KPI_REQUESTS),
        FormatCompactCount(summary.current.request_count),
        FormatTrend(
            summary.current.request_count,
            summary.previous.request_count,
            summary.previous_available
        ),
        &summary.request_series
    );
    DrawStandardSummaryCard(
        context,
        layout.summary_card_rects[2],
        T(IDS_GRAPH_KPI_AVERAGE),
        FormatCompactCount(average_tokens),
        std::nullopt,
        nullptr
    );

    const D2D1_RECT_F& cache_rect = layout.summary_card_rects[3];
    DrawSummaryCardBackground(context, cache_rect);
    DrawSummaryText(
        context,
        T(IDS_GRAPH_KPI_CACHE),
        D2D1::RectF(
            cache_rect.left + kSummaryCardPadding,
            cache_rect.top + 2.0F,
            cache_rect.right - kSummaryCardPadding,
            cache_rect.top + 19.0F
        ),
        context.caption_text_format,
        context.muted_text_brush
    );
    DrawSummaryText(
        context,
        std::to_wstring(static_cast<int>(std::lround(cache_ratio * 100.0))) + L" %",
        D2D1::RectF(
            cache_rect.left + kSummaryCardPadding,
            cache_rect.top + 18.0F,
            cache_rect.right - 48.0F,
            cache_rect.bottom - 3.0F
        ),
        context.title_text_format,
        context.body_text_brush
    );
    DrawCacheGauge(
        context,
        D2D1::Point2F(
            cache_rect.right - 24.0F,
            cache_rect.top + ((cache_rect.bottom - cache_rect.top) * 0.58F)
        ),
        cache_ratio
    );
    DrawSummaryHeading(context, layout, interaction);
}

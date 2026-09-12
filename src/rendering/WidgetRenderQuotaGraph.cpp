// ============================================================================
// Codex Glass - Rendu du graphe de quotas
// ----------------------------------------------------------------------------
// Ce fichier dessine le quota hebdomadaire restant sur la plage choisie. Il lit
// l'historique SQLite sans modifier son schema, son contenu ni sa cadence.
// ============================================================================

#include "WidgetRenderQuotaGraph.h"

#include "WidgetRenderConstants.h"
#include "WidgetGraphData.h"
#include "WidgetGraphFormatting.h"
#include "WidgetGraphInfoIcon.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace {

// Nombre maximal de samples charges pour couvrir la plage la plus longue.
constexpr int kQuotaHistoryMaximumSamples = 4096;

// Opacite des lignes de grille pointillees.
constexpr float kQuotaGridOpacity = 0.22F;

// Epaisseur de la courbe hebdomadaire restante.
constexpr float kQuotaCurveStrokeWidth = 1.6F;

// Opacite initiale du degrade sous la courbe.
constexpr float kQuotaAreaTopOpacity = 0.14F;

// Opacite intermediaire du degrade sous la courbe.
constexpr float kQuotaAreaMiddleOpacity = 0.04F;

// Rayon du point selectionne sur la courbe.
constexpr float kQuotaSelectedPointRadius = 3.5F;

// Marge interieure uniforme du hint explicatif.
constexpr float kQuotaHintPadding = 7.0F;

// Marge horizontale compacte du tooltip de valeur.
constexpr float kQuotaTooltipHorizontalPadding = 7.0F;

// Marge verticale compacte du tooltip de valeur.
constexpr float kQuotaTooltipVerticalPadding = 3.0F;

// Hauteur des pointes reliant les infobulles a leur cible.
constexpr float kQuotaTooltipPointerHeight = 6.0F;

// Demi-largeur de la base des pointes d'infobulle.
constexpr float kQuotaTooltipPointerHalfWidth = 5.0F;

// Espacement horizontal minimal entre deux dates de l'abscisse.
constexpr float kQuotaMinimumDateTickSpacing = 68.0F;

// ----------------------------------------------------------------------------
// Associe un sample historique a sa position et a sa valeur restante.
// ----------------------------------------------------------------------------
struct QuotaGraphPoint {
    // Date du sample utilisee par le tooltip.
    std::chrono::system_clock::time_point sampled_at{};

    // Position Direct2D si le quota hebdomadaire est disponible.
    std::optional<D2D1_POINT_2F> position;

    // Pourcentage hebdomadaire restant si la source l'a fourni.
    std::optional<double> remaining_percent;
};

// ----------------------------------------------------------------------------
// Dessine un texte avec des alignements temporaires.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - text : texte a dessiner.
// - rect : rectangle de destination.
// - format : format DirectWrite utilise.
// - brush : brosse appliquee au texte.
// - alignment : alignement horizontal temporaire.
// ----------------------------------------------------------------------------
void DrawQuotaText(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& rect,
    IDWriteTextFormat* format,
    ID2D1Brush* brush,
    DWRITE_TEXT_ALIGNMENT alignment
) {
    const DWRITE_TEXT_ALIGNMENT old_alignment = format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT old_paragraph = format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING old_wrapping = format->GetWordWrapping();
    format->SetTextAlignment(alignment);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    context.render_target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), format,
        rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    format->SetTextAlignment(old_alignment);
    format->SetParagraphAlignment(old_paragraph);
    format->SetWordWrapping(old_wrapping);
}

// ----------------------------------------------------------------------------
// Mesure exactement un texte secondaire sans retour a la ligne.
//
// Parametres :
// - context : ressources DirectWrite du graphe.
// - text : texte a mesurer.
//
// Retour :
// - largeur et hauteur du texte, ou des valeurs nulles en cas d'echec.
// ----------------------------------------------------------------------------
D2D1_SIZE_F MeasureQuotaCaption(
    const WidgetRenderGraphContext& context,
    const std::wstring& text
) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(context.dwrite_factory->CreateTextLayout(
            text.c_str(), static_cast<UINT32>(text.size()),
            context.caption_text_format, 1000.0F, 100.0F,
            layout.GetAddressOf()))) {
        return D2D1::SizeF(0.0F, 0.0F);
    }
    DWRITE_TEXT_METRICS metrics{};
    return SUCCEEDED(layout->GetMetrics(&metrics))
        ? D2D1::SizeF(metrics.widthIncludingTrailingWhitespace, metrics.height)
        : D2D1::SizeF(0.0F, 0.0F);
}

// ----------------------------------------------------------------------------
// Mesure un texte secondaire apres retour automatique a la ligne.
//
// Parametres :
// - context : ressources DirectWrite du graphe.
// - text : texte a mesurer.
// - width : largeur maximale disponible sans les marges.
//
// Retour :
// - largeur de la ligne la plus longue et hauteur totale du texte.
// ----------------------------------------------------------------------------
D2D1_SIZE_F MeasureWrappedQuotaCaption(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    float width
) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(context.dwrite_factory->CreateTextLayout(
            text.c_str(), static_cast<UINT32>(text.size()),
            context.caption_text_format, std::max(1.0F, width), 1000.0F,
            layout.GetAddressOf()))) {
        return D2D1::SizeF(0.0F, 0.0F);
    }
    layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    DWRITE_TEXT_METRICS metrics{};
    return SUCCEEDED(layout->GetMetrics(&metrics))
        ? D2D1::SizeF(metrics.width, metrics.height)
        : D2D1::SizeF(0.0F, 0.0F);
}

// ----------------------------------------------------------------------------
// Dessine une pointe triangulaire ouverte sur sa base.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - base_left : extremite gauche de la base.
// - tip : pointe dirigee vers la cible.
// - base_right : extremite droite de la base.
// ----------------------------------------------------------------------------
void DrawQuotaTooltipPointer(
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
    context.render_target->DrawLine(base_left, base_right, context.panel_background_brush, 2.0F);
    context.render_target->DrawLine(base_left, tip, context.border_brush, 1.0F);
    context.render_target->DrawLine(tip, base_right, context.border_brush, 1.0F);
}

// ----------------------------------------------------------------------------
// Dessine une ligne horizontale pointillee.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - left : debut horizontal de la ligne.
// - right : fin horizontale de la ligne.
// - y : position verticale de la ligne.
// ----------------------------------------------------------------------------
void DrawQuotaHorizontalDashes(
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
// Dessine une ligne verticale pointillee pour le sample selectionne.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - top : debut vertical de la ligne.
// - bottom : fin verticale de la ligne.
// - x : position horizontale de la ligne.
// ----------------------------------------------------------------------------
void DrawQuotaVerticalDashes(
    const WidgetRenderGraphContext& context,
    float top,
    float bottom,
    float x
) {
    // Hauteur d'un tiret vertical de la selection.
    constexpr float kDashHeight = 3.0F;
    // Espacement entre deux tirets verticaux de la selection.
    constexpr float kDashGap = 4.0F;
    for (float y = top; y < bottom; y += kDashHeight + kDashGap) {
        context.render_target->DrawLine(
            D2D1::Point2F(x, y),
            D2D1::Point2F(x, std::min(y + kDashHeight, bottom)),
            context.history_curve_brush,
            0.8F
        );
    }
}

// ----------------------------------------------------------------------------
// Dessine les axes fixes de pourcentage et les dates espacees.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - layout : geometrie partagee de la vue.
// - range_start : debut de la fenetre choisie.
// - range_end : fin de la fenetre choisie.
// - graph_range : plage qui determine le format de l'abscisse.
// ----------------------------------------------------------------------------
void DrawQuotaAxes(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    std::chrono::system_clock::time_point range_start,
    std::chrono::system_clock::time_point range_end,
    GraphRange graph_range
) {
    for (std::size_t index = 0; index < kWidgetGraphYAxisTickCount; ++index) {
        const int percent = 100 - (static_cast<int>(index) * 25);
        const float y = layout.y_tick_positions[index];
        context.muted_text_brush->SetOpacity(kQuotaGridOpacity);
        DrawQuotaHorizontalDashes(context, layout.plot_rect.left, layout.plot_rect.right, y);
        context.muted_text_brush->SetOpacity(1.0F);
        DrawQuotaText(
            context,
            std::to_wstring(percent) + L" %",
            D2D1::RectF(layout.y_axis_rect.left, y - 8.0F, layout.y_axis_rect.right - 3.0F, y + 8.0F),
            context.caption_text_format,
            context.muted_text_brush,
            DWRITE_TEXT_ALIGNMENT_CENTER
        );
    }
    context.render_target->DrawLine(
        D2D1::Point2F(layout.plot_rect.left, layout.plot_rect.bottom),
        D2D1::Point2F(layout.plot_rect.right, layout.plot_rect.bottom),
        context.muted_text_brush,
        0.8F
    );

    const float plot_width = std::max(0.0F, layout.plot_rect.right - layout.plot_rect.left);
    const std::size_t tick_count = std::clamp<std::size_t>(
        static_cast<std::size_t>(plot_width / kQuotaMinimumDateTickSpacing) + 1U,
        2U,
        kWidgetGraphXAxisTickCount
    );
    const float spacing = plot_width / static_cast<float>(tick_count - 1U);
    const float half_width = std::max(1.0F, (spacing * 0.5F) - 3.0F);
    const auto duration = range_end - range_start;
    for (std::size_t index = 0; index < tick_count; ++index) {
        const double ratio = static_cast<double>(index) / static_cast<double>(tick_count - 1U);
        const auto sample_time = range_start
            + std::chrono::duration_cast<std::chrono::system_clock::duration>(duration * ratio);
        const float center = layout.plot_rect.left + (spacing * static_cast<float>(index));
        DrawQuotaText(
            context,
            FormatQuotaAxisTime(sample_time, graph_range),
            D2D1::RectF(
                std::max(layout.x_axis_rect.left, center - half_width),
                layout.x_axis_rect.top,
                std::min(layout.x_axis_rect.right, center + half_width),
                layout.x_axis_rect.bottom
            ),
            context.caption_text_format,
            context.muted_text_brush,
            DWRITE_TEXT_ALIGNMENT_CENTER
        );
    }
}

// ----------------------------------------------------------------------------
// Convertit les samples SQLite en points de graphe ordonnes.
//
// Parametres :
// - samples : samples historiques charges depuis SQLite.
// - range_start : debut de la fenetre temporelle.
// - range_end : fin de la fenetre temporelle.
// - plot_rect : rectangle de tracage en DIPs.
//
// Retour :
// - serie limitee a la fenetre visible et conservant les valeurs absentes pour
//   representer les trous.
// ----------------------------------------------------------------------------
std::vector<QuotaGraphPoint> BuildQuotaPoints(
    const std::vector<UsageHistorySample>& samples,
    std::chrono::system_clock::time_point range_start,
    std::chrono::system_clock::time_point range_end,
    const D2D1_RECT_F& plot_rect
) {
    std::vector<QuotaGraphPoint> points;
    const std::vector<QuotaGraphSample> series = BuildQuotaGraphSeries(samples);
    points.reserve(series.size());
    const double range_seconds = std::max(
        1.0,
        std::chrono::duration<double>(range_end - range_start).count()
    );
    for (const QuotaGraphSample& sample : series) {
        if (sample.sampled_at > range_end) {
            continue;
        }

        QuotaGraphPoint point{};
        point.sampled_at = sample.sampled_at;
        if (sample.remaining_percent.has_value()) {
            point.remaining_percent = sample.remaining_percent;
            const double time_ratio = std::clamp(
                std::chrono::duration<double>(sample.sampled_at - range_start).count() / range_seconds,
                0.0,
                1.0
            );
            const float x = plot_rect.left
                + (static_cast<float>(time_ratio) * (plot_rect.right - plot_rect.left));
            const float y = plot_rect.bottom
                - (static_cast<float>(*point.remaining_percent / 100.0)
                    * (plot_rect.bottom - plot_rect.top));
            point.position = D2D1::Point2F(x, y);
        }
        points.push_back(point);
    }
    return points;
}

// ----------------------------------------------------------------------------
// Ajoute le snapshot courant a la serie affichee s'il n'est pas deja persiste.
//
// Parametres :
// - samples : serie SQLite a completer uniquement pour le rendu courant.
// - snapshot : dernier releve fourni par le provider.
//
// Effets de bord :
// - ajoute au plus un sample en memoire sans ecrire dans SQLite.
// ----------------------------------------------------------------------------
void AppendCurrentQuotaSnapshot(
    std::vector<UsageHistorySample>& samples,
    const UsageSnapshot& snapshot
) {
    if (!snapshot.weekly_available || snapshot.sampled_at.time_since_epoch().count() == 0) {
        return;
    }

    const auto snapshot_second = std::chrono::duration_cast<std::chrono::seconds>(
        snapshot.sampled_at.time_since_epoch()
    );
    const bool already_present = std::any_of(
        samples.begin(),
        samples.end(),
        [snapshot_second](const UsageHistorySample& sample) {
            return std::chrono::duration_cast<std::chrono::seconds>(
                sample.sampled_at.time_since_epoch()
            ) == snapshot_second;
        }
    );
    if (already_present) {
        return;
    }

    UsageHistorySample current{};
    current.sampled_at = snapshot.sampled_at;
    current.five_hour_used_percent = snapshot.five_hour_available
        ? std::optional<double>{snapshot.five_hour_used_percent}
        : std::nullopt;
    current.weekly_used_percent = snapshot.weekly_used_percent;
    if (snapshot.five_hour_reset_at.time_since_epoch().count() != 0) {
        current.five_hour_reset_at = snapshot.five_hour_reset_at;
    }
    if (snapshot.weekly_reset_at.time_since_epoch().count() != 0) {
        current.weekly_reset_at = snapshot.weekly_reset_at;
    }
    samples.push_back(current);
}

// ----------------------------------------------------------------------------
// Regroupe les points disponibles en segments contigus.
//
// Parametres :
// - points : serie comprenant les valeurs absentes.
//
// Retour :
// - segments pouvant etre relies sans traverser un trou.
// ----------------------------------------------------------------------------
std::vector<std::vector<D2D1_POINT_2F>> BuildQuotaRuns(
    const std::vector<QuotaGraphPoint>& points
) {
    std::vector<std::vector<D2D1_POINT_2F>> runs;
    std::vector<D2D1_POINT_2F> current;
    for (const QuotaGraphPoint& point : points) {
        if (point.position.has_value()) {
            current.push_back(*point.position);
        } else if (!current.empty()) {
            runs.push_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        runs.push_back(std::move(current));
    }
    return runs;
}

// ----------------------------------------------------------------------------
// Ajoute une interpolation douce entre les points d'un segment.
//
// Parametres :
// - sink : sink Direct2D recevant les courbes de Bezier.
// - run : segment contigu a convertir.
// - plot_rect : limites globales de la zone de tracage.
// ----------------------------------------------------------------------------
void AddSmoothQuotaSegments(
    ID2D1GeometrySink* sink,
    const std::vector<D2D1_POINT_2F>& run,
    const D2D1_RECT_F& plot_rect
) {
    for (std::size_t index = 0; index + 1U < run.size(); ++index) {
        const D2D1_POINT_2F previous = index > 0U ? run[index - 1U] : run[index];
        const D2D1_POINT_2F current = run[index];
        const D2D1_POINT_2F next = run[index + 1U];
        const D2D1_POINT_2F following = index + 2U < run.size() ? run[index + 2U] : next;
        D2D1_POINT_2F control_one = D2D1::Point2F(
            current.x + ((next.x - previous.x) / 6.0F),
            current.y + ((next.y - previous.y) / 6.0F)
        );
        D2D1_POINT_2F control_two = D2D1::Point2F(
            next.x - ((following.x - current.x) / 6.0F),
            next.y - ((following.y - current.y) / 6.0F)
        );
        const float segment_left = std::max(plot_rect.left, std::min(current.x, next.x));
        const float segment_right = std::min(plot_rect.right, std::max(current.x, next.x));
        const float segment_top = std::max(plot_rect.top, std::min(current.y, next.y));
        const float segment_bottom = std::min(plot_rect.bottom, std::max(current.y, next.y));
        control_one.x = std::clamp(control_one.x, segment_left, segment_right);
        control_two.x = std::clamp(control_two.x, segment_left, segment_right);
        control_one.y = std::clamp(control_one.y, segment_top, segment_bottom);
        control_two.y = std::clamp(control_two.y, segment_top, segment_bottom);
        sink->AddBezier(D2D1::BezierSegment(control_one, control_two, next));
    }
}

// ----------------------------------------------------------------------------
// Cree les geometries de ligne et de remplissage des segments disponibles.
//
// Parametres :
// - context : factory Direct2D du graphe.
// - runs : segments contigus a dessiner.
// - plot_rect : rectangle et ligne de base du remplissage.
// - line_geometry : recoit la geometrie ouverte de la courbe.
// - fill_geometry : recoit la geometrie fermee sous la courbe.
//
// Retour :
// - true si les deux geometries ont ete construites.
// ----------------------------------------------------------------------------
bool CreateQuotaGeometries(
    const WidgetRenderGraphContext& context,
    const std::vector<std::vector<D2D1_POINT_2F>>& runs,
    const D2D1_RECT_F& plot_rect,
    Microsoft::WRL::ComPtr<ID2D1PathGeometry>& line_geometry,
    Microsoft::WRL::ComPtr<ID2D1PathGeometry>& fill_geometry
) {
    if (FAILED(context.d2d_factory->CreatePathGeometry(line_geometry.GetAddressOf()))
        || FAILED(context.d2d_factory->CreatePathGeometry(fill_geometry.GetAddressOf()))) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> line_sink;
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> fill_sink;
    if (FAILED(line_geometry->Open(line_sink.GetAddressOf()))
        || FAILED(fill_geometry->Open(fill_sink.GetAddressOf()))) {
        return false;
    }
    for (const std::vector<D2D1_POINT_2F>& run : runs) {
        if (run.size() < 2U) {
            continue;
        }
        line_sink->BeginFigure(run.front(), D2D1_FIGURE_BEGIN_HOLLOW);
        AddSmoothQuotaSegments(line_sink.Get(), run, plot_rect);
        line_sink->EndFigure(D2D1_FIGURE_END_OPEN);

        fill_sink->BeginFigure(D2D1::Point2F(run.front().x, plot_rect.bottom), D2D1_FIGURE_BEGIN_FILLED);
        fill_sink->AddLine(run.front());
        AddSmoothQuotaSegments(fill_sink.Get(), run, plot_rect);
        fill_sink->AddLine(D2D1::Point2F(run.back().x, plot_rect.bottom));
        fill_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    }
    return SUCCEEDED(line_sink->Close()) && SUCCEEDED(fill_sink->Close());
}

// ----------------------------------------------------------------------------
// Retourne le sample disponible le plus proche horizontalement du pointeur.
//
// Parametres :
// - points : serie historique dessinee.
// - pointer : position souris dans le tracage.
//
// Retour :
// - index du point le plus proche, ou aucune valeur si la serie est vide.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestQuotaPoint(
    const std::vector<QuotaGraphPoint>& points,
    D2D1_POINT_2F pointer
) {
    std::optional<std::size_t> selected;
    float best_distance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (!points[index].position.has_value()) {
            continue;
        }
        const float distance = std::abs(points[index].position->x - pointer.x);
        if (distance <= best_distance) {
            best_distance = distance;
            selected = index;
        }
    }
    return selected;
}

// ----------------------------------------------------------------------------
// Dessine le tooltip compact du sample Quotas selectionne.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - layout : geometrie partagee de la vue.
// - point : sample selectionne et sa position.
// - graph_range : plage qui determine la precision temporelle du texte.
// ----------------------------------------------------------------------------
void DrawQuotaTooltip(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const QuotaGraphPoint& point,
    GraphRange graph_range
) {
    if (!point.position.has_value() || !point.remaining_percent.has_value()) {
        return;
    }
    const std::wstring text = FormatQuotaTooltipTime(point.sampled_at, graph_range) + L" \267 "
        + FormatGraphPercent(*point.remaining_percent);
    const D2D1_SIZE_F text_size = MeasureQuotaCaption(context, text);
    const float width = std::min(
        std::max(1.0F, layout.plot_rect.right - layout.plot_rect.left),
        text_size.width + (kQuotaTooltipHorizontalPadding * 2.0F)
    );
    const float height = text_size.height + (kQuotaTooltipVerticalPadding * 2.0F);
    const float left = std::clamp(
        point.position->x - (width * 0.5F),
        layout.plot_rect.left,
        layout.plot_rect.right - width
    );
    const float bottom = std::max(
        layout.plot_rect.top + height + 2.0F,
        point.position->y - kQuotaTooltipPointerHeight - 2.0F
    );
    const D2D1_RECT_F rect = D2D1::RectF(left, bottom - height, left + width, bottom);
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 3.0F, 3.0F);
    context.render_target->FillRoundedRectangle(rounded, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(rounded, context.border_brush, 1.0F);
    const float pointer_x = std::clamp(point.position->x, rect.left + 9.0F, rect.right - 9.0F);
    DrawQuotaTooltipPointer(
        context,
        D2D1::Point2F(pointer_x - kQuotaTooltipPointerHalfWidth, rect.bottom),
        D2D1::Point2F(pointer_x, rect.bottom + kQuotaTooltipPointerHeight),
        D2D1::Point2F(pointer_x + kQuotaTooltipPointerHalfWidth, rect.bottom)
    );
    DrawQuotaText(
        context,
        text,
        D2D1::RectF(
            rect.left + kQuotaTooltipHorizontalPadding,
            rect.top + kQuotaTooltipVerticalPadding,
            rect.right - kQuotaTooltipHorizontalPadding,
            rect.bottom - kQuotaTooltipVerticalPadding
        ),
        context.caption_text_format,
        context.body_text_brush,
        DWRITE_TEXT_ALIGNMENT_CENTER
    );
}

// ----------------------------------------------------------------------------
// Dessine le titre Quotas, la bulle d'information et son hint.
//
// Parametres :
// - context : ressources de rendu du graphe.
// - layout : geometrie partagee de la vue.
// - interaction : etat de survol de la bulle.
// ----------------------------------------------------------------------------
void DrawQuotaHeading(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
) {
    DrawQuotaText(
        context, T(IDS_GRAPH_TAB_QUOTAS), layout.title_rect,
        context.title_text_format, context.body_text_brush,
        DWRITE_TEXT_ALIGNMENT_LEADING
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

    const std::wstring hint = T(IDS_GRAPH_QUOTAS_SOURCE_HINT);
    D2D1_RECT_F hint_rect = layout.hint_tooltip_rect;
    const D2D1_SIZE_F hint_size = MeasureWrappedQuotaCaption(
        context, hint,
        (hint_rect.right - hint_rect.left) - (kQuotaHintPadding * 2.0F)
    );
    if (hint_size.width > 0.0F && hint_size.height > 0.0F) {
        hint_rect.right = std::min(hint_rect.right, hint_rect.left + std::ceil(hint_size.width) + (kQuotaHintPadding * 2.0F));
        hint_rect.bottom = std::min(hint_rect.bottom, hint_rect.top + hint_size.height + (kQuotaHintPadding * 2.0F));
    }
    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(hint_rect, 4.0F, 4.0F);
    context.render_target->FillRoundedRectangle(rounded, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(rounded, context.border_brush, 1.0F);
    const float help_center_x = (
        layout.help_button_rect.left + layout.help_button_rect.right
    ) * 0.5F;
    const float pointer_x = std::clamp(
        help_center_x,
        hint_rect.left + 10.0F,
        hint_rect.right - 10.0F
    );
    DrawQuotaTooltipPointer(
        context,
        D2D1::Point2F(pointer_x - kQuotaTooltipPointerHalfWidth, hint_rect.top),
        D2D1::Point2F(pointer_x, hint_rect.top - kQuotaTooltipPointerHeight),
        D2D1::Point2F(pointer_x + kQuotaTooltipPointerHalfWidth, hint_rect.top)
    );

    const DWRITE_TEXT_ALIGNMENT old_alignment = context.caption_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT old_paragraph = context.caption_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING old_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    context.render_target->DrawTextW(
        hint.c_str(), static_cast<UINT32>(hint.size()), context.caption_text_format,
        D2D1::RectF(
            hint_rect.left + kQuotaHintPadding,
            hint_rect.top + kQuotaHintPadding,
            hint_rect.right - kQuotaHintPadding,
            hint_rect.bottom - kQuotaHintPadding
        ),
        context.body_text_brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(old_alignment);
    context.caption_text_format->SetParagraphAlignment(old_paragraph);
    context.caption_text_format->SetWordWrapping(old_wrapping);
}

// ----------------------------------------------------------------------------
// Dessine une serie Quotas avec son remplissage et une opacite partagee.
//
// Parametres :
// - context : ressources Direct2D du graphe.
// - layout : geometrie fixe de la zone de tracage.
// - runs : segments contigus a dessiner.
// - opacity : opacite appliquee a la ligne et au remplissage.
// ----------------------------------------------------------------------------
void DrawQuotaCurve(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const std::vector<std::vector<D2D1_POINT_2F>>& runs,
    float opacity
) {
    const float clamped_opacity = std::clamp(opacity, 0.0F, 1.0F);
    if (clamped_opacity <= 0.0F) {
        return;
    }

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> line_geometry;
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> fill_geometry;
    if (CreateQuotaGeometries(context, runs, layout.plot_rect, line_geometry, fill_geometry)) {
        const D2D1_COLOR_F accent = context.history_curve_brush->GetColor();
        const std::array<D2D1_GRADIENT_STOP, 3> stops = {
            D2D1::GradientStop(0.0F, D2D1::ColorF(
                accent.r, accent.g, accent.b, kQuotaAreaTopOpacity * clamped_opacity
            )),
            D2D1::GradientStop(0.58F, D2D1::ColorF(
                accent.r, accent.g, accent.b, kQuotaAreaMiddleOpacity * clamped_opacity
            )),
            D2D1::GradientStop(1.0F, D2D1::ColorF(accent.r, accent.g, accent.b, 0.0F)),
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stop_collection;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> area_brush;
        if (SUCCEEDED(context.render_target->CreateGradientStopCollection(
                stops.data(), static_cast<UINT32>(stops.size()), stop_collection.GetAddressOf()))
            && SUCCEEDED(context.render_target->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(0.0F, layout.plot_rect.top),
                    D2D1::Point2F(0.0F, layout.plot_rect.bottom)
                ),
                stop_collection.Get(),
                area_brush.GetAddressOf()))) {
            context.render_target->FillGeometry(fill_geometry.Get(), area_brush.Get());
        }
        context.history_curve_brush->SetOpacity(clamped_opacity);
        context.render_target->DrawGeometry(
            line_geometry.Get(),
            context.history_curve_brush,
            kQuotaCurveStrokeWidth
        );
        context.history_curve_brush->SetOpacity(1.0F);
    }
    for (const std::vector<D2D1_POINT_2F>& run : runs) {
        if (run.size() == 1U) {
            context.history_curve_brush->SetOpacity(clamped_opacity);
            context.render_target->FillEllipse(
                D2D1::Ellipse(run.front(), 1.5F, 1.5F),
                context.history_curve_brush
            );
            context.history_curve_brush->SetOpacity(1.0F);
        }
    }
}

} // namespace

// ----------------------------------------------------------------------------
// Dessine la page Quotas sur la plage choisie et ses interactions.
//
// Parametres :
// - context : ressources Direct2D et DirectWrite du graphe.
// - layout : geometrie partagee avec la vue Tokens.
// - history_store : stockage SQLite lu sur la fenetre demandee.
// - snapshot : dernier etat du provider Codex.
// - interaction : position survolee et etat de la bulle d'information.
// - graph_range : plage temporelle choisie dans le menu.
// - animation : gel ou translation applique a la serie courante.
// ----------------------------------------------------------------------------
void DrawQuotaGraph(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const UsageHistoryStore& history_store,
    const UsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange graph_range,
    const WidgetQuotaGraphAnimationFrame& animation
) {
    const auto draw_state = [&context, &layout, &interaction](unsigned int resource_id) {
        DrawQuotaText(
            context, T(resource_id), layout.plot_rect,
            context.caption_text_format, context.muted_text_brush,
            DWRITE_TEXT_ALIGNMENT_CENTER
        );
        DrawQuotaHeading(context, layout, interaction);
    };
    if (!history_store.IsOpen()) {
        draw_state(IDS_GRAPH_HISTORY_UNAVAILABLE);
        return;
    }

    const auto range_end = animation.range_end.value_or(
        snapshot.sampled_at.time_since_epoch().count() != 0
            ? snapshot.sampled_at
            : std::chrono::system_clock::now()
    );
    const auto range_start = range_end - GraphRangeDuration(graph_range);
    DrawQuotaAxes(context, layout, range_start, range_end, graph_range);

    std::vector<UsageHistorySample> full_samples = history_store.LoadSamplesForRange(
        range_start,
        kQuotaHistoryMaximumSamples
    );
    AppendCurrentQuotaSnapshot(full_samples, snapshot);
    std::vector<UsageHistorySample> samples = full_samples;
    if (animation.maximum_sampled_at.has_value() && !animation.active) {
        samples.erase(
            std::remove_if(
                samples.begin(),
                samples.end(),
                [&animation](const UsageHistorySample& sample) {
                    return sample.sampled_at > *animation.maximum_sampled_at;
                }
            ),
            samples.end()
        );
    }
    const std::vector<QuotaGraphPoint> points = BuildQuotaPoints(
        samples,
        range_start,
        range_end,
        layout.plot_rect
    );
    const std::vector<std::vector<D2D1_POINT_2F>> runs = BuildQuotaRuns(points);
    const std::size_t available_point_count = static_cast<std::size_t>(std::count_if(
        points.begin(), points.end(), [](const QuotaGraphPoint& point) {
            return point.position.has_value();
        }
    ));
    if (available_point_count < 2U) {
        if (snapshot.freshness == UsageFreshness::Refreshing) {
            draw_state(IDS_GRAPH_LOADING);
        } else if (snapshot.freshness == UsageFreshness::Error) {
            draw_state(IDS_GRAPH_QUOTAS_UNAVAILABLE);
        } else if (!snapshot.weekly_available) {
            draw_state(IDS_GRAPH_VALUE_UNAVAILABLE);
        } else {
            draw_state(IDS_GRAPH_HISTORY_BUILDING);
        }
        return;
    }

    context.render_target->PushAxisAlignedClip(
        layout.plot_rect,
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
    );
    DrawQuotaCurve(context, layout, runs, 1.0F);
    context.render_target->PopAxisAlignedClip();

    if (!interaction.hovered_graph_info && interaction.hovered_quota_plot_point.has_value()) {
        const std::optional<std::size_t> selected = HitTestQuotaPoint(points, *interaction.hovered_quota_plot_point);
        if (selected.has_value() && points[*selected].position.has_value()) {
            context.history_curve_brush->SetOpacity(0.55F);
            DrawQuotaVerticalDashes(context, layout.plot_rect.top, layout.plot_rect.bottom, points[*selected].position->x);
            context.history_curve_brush->SetOpacity(1.0F);
            context.render_target->FillEllipse(
                D2D1::Ellipse(*points[*selected].position, kQuotaSelectedPointRadius, kQuotaSelectedPointRadius),
                context.history_curve_brush
            );
            DrawQuotaTooltip(context, layout, points[*selected], graph_range);
        }
    }
    context.history_curve_brush->SetOpacity(1.0F);
    DrawQuotaHeading(context, layout, interaction);
}

// ============================================================================
// Codex Glass - Rendu de la heatmap quotidienne de tokens
// ----------------------------------------------------------------------------
// Ce fichier dessine une grille lundi-dimanche, ses mois, sa legende derivee
// du theme et ses tooltips. Les calculs calendaires restent dans tokens/.
// ============================================================================

#include "WidgetRenderTokenHeatmap.h"

#include "WidgetGraphFormatting.h"
#include "WidgetGraphInfoIcon.h"
#include "WidgetTokenGraphState.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../tokens/TokenHeatmapData.h"

#include <algorithm>
#include <array>
#include <wrl/client.h>

namespace {

// Marge interne uniforme des hints et tooltips en DIPs.
constexpr float kHeatmapTooltipPadding = 6.0F;

// Largeur maximale du texte du hint d'information en DIPs.
constexpr float kHeatmapHintMaximumTextWidth = 230.0F;

// Distance minimale entre deux libelles de mois en DIPs.
constexpr float kHeatmapMonthMinimumSpacing = 34.0F;

// Taille des cellules de la legende en DIPs.
constexpr float kHeatmapLegendCellSize = 8.0F;

// Espace entre les cellules de la legende en DIPs.
constexpr float kHeatmapLegendCellGap = 3.0F;

// Demi-largeur de la base ouverte d'une pointe de tooltip en DIPs.
constexpr float kHeatmapTooltipPointerHalfWidth = 5.0F;

// ----------------------------------------------------------------------------
// Dessine une pointe triangulaire dont la base reste ouverte sur le tooltip.
//
// Parametres :
// - context : factory et cible Direct2D courantes.
// - base_center : centre de la base collee au tooltip.
// - tip : pointe dirigee vers la cellule ou le bouton d'information.
// ----------------------------------------------------------------------------
void DrawHeatmapTooltipPointer(
    const WidgetRenderGraphContext& context,
    D2D1_POINT_2F base_center,
    D2D1_POINT_2F tip
) {
    const D2D1_POINT_2F base_left = D2D1::Point2F(
        base_center.x - kHeatmapTooltipPointerHalfWidth,
        base_center.y
    );
    const D2D1_POINT_2F base_right = D2D1::Point2F(
        base_center.x + kHeatmapTooltipPointerHalfWidth,
        base_center.y
    );
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
    context.render_target->DrawLine(base_left, tip, context.border_brush, 1.0F);
    context.render_target->DrawLine(tip, base_right, context.border_brush, 1.0F);
}

// ----------------------------------------------------------------------------
// Melange deux couleurs en conservant l'opacite de la couleur d'accent.
//
// Parametres :
// - background : couleur du panneau servant de base.
// - accent : couleur personnalisee du theme.
// - ratio : part de couleur d'accent comprise entre zero et un.
//
// Retour :
// - couleur interpolee assez opaque pour rester lisible sur le verre.
// ----------------------------------------------------------------------------
D2D1_COLOR_F MixHeatmapColor(
    const D2D1_COLOR_F& background,
    const D2D1_COLOR_F& accent,
    float ratio
) {
    const float safe_ratio = std::clamp(ratio, 0.0F, 1.0F);
    return D2D1::ColorF(
        background.r + ((accent.r - background.r) * safe_ratio),
        background.g + ((accent.g - background.g) * safe_ratio),
        background.b + ((accent.b - background.b) * safe_ratio),
        accent.a
    );
}

// ----------------------------------------------------------------------------
// Dessine un texte secondaire centre sans conserver l'alignement temporaire.
//
// Parametres :
// - context : ressources de rendu.
// - text : texte a dessiner.
// - rect : rectangle de destination.
// - brush : brosse du texte.
// ----------------------------------------------------------------------------
void DrawCenteredCaption(
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

// ----------------------------------------------------------------------------
// Dessine un texte aligne a gauche avec le format de titre commun.
//
// Parametres :
// - context : ressources de rendu.
// - text : texte a dessiner.
// - rect : rectangle de destination.
// ----------------------------------------------------------------------------
void DrawLeadingTitle(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& rect
) {
    const auto old_text = context.title_text_format->GetTextAlignment();
    const auto old_paragraph = context.title_text_format->GetParagraphAlignment();
    context.title_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.title_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    context.render_target->DrawTextW(
        text.c_str(), static_cast<UINT32>(text.size()), context.title_text_format,
        rect, context.body_text_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.title_text_format->SetTextAlignment(old_text);
    context.title_text_format->SetParagraphAlignment(old_paragraph);
}

// ----------------------------------------------------------------------------
// Mesure un texte secondaire pour ajuster une infobulle compacte.
//
// Parametres :
// - context : factory et format DirectWrite.
// - text : texte a mesurer.
// - maximum_width : largeur maximale autorisee.
//
// Retour :
// - largeur et hauteur du texte mis en page.
// ----------------------------------------------------------------------------
D2D1_SIZE_F MeasureCaption(
    const WidgetRenderGraphContext& context,
    const std::wstring& text,
    float maximum_width
) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
    if (FAILED(context.dwrite_factory->CreateTextLayout(
            text.c_str(), static_cast<UINT32>(text.size()), context.caption_text_format,
            std::max(1.0F, maximum_width), 1000.0F, text_layout.GetAddressOf()))) {
        return D2D1::SizeF();
    }
    text_layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    DWRITE_TEXT_METRICS metrics{};
    return SUCCEEDED(text_layout->GetMetrics(&metrics))
        ? D2D1::SizeF(metrics.width, metrics.height)
        : D2D1::SizeF();
}

// ----------------------------------------------------------------------------
// Dessine le titre Activite et sa bulle d'information.
//
// Parametres :
// - context : ressources de rendu.
// - layout : geometrie partagee.
// - interaction : etat de survol de la bulle.
// ----------------------------------------------------------------------------
void DrawHeatmapHeading(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
) {
    DrawLeadingTitle(context, T(IDS_GRAPH_ACTIVITY_TITLE), layout.title_rect);
    if (context.show_header_controls) {
        context.muted_text_brush->SetOpacity(interaction.hovered_graph_info ? 1.0F : 0.72F);
        DrawWidgetGraphInfoIcon(
            context.render_target,
            context.muted_text_brush,
            layout.help_button_rect
        );
        context.muted_text_brush->SetOpacity(1.0F);
    }
}

// ----------------------------------------------------------------------------
// Dessine le hint d'information au-dessus de tous les elements de la heatmap.
//
// Parametres :
// - context : ressources de rendu.
// - layout : geometrie partagee.
// - interaction : etat de survol de la bulle.
// ----------------------------------------------------------------------------
void DrawHeatmapInformationHint(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
) {
    if (!interaction.hovered_graph_info) {
        return;
    }
    const D2D1_POINT_2F center = D2D1::Point2F(
        (layout.help_button_rect.left + layout.help_button_rect.right) * 0.5F,
        (layout.help_button_rect.top + layout.help_button_rect.bottom) * 0.5F
    );
    const std::wstring hint = T(IDS_GRAPH_ACTIVITY_SOURCE_HINT);
    D2D1_RECT_F hint_rect = layout.hint_tooltip_rect;
    const float maximum_text_width = std::min(
        kHeatmapHintMaximumTextWidth,
        std::max(
            1.0F,
            (hint_rect.right - hint_rect.left) - (2.0F * kHeatmapTooltipPadding)
        )
    );
    const D2D1_SIZE_F text_size = MeasureCaption(
        context,
        hint,
        maximum_text_width
    );
    hint_rect.right = std::min(
        hint_rect.right,
        hint_rect.left + text_size.width + (2.0F * kHeatmapTooltipPadding)
    );
    hint_rect.bottom = std::min(
        hint_rect.bottom,
        hint_rect.top + text_size.height + (2.0F * kHeatmapTooltipPadding)
    );
    const auto rounded = D2D1::RoundedRect(hint_rect, 4.0F, 4.0F);
    context.render_target->FillRoundedRectangle(rounded, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(rounded, context.border_brush, 1.0F);
    DrawHeatmapTooltipPointer(
        context,
        D2D1::Point2F(
            std::clamp(center.x, hint_rect.left + 10.0F, hint_rect.right - 10.0F),
            hint_rect.top
        ),
        D2D1::Point2F(center.x, center.y + kWidgetGraphInfoRadius)
    );
    const D2D1_RECT_F text_rect = D2D1::RectF(
        hint_rect.left + kHeatmapTooltipPadding,
        hint_rect.top + kHeatmapTooltipPadding,
        hint_rect.right - kHeatmapTooltipPadding,
        hint_rect.bottom - kHeatmapTooltipPadding
    );
    const auto old_text = context.caption_text_format->GetTextAlignment();
    const auto old_paragraph = context.caption_text_format->GetParagraphAlignment();
    const auto old_wrapping = context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    context.render_target->DrawTextW(
        hint.c_str(), static_cast<UINT32>(hint.size()), context.caption_text_format,
        text_rect, context.body_text_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(old_text);
    context.caption_text_format->SetParagraphAlignment(old_paragraph);
    context.caption_text_format->SetWordWrapping(old_wrapping);
}

// ----------------------------------------------------------------------------
// Dessine les libelles mensuels sans collision horizontale.
//
// Parametres :
// - context : ressources de rendu.
// - layout : geometrie de la grille.
// - cells : cellules calendaires visibles.
// ----------------------------------------------------------------------------
void DrawHeatmapMonths(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const std::vector<TokenHeatmapCell>& cells
) {
    float previous_right = layout.heatmap_months_rect.left - kHeatmapMonthMinimumSpacing;
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const TokenHeatmapCell& cell = cells[index];
        if (!cell.present || cell.date_key.size() != 10 || cell.date_key.substr(8, 2) != "01") {
            continue;
        }
        const D2D1_RECT_F& cell_rect = layout.heatmap_cell_rects[index];
        if (cell_rect.left - previous_right < kHeatmapMonthMinimumSpacing) {
            continue;
        }
        std::wstring label = FormatGraphDateKey(cell.date_key);
        const std::size_t space = label.find(L' ');
        if (space != std::wstring::npos) {
            label.erase(0, space + 1);
        }
        const D2D1_RECT_F rect = D2D1::RectF(
            cell_rect.left,
            layout.heatmap_months_rect.top,
            std::min(layout.heatmap_months_rect.right, cell_rect.left + 42.0F),
            layout.heatmap_months_rect.bottom
        );
        DrawCenteredCaption(context, label, rect, context.muted_text_brush);
        previous_right = rect.right;
    }
}

// ----------------------------------------------------------------------------
// Dessine trois reperes horaires pour la grille en tranches de cinq minutes.
// ----------------------------------------------------------------------------
void DrawFiveMinuteTimeLabels(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const std::vector<TokenHeatmapCell>& cells
) {
    if (cells.empty() || layout.heatmap_cell_rects.size() != cells.size()) {
        return;
    }
    for (const std::size_t index : {std::size_t{0}, cells.size() / 2U, cells.size() - 1U}) {
        if (!cells[index].present) {
            continue;
        }
        const D2D1_RECT_F& cell = layout.heatmap_cell_rects[index];
        DrawCenteredCaption(
            context,
            FormatTokenBucketAxis(cells[index].bucket_start, GraphRange::Minutes5),
            D2D1::RectF(
                std::max(layout.heatmap_months_rect.left, cell.left - 20.0F),
                layout.heatmap_months_rect.top,
                std::min(layout.heatmap_months_rect.right, cell.right + 20.0F),
                layout.heatmap_months_rect.bottom
            ),
            context.muted_text_brush
        );
    }
}

// ----------------------------------------------------------------------------
// Dessine la legende Moins/Plus et les cinq niveaux du theme.
//
// Parametres :
// - context : ressources de rendu et couleurs du theme.
// - layout : zone reservee a la legende.
// - brushes : brosses des quatre niveaux non nuls.
// ----------------------------------------------------------------------------
void DrawHeatmapLegend(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, 4>& brushes
) {
    const std::wstring less = T(IDS_GRAPH_ACTIVITY_LESS);
    const std::wstring more = T(IDS_GRAPH_ACTIVITY_MORE);
    const float less_width = MeasureCaption(context, less, 80.0F).width;
    const float more_width = MeasureCaption(context, more, 80.0F).width;
    const float cells_width = (5.0F * kHeatmapLegendCellSize) + (4.0F * kHeatmapLegendCellGap);
    const float total_width = less_width + 5.0F + cells_width + 5.0F + more_width;
    float left = std::max(layout.heatmap_legend_rect.left, layout.heatmap_legend_rect.right - total_width);
    DrawCenteredCaption(
        context,
        less,
        D2D1::RectF(left, layout.heatmap_legend_rect.top, left + less_width, layout.heatmap_legend_rect.bottom),
        context.muted_text_brush
    );
    left += less_width + 5.0F;
    const float top = layout.heatmap_legend_rect.top
        + ((layout.heatmap_legend_rect.bottom - layout.heatmap_legend_rect.top - kHeatmapLegendCellSize) * 0.5F);
    for (std::size_t level = 0; level <= brushes.size(); ++level) {
        const D2D1_RECT_F rect = D2D1::RectF(
            left, top, left + kHeatmapLegendCellSize, top + kHeatmapLegendCellSize
        );
        context.render_target->FillRoundedRectangle(
            D2D1::RoundedRect(rect, 1.5F, 1.5F),
            level == 0 ? context.panel_background_brush : brushes[level - 1].Get()
        );
        if (level == 0) {
            context.render_target->DrawRoundedRectangle(
                D2D1::RoundedRect(rect, 1.5F, 1.5F), context.border_brush, 0.7F
            );
        }
        left += kHeatmapLegendCellSize + kHeatmapLegendCellGap;
    }
    DrawCenteredCaption(
        context,
        more,
        D2D1::RectF(left + 2.0F, layout.heatmap_legend_rect.top, layout.heatmap_legend_rect.right, layout.heatmap_legend_rect.bottom),
        context.muted_text_brush
    );
}

// ----------------------------------------------------------------------------
// Dessine le tooltip compact de la cellule survolee.
//
// Parametres :
// - context : ressources de rendu.
// - layout : limites du graphe.
// - cell : valeur quotidienne a afficher.
// - target : cellule servant d'ancrage.
// ----------------------------------------------------------------------------
void DrawHeatmapTooltip(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenHeatmapCell& cell,
    const D2D1_RECT_F& target,
    GraphRange granularity
) {
    const std::wstring text = granularity == GraphRange::Minutes5
        ? FormatTokenBucketTooltip(cell.bucket_start, GraphRange::Minutes5, cell.total_tokens)
        : FormatTokenDayTooltip(cell.date_key, cell.total_tokens);
    const D2D1_SIZE_F text_size = MeasureCaption(context, text, 260.0F);
    const float width = std::min(
        layout.section_rect.right - layout.section_rect.left,
        text_size.width + (2.0F * kHeatmapTooltipPadding)
    );
    const float height = text_size.height + (2.0F * kHeatmapTooltipPadding);
    const float center = (target.left + target.right) * 0.5F;
    const float left = std::clamp(
        center - (width * 0.5F),
        layout.section_rect.left,
        layout.section_rect.right - width
    );
    const float top = std::max(layout.title_rect.bottom + 2.0F, target.top - height - 5.0F);
    const D2D1_RECT_F rect = D2D1::RectF(left, top, left + width, top + height);
    const auto rounded = D2D1::RoundedRect(rect, 3.0F, 3.0F);
    context.render_target->FillRoundedRectangle(rounded, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(rounded, context.border_brush, 1.0F);
    DrawHeatmapTooltipPointer(
        context,
        D2D1::Point2F(
            std::clamp(center, rect.left + 9.0F, rect.right - 9.0F),
            rect.bottom
        ),
        D2D1::Point2F(center, target.top)
    );
    DrawCenteredCaption(
        context,
        text,
        D2D1::RectF(
            rect.left + kHeatmapTooltipPadding,
            rect.top + kHeatmapTooltipPadding,
            rect.right - kHeatmapTooltipPadding,
            rect.bottom - kHeatmapTooltipPadding
        ),
        context.body_text_brush
    );
}

} // namespace

// ----------------------------------------------------------------------------
// Dessine une rangee compacte des dernieres tranches d'activite Tokens.
// ----------------------------------------------------------------------------
void DrawCompactTokenActivityStrip(
    const WidgetRenderGraphContext& context,
    const TokenUsageSnapshot& snapshot,
    const D2D1_RECT_F& rect
) {
    // Nombre de tranches recentes conservees dans une carte de quota.
    constexpr std::size_t kCompactCellCount = 8U;

    // Espacement logique entre deux cellules compactes.
    constexpr float kCompactCellGap = 2.0F;

    // Intensites identiques aux quatre niveaux actifs de la vue Activite.
    constexpr std::array<float, 4> kCompactLevelRatios = {0.28F, 0.46F, 0.68F, 0.92F};

    if (context.render_target == nullptr
        || context.panel_background_brush == nullptr
        || context.history_curve_brush == nullptr
        || context.border_brush == nullptr
        || rect.right <= rect.left
        || rect.bottom <= rect.top) {
        return;
    }
    const std::vector<TokenHeatmapCell> cells = BuildTokenFiveMinuteHeatmapCells(
        snapshot.five_minute,
        kCompactCellCount
    );
    if (cells.size() != kCompactCellCount) {
        return;
    }
    const float available_width = rect.right - rect.left
        - (kCompactCellGap * static_cast<float>(kCompactCellCount - 1U));
    const float cell_size = std::min(
        rect.bottom - rect.top,
        available_width / static_cast<float>(kCompactCellCount)
    );
    if (cell_size <= 0.0F) {
        return;
    }
    const float strip_width = (cell_size * static_cast<float>(kCompactCellCount))
        + (kCompactCellGap * static_cast<float>(kCompactCellCount - 1U));
    const float left = rect.left + ((rect.right - rect.left - strip_width) * 0.5F);
    const float top = rect.top + ((rect.bottom - rect.top - cell_size) * 0.5F);
    const D2D1_COLOR_F background = context.panel_background_brush->GetColor();
    const D2D1_COLOR_F accent = context.history_curve_brush->GetColor();
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, 4> brushes;
    for (std::size_t index = 0; index < brushes.size(); ++index) {
        context.render_target->CreateSolidColorBrush(
            MixHeatmapColor(background, accent, kCompactLevelRatios[index]),
            brushes[index].GetAddressOf()
        );
    }
    for (std::size_t index = 0; index < cells.size(); ++index) {
        const float cell_left = left + static_cast<float>(index) * (cell_size + kCompactCellGap);
        const D2D1_ROUNDED_RECT cell_rect = D2D1::RoundedRect(
            D2D1::RectF(cell_left, top, cell_left + cell_size, top + cell_size),
            1.5F,
            1.5F
        );
        const TokenHeatmapCell& cell = cells[index];
        ID2D1Brush* brush = cell.present && cell.level > 0
            ? static_cast<ID2D1Brush*>(brushes[cell.level - 1U].Get())
            : static_cast<ID2D1Brush*>(context.panel_background_brush);
        context.render_target->FillRoundedRectangle(cell_rect, brush);
        if (!cell.present || cell.level == 0) {
            const float previous_opacity = context.border_brush->GetOpacity();
            context.border_brush->SetOpacity(cell.present ? 0.45F : 0.24F);
            context.render_target->DrawRoundedRectangle(cell_rect, context.border_brush, 0.7F);
            context.border_brush->SetOpacity(previous_opacity);
        }
    }
}

// ----------------------------------------------------------------------------
// Dessine la heatmap quotidienne responsive et ses interactions.
// ----------------------------------------------------------------------------
void DrawTokenUsageHeatmap(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const TokenUsageSnapshot& snapshot,
    const WidgetGraphInteraction& interaction,
    GraphRange granularity
) {
    const auto draw_state = [&context, &layout](unsigned int resource_id) {
        DrawCenteredCaption(context, T(resource_id), layout.heatmap_grid_rect, context.muted_text_brush);
    };
    DrawHeatmapHeading(context, layout, interaction);
    const WidgetTokenGraphState state = ResolveTokenHeatmapState(snapshot, granularity);
    if (state != WidgetTokenGraphState::Data) {
        const unsigned int resource_id = state == WidgetTokenGraphState::Loading
            ? IDS_GRAPH_LOADING
            : (state == WidgetTokenGraphState::Error
                ? IDS_GRAPH_TOKENS_UNAVAILABLE
                : (state == WidgetTokenGraphState::NoSessions
                    ? IDS_GRAPH_NO_LOCAL_SESSIONS
                    : IDS_GRAPH_HISTORY_BUILDING));
        draw_state(resource_id);
        DrawHeatmapInformationHint(context, layout, interaction);
        return;
    }
    const std::vector<TokenHeatmapCell> cells = granularity == GraphRange::Minutes5
        ? BuildTokenFiveMinuteHeatmapCells(
            snapshot.five_minute,
            layout.heatmap_cell_rects.size()
        )
        : BuildTokenHeatmapCells(snapshot.daily, layout.heatmap_week_count);
    if (cells.empty() || cells.size() != layout.heatmap_cell_rects.size()) {
        draw_state(IDS_GRAPH_HISTORY_BUILDING);
        DrawHeatmapInformationHint(context, layout, interaction);
        return;
    }

    const D2D1_COLOR_F background = context.panel_background_brush->GetColor();
    const D2D1_COLOR_F accent = context.history_curve_brush->GetColor();
    // Opacites croissantes des quatre niveaux actifs de la heatmap.
    constexpr std::array<float, 4> kLevelRatios = {0.28F, 0.46F, 0.68F, 0.92F};
    std::array<Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>, 4> brushes;
    for (std::size_t index = 0; index < brushes.size(); ++index) {
        context.render_target->CreateSolidColorBrush(
            MixHeatmapColor(background, accent, kLevelRatios[index]),
            brushes[index].GetAddressOf()
        );
    }

    for (std::size_t index = 0; index < cells.size(); ++index) {
        const TokenHeatmapCell& cell = cells[index];
        if (!cell.present) {
            continue;
        }
        const D2D1_RECT_F& rect = layout.heatmap_cell_rects[index];
        const bool hovered = interaction.hovered_heatmap_cell == index;
        ID2D1Brush* brush = cell.level == 0
            ? static_cast<ID2D1Brush*>(context.panel_background_brush)
            : static_cast<ID2D1Brush*>(brushes[cell.level - 1].Get());
        context.render_target->FillRoundedRectangle(
            D2D1::RoundedRect(rect, kWidgetHeatmapCellRadius, kWidgetHeatmapCellRadius),
            brush
        );
        if (cell.level == 0 || hovered) {
            context.border_brush->SetOpacity(hovered ? 1.0F : 0.45F);
            context.render_target->DrawRoundedRectangle(
                D2D1::RoundedRect(rect, kWidgetHeatmapCellRadius, kWidgetHeatmapCellRadius),
                hovered ? static_cast<ID2D1Brush*>(context.body_text_brush)
                    : static_cast<ID2D1Brush*>(context.border_brush),
                hovered ? 1.2F : 0.7F
            );
            context.border_brush->SetOpacity(1.0F);
        }
    }
    if (granularity == GraphRange::Minutes5) {
        DrawFiveMinuteTimeLabels(context, layout, cells);
    } else {
        DrawHeatmapMonths(context, layout, cells);
    }
    DrawHeatmapLegend(context, layout, brushes);
    if (!interaction.hovered_graph_info
        && interaction.hovered_heatmap_cell.has_value()
        && *interaction.hovered_heatmap_cell < cells.size()
        && cells[*interaction.hovered_heatmap_cell].present) {
        DrawHeatmapTooltip(
            context,
            layout,
            cells[*interaction.hovered_heatmap_cell],
            layout.heatmap_cell_rects[*interaction.hovered_heatmap_cell],
            granularity
        );
    }
    DrawHeatmapInformationHint(context, layout, interaction);
}

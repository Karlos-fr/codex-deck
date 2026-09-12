// ============================================================================
// Codex Glass - Geometrie partagee des graphes
// ----------------------------------------------------------------------------
// Ce fichier calcule une geometrie fixe pour que dessin, survol et clic restent
// parfaitement alignes apres un changement de DPI ou de hauteur dynamique.
// ============================================================================

#include "WidgetGraphLayout.h"

#include "WidgetRenderConstants.h"
#include "WidgetRenderProviderInfo.h"

#include <algorithm>
#include <cmath>

namespace {

// Hauteur de la ligne de titre du graphe en DIPs.
constexpr float kGraphTitleHeight = 20.0F;

// Taille du bouton d'aide reserve a cote du titre en DIPs.
constexpr float kGraphHelpButtonSize = 14.0F;

// Espace horizontal entre le titre mesure et la bulle d'information en DIPs.
constexpr float kGraphTitleToHelpSpacing = 4.0F;

// Largeur du selecteur de plage dans l'en-tete du graphe en DIPs.
constexpr float kGraphRangeSelectorWidth = 48.0F;

// Hauteur du selecteur de plage dans l'en-tete du graphe en DIPs.
constexpr float kGraphRangeSelectorHeight = 18.0F;

// Taille carree du bouton de capture de l'onglet en DIPs.
constexpr float kGraphCaptureButtonSize = 18.0F;

// Espace horizontal entre le bouton de capture et le selecteur en DIPs.
constexpr float kGraphCaptureButtonGap = 5.0F;

// Hauteur du hint du bouton de capture en DIPs.
constexpr float kGraphCaptureHintHeight = 24.0F;

// Espace entre le contenu du titre et le selecteur en DIPs.
constexpr float kGraphTitleToSelectorSpacing = 5.0F;

// Espace vertical entre le titre et la zone de tracage en DIPs.
constexpr float kGraphTitleToPlotSpacing = 12.0F;

// Largeur de la gouttiere des valeurs de l'axe Y en DIPs.
constexpr float kGraphYAxisWidth = 38.0F;

// Espace entre l'axe Y et la zone de tracage en DIPs.
constexpr float kGraphYAxisGap = 6.0F;

// Hauteur reservee aux libelles de dates en DIPs.
constexpr float kGraphXAxisHeight = 18.0F;

// Espace entre les dates et le controle segmente en DIPs.
constexpr float kGraphXAxisToControlSpacing = 12.0F;

// Largeur nominale du controle segmente en DIPs.
constexpr float kGraphSegmentedControlWidth = 240.0F;

// Largeur minimale du controle segmente sur une petite fenetre en DIPs.
constexpr float kGraphSegmentedControlMinimumWidth = 160.0F;

// Hauteur du controle segmente en DIPs.
constexpr float kGraphSegmentedControlHeight = 28.0F;

// Hauteur de la ligne de statut placee sous le controle segmente en DIPs.
constexpr float kGraphFreshnessStatusHeight = 14.0F;

// Espace entre le controle segmente et la ligne de statut en DIPs.
constexpr float kGraphControlToStatusSpacing = 8.0F;

// Marge basse de la ligne de statut en DIPs.
constexpr float kGraphBottomPadding = 10.0F;

// Largeur maximale du hint explicatif en DIPs.
constexpr float kGraphHintMaximumWidth = 280.0F;

// Hauteur maximale du hint explicatif en DIPs.
constexpr float kGraphHintHeight = 62.0F;

// Hauteur reservee au tooltip de valeur en DIPs.
constexpr float kGraphDataTooltipHeight = 28.0F;

// Hauteur reservee aux libelles mensuels de la heatmap en DIPs.
constexpr float kHeatmapMonthsHeight = 16.0F;

// Espace entre le titre et les libelles mensuels de la heatmap en DIPs.
constexpr float kHeatmapTitleToMonthsSpacing = 2.0F;

// Espace vertical entre les zones de la heatmap en DIPs.
constexpr float kHeatmapVerticalSpacing = 4.0F;

// Espace horizontal et vertical entre les cartes du Bilan en DIPs.
constexpr float kSummaryCardGap = 8.0F;

// Espace entre le titre et les cartes du Bilan en DIPs.
constexpr float kSummaryTitleToCardsSpacing = 8.0F;

// Espace entre les cartes du Bilan et le controle segmente en DIPs.
constexpr float kSummaryCardsToControlSpacing = 10.0F;

// Nombre de tranches de cinq minutes empilees dans chaque colonne horaire.
constexpr std::size_t kFiveMinuteActivityRowCount = 12;

// ----------------------------------------------------------------------------
// Calcule des positions regulieres entre deux coordonnees incluses.
//
// Parametres :
// - start : premiere coordonnee.
// - end : derniere coordonnee.
//
// Retour :
// - positions ordonnees couvrant tout l'intervalle.
// ----------------------------------------------------------------------------
template <std::size_t TCount>
std::array<float, TCount> BuildEvenPositions(float start, float end) {
    std::array<float, TCount> positions{};
    if constexpr (TCount == 1) {
        positions[0] = start;
        return positions;
    }
    const float step = (end - start) / static_cast<float>(TCount - 1);
    for (std::size_t index = 0; index < TCount; ++index) {
        positions[index] = start + (step * static_cast<float>(index));
    }
    return positions;
}

} // namespace

// ----------------------------------------------------------------------------
// Calcule combien de semaines completes tiennent dans la zone heatmap.
//
// Parametres :
// - rect : rectangle disponible pour les cellules.
//
// Retour :
// - nombre de semaines completes limite a la couverture annuelle.
// ----------------------------------------------------------------------------
std::size_t CalculateWidgetHeatmapWeekCount(const D2D1_RECT_F& rect) {
    const float width = std::max(0.0F, rect.right - rect.left);
    const float height = std::max(0.0F, rect.bottom - rect.top);
    const float cell_size = std::max(
        0.0F,
        (height - (kWidgetHeatmapCellGap * static_cast<float>(kWidgetHeatmapRowCount - 1)))
            / static_cast<float>(kWidgetHeatmapRowCount)
    );
    if (cell_size <= 0.0F || width < cell_size) {
        return 0;
    }
    const float pitch = cell_size + kWidgetHeatmapCellGap;
    return std::min(
        kWidgetHeatmapMaximumWeekCount,
        static_cast<std::size_t>(std::floor((width + kWidgetHeatmapCellGap) / pitch))
    );
}

// ----------------------------------------------------------------------------
// Calcule la geometrie stable de la zone de graphe pour la taille courante.
// ----------------------------------------------------------------------------
WidgetGraphLayout BuildWidgetGraphLayout(
    D2D1_SIZE_F size,
    const AppSettings& settings,
    const UsageSnapshot& snapshot,
    std::size_t token_bar_count,
    float title_width,
    float capture_hint_text_width
) {
    WidgetGraphLayout layout{};
    const float content_left = kPanelPadding;
    const float content_right = std::max(content_left, size.width - kPanelPadding);
    const float first_usage_top = 54.0F;
    const std::size_t visible_quota_rows = CountVisibleQuotaRows(
        snapshot,
        settings.quota_visibility,
        settings.display_mode == WidgetDisplayMode::Complete
    );
    const float usage_bottom = visible_quota_rows == 0
        ? first_usage_top
        : first_usage_top
            + (static_cast<float>(visible_quota_rows - 1U) * kRowSpacing)
            + kUsageRowHeight;

    const float section_top = usage_bottom + kGraphTopSpacing;
    const float status_bottom = std::max(section_top, size.height - kGraphBottomPadding);
    const float status_top = std::max(section_top, status_bottom - kGraphFreshnessStatusHeight);
    const float section_bottom = std::max(
        section_top,
        status_top - kGraphControlToStatusSpacing
    );
    layout.section_rect = D2D1::RectF(content_left, section_top, content_right, section_bottom);

    layout.freshness_status_rect = D2D1::RectF(
        content_left,
        status_top,
        content_right,
        status_bottom
    );

    layout.range_selector_rect = D2D1::RectF(
        std::max(content_left, content_right - kGraphRangeSelectorWidth),
        section_top + 1.0F,
        content_right,
        section_top + 1.0F + kGraphRangeSelectorHeight
    );
    if (settings.graph_page == WidgetGraphPage::Activity) {
        layout.range_selector_rect.right = layout.range_selector_rect.left;
    }
    const float capture_right = layout.range_selector_rect.left - kGraphCaptureButtonGap;
    layout.capture_button_rect = D2D1::RectF(
        capture_right - kGraphCaptureButtonSize,
        section_top + 1.0F,
        capture_right,
        section_top + 1.0F + kGraphCaptureButtonSize
    );
    const float capture_hint_width = std::max(
        0.0F,
        capture_hint_text_width + (kWidgetHintHorizontalPadding * 2.0F)
    );
    layout.capture_hint_rect = D2D1::RectF(
        std::max(content_left, capture_right - capture_hint_width),
        layout.capture_button_rect.bottom + 5.0F,
        capture_right,
        layout.capture_button_rect.bottom + 5.0F + kGraphCaptureHintHeight
    );
    layout.title_rect = D2D1::RectF(
        content_left,
        section_top,
        std::max(content_left, layout.capture_button_rect.left - kGraphTitleToSelectorSpacing),
        section_top + kGraphTitleHeight
    );
    const float help_left = std::clamp(
        content_left + std::max(0.0F, title_width) + kGraphTitleToHelpSpacing,
        content_left,
        std::max(content_left, layout.title_rect.right - kGraphHelpButtonSize)
    );
    layout.help_button_rect = D2D1::RectF(
        help_left,
        section_top + 1.0F,
        help_left + kGraphHelpButtonSize,
        section_top + 1.0F + kGraphHelpButtonSize
    );
    const float hint_width = std::min(
        kGraphHintMaximumWidth,
        std::max(0.0F, content_right - content_left)
    );
    layout.hint_tooltip_rect = D2D1::RectF(
        content_left,
        layout.title_rect.bottom + 9.0F,
        content_left + hint_width,
        layout.title_rect.bottom + 9.0F + kGraphHintHeight
    );

    const float available_control_width = std::max(0.0F, content_right - content_left);
    const float control_width = std::min(
        kGraphSegmentedControlWidth,
        std::max(
            std::min(kGraphSegmentedControlMinimumWidth, available_control_width),
            available_control_width * 0.79F
        )
    );
    const float control_left = content_left + ((available_control_width - control_width) * 0.5F);
    layout.segmented_control_rect = D2D1::RectF(
        control_left,
        section_bottom - kGraphSegmentedControlHeight,
        control_left + control_width,
        section_bottom
    );
    const float segment_width = (
        layout.segmented_control_rect.right - layout.segmented_control_rect.left
    ) / 4.0F;
    const float first_segment_right = layout.segmented_control_rect.left + segment_width;
    const float second_segment_right = first_segment_right + segment_width;
    const float third_segment_right = second_segment_right + segment_width;
    layout.quotas_tab_rect = D2D1::RectF(
        layout.segmented_control_rect.left,
        layout.segmented_control_rect.top,
        first_segment_right,
        layout.segmented_control_rect.bottom
    );
    layout.tokens_tab_rect = D2D1::RectF(
        first_segment_right,
        layout.segmented_control_rect.top,
        second_segment_right,
        layout.segmented_control_rect.bottom
    );
    layout.activity_tab_rect = D2D1::RectF(
        second_segment_right,
        layout.segmented_control_rect.top,
        third_segment_right,
        layout.segmented_control_rect.bottom
    );
    layout.summary_tab_rect = D2D1::RectF(
        third_segment_right,
        layout.segmented_control_rect.top,
        layout.segmented_control_rect.right,
        layout.segmented_control_rect.bottom
    );

    layout.summary_grid_rect = D2D1::RectF(
        content_left,
        layout.title_rect.bottom + kSummaryTitleToCardsSpacing,
        content_right,
        std::max(
            layout.title_rect.bottom + kSummaryTitleToCardsSpacing,
            layout.segmented_control_rect.top - kSummaryCardsToControlSpacing
        )
    );
    const float summary_middle_x = layout.summary_grid_rect.left
        + ((layout.summary_grid_rect.right - layout.summary_grid_rect.left) * 0.5F);
    const float summary_middle_y = layout.summary_grid_rect.top
        + ((layout.summary_grid_rect.bottom - layout.summary_grid_rect.top) * 0.5F);
    const float half_gap = kSummaryCardGap * 0.5F;
    layout.summary_card_rects = {
        D2D1::RectF(
            layout.summary_grid_rect.left,
            layout.summary_grid_rect.top,
            summary_middle_x - half_gap,
            summary_middle_y - half_gap
        ),
        D2D1::RectF(
            summary_middle_x + half_gap,
            layout.summary_grid_rect.top,
            layout.summary_grid_rect.right,
            summary_middle_y - half_gap
        ),
        D2D1::RectF(
            layout.summary_grid_rect.left,
            summary_middle_y + half_gap,
            summary_middle_x - half_gap,
            layout.summary_grid_rect.bottom
        ),
        D2D1::RectF(
            summary_middle_x + half_gap,
            summary_middle_y + half_gap,
            layout.summary_grid_rect.right,
            layout.summary_grid_rect.bottom
        ),
    };

    layout.x_axis_rect = D2D1::RectF(
        content_left + kGraphYAxisWidth + kGraphYAxisGap,
        layout.segmented_control_rect.top - kGraphXAxisToControlSpacing - kGraphXAxisHeight,
        content_right,
        layout.segmented_control_rect.top - kGraphXAxisToControlSpacing
    );
    const float plot_top = layout.title_rect.bottom + kGraphTitleToPlotSpacing;
    const float plot_bottom = std::max(plot_top, layout.x_axis_rect.top);
    layout.y_axis_rect = D2D1::RectF(
        content_left,
        plot_top,
        content_left + kGraphYAxisWidth,
        plot_bottom
    );
    layout.plot_rect = D2D1::RectF(
        content_left + kGraphYAxisWidth + kGraphYAxisGap,
        plot_top,
        content_right,
        plot_bottom
    );
    layout.heatmap_months_rect = D2D1::RectF(
        content_left,
        layout.title_rect.bottom + kHeatmapTitleToMonthsSpacing,
        content_right,
        layout.title_rect.bottom + kHeatmapTitleToMonthsSpacing + kHeatmapMonthsHeight
    );
    layout.heatmap_legend_rect = D2D1::RectF(
        content_left,
        layout.x_axis_rect.top,
        content_right,
        layout.x_axis_rect.bottom
    );
    layout.heatmap_grid_rect = D2D1::RectF(
        content_left,
        layout.heatmap_months_rect.bottom + kHeatmapVerticalSpacing,
        content_right,
        std::max(layout.heatmap_months_rect.bottom + kHeatmapVerticalSpacing, plot_bottom)
    );
    const bool five_minute_activity = settings.graph_page == WidgetGraphPage::Activity
        && settings.activity_graph_range == GraphRange::Minutes5;
    layout.heatmap_week_count = five_minute_activity
        ? 0U
        : CalculateWidgetHeatmapWeekCount(layout.heatmap_grid_rect);
    if (five_minute_activity) {
        const std::size_t column_count = std::max<std::size_t>(
            1U,
            (token_bar_count + kFiveMinuteActivityRowCount - 1U)
                / kFiveMinuteActivityRowCount
        );
        const float available_width = std::max(
            0.0F,
            layout.heatmap_grid_rect.right - layout.heatmap_grid_rect.left
        );
        const float available_height = std::max(
            0.0F,
            layout.heatmap_grid_rect.bottom - layout.heatmap_grid_rect.top
        );
        const float cell_width = std::max(0.0F,
            (available_width - (kWidgetHeatmapCellGap
                * static_cast<float>(column_count - 1U)))
                / static_cast<float>(column_count)
        );
        const float cell_height = std::max(0.0F,
            (available_height - (kWidgetHeatmapCellGap
                * static_cast<float>(kFiveMinuteActivityRowCount - 1U)))
                / static_cast<float>(kFiveMinuteActivityRowCount)
        );
        const float occupied_width = (cell_width * static_cast<float>(column_count))
            + (kWidgetHeatmapCellGap * static_cast<float>(column_count - 1U));
        const float occupied_height = (cell_height * static_cast<float>(kFiveMinuteActivityRowCount))
            + (kWidgetHeatmapCellGap * static_cast<float>(kFiveMinuteActivityRowCount - 1U));
        const float left_origin = layout.heatmap_grid_rect.left
            + ((available_width - occupied_width) * 0.5F);
        const float top_origin = layout.heatmap_grid_rect.top
            + ((available_height - occupied_height) * 0.5F);
        layout.heatmap_cell_rects.reserve(token_bar_count);
        for (std::size_t column = 0; column < column_count; ++column) {
            for (std::size_t row = 0; row < kFiveMinuteActivityRowCount; ++row) {
                if (layout.heatmap_cell_rects.size() >= token_bar_count) {
                    break;
                }
                const float left = left_origin
                    + ((cell_width + kWidgetHeatmapCellGap) * static_cast<float>(column));
                const float top = top_origin
                    + ((cell_height + kWidgetHeatmapCellGap) * static_cast<float>(row));
                layout.heatmap_cell_rects.push_back(D2D1::RectF(
                    left,
                    top,
                    left + cell_width,
                    top + cell_height
                ));
            }
        }
    } else if (layout.heatmap_week_count > 0) {
        const float grid_height = std::max(
            0.0F,
            layout.heatmap_grid_rect.bottom - layout.heatmap_grid_rect.top
        );
        const float cell_size = std::max(
            0.0F,
            (grid_height
                - (kWidgetHeatmapCellGap * static_cast<float>(kWidgetHeatmapRowCount - 1)))
                / static_cast<float>(kWidgetHeatmapRowCount)
        );
        const float pitch = cell_size + kWidgetHeatmapCellGap;
        const float occupied_width = (cell_size * static_cast<float>(layout.heatmap_week_count))
            + (kWidgetHeatmapCellGap * static_cast<float>(layout.heatmap_week_count - 1));
        const float grid_left = layout.heatmap_grid_rect.right - occupied_width;
        layout.heatmap_cell_rects.reserve(
            layout.heatmap_week_count * kWidgetHeatmapRowCount
        );
        for (std::size_t column = 0; column < layout.heatmap_week_count; ++column) {
            for (std::size_t row = 0; row < kWidgetHeatmapRowCount; ++row) {
                const float left = grid_left + (pitch * static_cast<float>(column));
                const float top = layout.heatmap_grid_rect.top + (pitch * static_cast<float>(row));
                layout.heatmap_cell_rects.push_back(D2D1::RectF(
                    left,
                    top,
                    left + cell_size,
                    top + cell_size
                ));
            }
        }
    }
    layout.data_tooltip_rect = D2D1::RectF(
        layout.plot_rect.left,
        layout.plot_rect.top,
        layout.plot_rect.right,
        std::min(layout.plot_rect.bottom, layout.plot_rect.top + kGraphDataTooltipHeight)
    );
    layout.y_tick_positions = BuildEvenPositions<kWidgetGraphYAxisTickCount>(
        layout.plot_rect.top,
        layout.plot_rect.bottom
    );
    layout.x_tick_positions = BuildEvenPositions<kWidgetGraphXAxisTickCount>(
        layout.plot_rect.left,
        layout.plot_rect.right
    );

    if (token_bar_count > 0) {
        const float graph_width = std::max(1.0F, layout.plot_rect.right - layout.plot_rect.left);
        const float slot_width = graph_width / static_cast<float>(token_bar_count);
        const float bar_width = std::clamp(slot_width * 0.62F, 2.0F, 8.0F);
        layout.token_bar_rects.reserve(token_bar_count);
        for (std::size_t index = 0; index < token_bar_count; ++index) {
            const float center = layout.plot_rect.left + (slot_width * (static_cast<float>(index) + 0.5F));
            layout.token_bar_rects.push_back(D2D1::RectF(
                center - (bar_width * 0.5F), layout.plot_rect.top,
                center + (bar_width * 0.5F), layout.plot_rect.bottom
            ));
        }
    }
    return layout;
}

// ----------------------------------------------------------------------------
// Calcule la geometrie en utilisant un hint de capture de largeur nulle.
//
// Parametres :
// - size : taille disponible en DIPs.
// - settings : reglages d'affichage courants.
// - snapshot : donnees de quotas utilisees pour positionner le graphe.
// - token_bar_count : nombre d'emplacements de barres a reserver.
// - title_width : largeur mesuree du titre actif.
//
// Retour :
// - geometrie stable de la zone de graphe.
// ----------------------------------------------------------------------------
WidgetGraphLayout BuildWidgetGraphLayout(
    D2D1_SIZE_F size,
    const AppSettings& settings,
    const UsageSnapshot& snapshot,
    std::size_t token_bar_count,
    float title_width
) {
    return BuildWidgetGraphLayout(size, settings, snapshot, token_bar_count, title_width, 0.0F);
}

// ----------------------------------------------------------------------------
// Indique si un point en DIPs appartient au rectangle fourni.
// ----------------------------------------------------------------------------
bool WidgetGraphRectContains(const D2D1_RECT_F& rect, D2D1_POINT_2F point) {
    return point.x >= rect.left && point.x <= rect.right
        && point.y >= rect.top && point.y <= rect.bottom;
}

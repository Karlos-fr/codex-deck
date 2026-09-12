// ============================================================================
// Codex Glass - Interaction des graphes
// ----------------------------------------------------------------------------
// Ce fichier effectue des hit-tests purs sur la geometrie partagee.
// ============================================================================

#include "WidgetGraphInteraction.h"

// ----------------------------------------------------------------------------
// Retourne la page dont l'onglet contient le point, le cas echeant.
// ----------------------------------------------------------------------------
std::optional<WidgetGraphPage> HitTestWidgetGraphTab(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    if (WidgetGraphRectContains(layout.quotas_tab_rect, point)) {
        return WidgetGraphPage::Quotas;
    }
    if (WidgetGraphRectContains(layout.tokens_tab_rect, point)) {
        return WidgetGraphPage::Tokens;
    }
    if (WidgetGraphRectContains(layout.activity_tab_rect, point)) {
        return WidgetGraphPage::Activity;
    }
    if (WidgetGraphRectContains(layout.summary_tab_rect, point)) {
        return WidgetGraphPage::Summary;
    }
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Retourne l'index de barre contenant le point, le cas echeant.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestWidgetTokenBar(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    if (!WidgetGraphRectContains(layout.plot_rect, point)) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < layout.token_bar_rects.size(); ++index) {
        const D2D1_RECT_F slot = D2D1::RectF(
            index == 0 ? layout.plot_rect.left : (layout.token_bar_rects[index - 1].right + layout.token_bar_rects[index].left) * 0.5F,
            layout.plot_rect.top,
            index + 1 == layout.token_bar_rects.size() ? layout.plot_rect.right : (layout.token_bar_rects[index].right + layout.token_bar_rects[index + 1].left) * 0.5F,
            layout.plot_rect.bottom
        );
        if (WidgetGraphRectContains(slot, point)) {
            return index;
        }
    }
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Retourne l'index de cellule heatmap contenant le point, le cas echeant.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestWidgetHeatmapCell(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    if (!WidgetGraphRectContains(layout.heatmap_grid_rect, point)) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < layout.heatmap_cell_rects.size(); ++index) {
        if (WidgetGraphRectContains(layout.heatmap_cell_rects[index], point)) {
            return index;
        }
    }
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Indique si la bulle d'information contient le point fourni.
//
// Parametres :
// - layout : geometrie partagee du graphe.
// - point : position a tester en DIPs.
//
// Retour :
// - true si le point appartient a la bulle, false sinon.
// ----------------------------------------------------------------------------
bool HitTestWidgetGraphInfo(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    return WidgetGraphRectContains(layout.help_button_rect, point);
}

// ----------------------------------------------------------------------------
// Indique si le selecteur de plage contient le point fourni.
//
// Parametres :
// - layout : geometrie contenant le rectangle du selecteur.
// - point : position a tester en DIPs.
//
// Retour :
// - true dans le selecteur, false ailleurs.
// ----------------------------------------------------------------------------
bool HitTestWidgetGraphRangeSelector(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    return WidgetGraphRectContains(layout.range_selector_rect, point);
}

// ----------------------------------------------------------------------------
// Indique si le bouton de capture contient le point fourni.
//
// Parametres :
// - layout : geometrie contenant le rectangle du bouton.
// - point : position a tester en DIPs.
//
// Retour :
// - true dans le bouton, false ailleurs.
// ----------------------------------------------------------------------------
bool HitTestWidgetGraphCaptureButton(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    return WidgetGraphRectContains(layout.capture_button_rect, point);
}

// ----------------------------------------------------------------------------
// Retourne le point survole lorsqu'il appartient au trace Quotas.
// ----------------------------------------------------------------------------
std::optional<D2D1_POINT_2F> HitTestWidgetQuotaPlot(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
) {
    return WidgetGraphRectContains(layout.plot_rect, point)
        ? std::optional<D2D1_POINT_2F>{point}
        : std::nullopt;
}

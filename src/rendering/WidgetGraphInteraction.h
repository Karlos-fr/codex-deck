// ============================================================================
// Codex Glass - Interaction des graphes
// ----------------------------------------------------------------------------
// Ce fichier declare l'etat de survol et les hit-tests des onglets et barres.
// Il ne depend d'aucun handle Direct2D.
// ============================================================================

#pragma once

#include "WidgetGraphLayout.h"

#include <optional>

// ----------------------------------------------------------------------------
// Regroupe l'etat souris courant de la zone de graphe.
// ----------------------------------------------------------------------------
struct WidgetGraphInteraction {
    // Segment graphique actuellement survole parmi les quatre pages.
    std::optional<WidgetGraphPage> hovered_graph_tab;

    // Segment maintenu enfonce jusqu'au relachement du bouton gauche.
    std::optional<WidgetGraphPage> pressed_graph_tab;

    // Index de la barre tokens survolee.
    std::optional<std::size_t> hovered_token_bar;

    // Index de la cellule quotidienne survolee dans la heatmap.
    std::optional<std::size_t> hovered_heatmap_cell;

    // Position survolee dans le tracage Quotas, exprimee en DIPs.
    std::optional<D2D1_POINT_2F> hovered_quota_plot_point;

    // Indique que la bulle d'information de la vue courante est survolee.
    bool hovered_graph_info = false;

    // Indique que le selecteur de plage de la vue courante est survole.
    bool hovered_graph_range_selector = false;

    // Indique que le bouton de capture de la vue active est survole.
    bool hovered_graph_capture_button = false;

    // Indique que le bouton de capture est maintenu enfonce.
    bool pressed_graph_capture_button = false;

    // Indique qu'un suivi WM_MOUSELEAVE est arme.
    bool tracking_mouse_leave = false;

    // Indique que le suivi de sortie courant concerne la zone non cliente.
    bool tracking_non_client_leave = false;
};

// ----------------------------------------------------------------------------
// Retourne la page dont l'onglet contient le point, le cas echeant.
// ----------------------------------------------------------------------------
std::optional<WidgetGraphPage> HitTestWidgetGraphTab(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Retourne l'index de barre contenant le point, le cas echeant.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestWidgetTokenBar(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Retourne l'index de cellule heatmap contenant le point, le cas echeant.
//
// Parametres :
// - layout : geometrie courante de la heatmap.
// - point : position a tester en DIPs.
//
// Retour :
// - index de cellule ou aucune valeur hors de la grille.
// ----------------------------------------------------------------------------
std::optional<std::size_t> HitTestWidgetHeatmapCell(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Indique si la bulle d'information contient le point fourni.
// ----------------------------------------------------------------------------
bool HitTestWidgetGraphInfo(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Indique si le selecteur de plage contient le point fourni.
// ----------------------------------------------------------------------------
bool HitTestWidgetGraphRangeSelector(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Indique si le bouton de capture contient le point fourni.
// ----------------------------------------------------------------------------
bool HitTestWidgetGraphCaptureButton(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Retourne le point survole lorsqu'il appartient au trace Quotas.
//
// Parametres :
// - layout : geometrie partagee du graphe.
// - point : position a tester en DIPs.
//
// Retour :
// - point fourni dans la zone de trace, ou aucune valeur hors de cette zone.
// ----------------------------------------------------------------------------
std::optional<D2D1_POINT_2F> HitTestWidgetQuotaPlot(
    const WidgetGraphLayout& layout,
    D2D1_POINT_2F point
);

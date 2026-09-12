// ============================================================================
// Codex Glass - Geometrie partagee des graphes
// ----------------------------------------------------------------------------
// Ce fichier declare les rectangles communs au rendu et au hit-test. Il ne
// dessine rien et ne possede aucune ressource Direct2D.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"
#include "../usage/UsageSnapshot.h"

#include <d2d1.h>

#include <array>
#include <cstddef>
#include <vector>

// Nombre fixe de lignes de la heatmap, du lundi au dimanche.
constexpr std::size_t kWidgetHeatmapRowCount = 7;

// Espacement horizontal et vertical entre les cellules en DIPs.
constexpr float kWidgetHeatmapCellGap = 3.0F;

// Rayon discret des cellules de heatmap en DIPs.
constexpr float kWidgetHeatmapCellRadius = 2.0F;

// Nombre maximal de semaines couvertes par 365 jours.
constexpr std::size_t kWidgetHeatmapMaximumWeekCount = 53;

// Nombre de graduations verticales partagees par les deux vues.
constexpr std::size_t kWidgetGraphYAxisTickCount = 5;

// Nombre de reperes temporels reserves sur l'axe horizontal.
constexpr std::size_t kWidgetGraphXAxisTickCount = 7;

// ----------------------------------------------------------------------------
// Regroupe toutes les zones partagees par les quatre vues graphiques.
// ----------------------------------------------------------------------------
struct WidgetGraphLayout {
    // Rectangle complet reserve a la section graphique.
    D2D1_RECT_F section_rect{};

    // Rectangle reserve au titre de la vue active.
    D2D1_RECT_F title_rect{};

    // Rectangle reserve au bouton d'aide place apres le titre.
    D2D1_RECT_F help_button_rect{};

    // Rectangle du selecteur de plage aligne a droite du titre.
    D2D1_RECT_F range_selector_rect{};

    // Rectangle du bouton qui copie la vue graphique active.
    D2D1_RECT_F capture_button_rect{};

    // Rectangle du hint associe au bouton de capture.
    D2D1_RECT_F capture_hint_rect{};

    // Rectangle maximal du hint explicatif affiche sous le titre.
    D2D1_RECT_F hint_tooltip_rect{};

    // Gouttiere reservee aux libelles de l'axe vertical.
    D2D1_RECT_F y_axis_rect{};

    // Rectangle de tracage commun aux courbes et aux barres.
    D2D1_RECT_F plot_rect{};

    // Rectangle reserve aux dates de l'axe horizontal.
    D2D1_RECT_F x_axis_rect{};

    // Rectangle maximal reserve au tooltip de la donnee survolee.
    D2D1_RECT_F data_tooltip_rect{};

    // Rectangle exterieur unique du controle segmente.
    D2D1_RECT_F segmented_control_rect{};

    // Rectangle reserve au statut de rafraichissement sous le controle segmente.
    D2D1_RECT_F freshness_status_rect{};

    // Rectangle cliquable du segment Quotas.
    D2D1_RECT_F quotas_tab_rect{};

    // Rectangle cliquable du segment Tokens.
    D2D1_RECT_F tokens_tab_rect{};

    // Rectangle cliquable du segment Activite.
    D2D1_RECT_F activity_tab_rect{};

    // Rectangle cliquable du segment Bilan.
    D2D1_RECT_F summary_tab_rect{};

    // Rectangle complet de la grille des indicateurs du Bilan.
    D2D1_RECT_F summary_grid_rect{};

    // Rectangles des quatre cartes KPI, dans l'ordre de lecture.
    std::array<D2D1_RECT_F, 4> summary_card_rects{};

    // Rectangle reserve aux libelles mensuels de la heatmap.
    D2D1_RECT_F heatmap_months_rect{};

    // Rectangle reserve a la grille quotidienne de la heatmap.
    D2D1_RECT_F heatmap_grid_rect{};

    // Rectangle reserve a la legende d'intensite de la heatmap.
    D2D1_RECT_F heatmap_legend_rect{};

    // Nombre de semaines effectivement visibles dans la heatmap.
    std::size_t heatmap_week_count = 0;

    // Rectangles des cellules, ordonnes par semaine puis du lundi au dimanche.
    std::vector<D2D1_RECT_F> heatmap_cell_rects;

    // Positions verticales des graduations, du haut vers le bas.
    std::array<float, kWidgetGraphYAxisTickCount> y_tick_positions{};

    // Positions horizontales des reperes temporels, de gauche a droite.
    std::array<float, kWidgetGraphXAxisTickCount> x_tick_positions{};

    // Rectangles des barres quotidiennes, dans l'ordre chronologique.
    std::vector<D2D1_RECT_F> token_bar_rects;
};

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
);

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
);

// ----------------------------------------------------------------------------
// Indique si un point en DIPs appartient au rectangle fourni.
// ----------------------------------------------------------------------------
bool WidgetGraphRectContains(const D2D1_RECT_F& rect, D2D1_POINT_2F point);

// ----------------------------------------------------------------------------
// Calcule combien de semaines completes tiennent dans la zone heatmap.
//
// Parametres :
// - rect : zone disponible pour les cellules.
//
// Retour :
// - nombre de colonnes compris entre zero et cinquante-trois.
// ----------------------------------------------------------------------------
std::size_t CalculateWidgetHeatmapWeekCount(const D2D1_RECT_F& rect);

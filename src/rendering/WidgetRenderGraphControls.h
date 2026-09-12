// ============================================================================
// Codex Glass - Controles partages des graphes
// ----------------------------------------------------------------------------
// Ce fichier declare le controle segmente commun aux pages Quotas, Tokens,
// Activite et Bilan. Il ne dessine aucune donnee de graphe.
// ============================================================================

#pragma once

#include "WidgetGraphInteraction.h"
#include "WidgetRenderGraph.h"

// ----------------------------------------------------------------------------
// Dessine le controle segmente unique sous la zone de graphe.
//
// Parametres :
// - context : ressources de rendu partagees.
// - layout : rectangles des quatre segments.
// - active_page : page actuellement affichee.
// - interaction : segments survole et presse.
// ----------------------------------------------------------------------------
void DrawWidgetGraphSegmentedControl(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    WidgetGraphPage active_page,
    const WidgetGraphInteraction& interaction
);

// ----------------------------------------------------------------------------
// Dessine le selecteur compact de la plage de la vue active.
//
// Parametres :
// - context : ressources de rendu partagees.
// - layout : rectangle du selecteur dans l'en-tete.
// - range : plage actuellement selectionnee.
// - page : page active qui donne sa semantique au libelle.
// - interaction : etat de survol du controle.
// ----------------------------------------------------------------------------
void DrawWidgetGraphRangeSelector(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    GraphRange range,
    WidgetGraphPage page,
    const WidgetGraphInteraction& interaction
);

// ----------------------------------------------------------------------------
// Dessine le bouton discret qui copie la vue active dans le presse-papiers.
//
// Parametres :
// - context : ressources de rendu partagees.
// - layout : rectangles du bouton et de son hint.
// - interaction : etat de survol et de pression du bouton.
// ----------------------------------------------------------------------------
void DrawWidgetGraphCaptureButton(
    const WidgetRenderGraphContext& context,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction
);

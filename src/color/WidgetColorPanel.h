// ============================================================================
// Codex Glass - Panneau integre de couleurs
// ----------------------------------------------------------------------------
// Ce fichier declare la geometrie et le hit-testing de l'extension laterale de
// personnalisation des couleurs du widget.
// ============================================================================

#pragma once

#include "WidgetColorTools.h"

#include <d2d1.h>
#include <windows.h>

#include <array>
#include <string>

// Largeur de l'extension laterale de couleurs en pixels et DIPs.
constexpr int kWidgetColorPanelWidth = 272;

// ----------------------------------------------------------------------------
// Liste les actions possibles dans le panneau de couleurs.
// ----------------------------------------------------------------------------
enum class WidgetColorPanelActionType {
    None,
    PickColor,
    Randomize,
    Close,
};

// ----------------------------------------------------------------------------
// Regroupe le resultat d'un hit-test du panneau de couleurs.
// ----------------------------------------------------------------------------
struct WidgetColorPanelHitTestResult {
    // Type d'action detectee.
    WidgetColorPanelActionType action = WidgetColorPanelActionType::None;

    // Champ couleur vise lorsque action vaut PickColor.
    WidgetColorField field = WidgetColorField::Background;
};

// ----------------------------------------------------------------------------
// Regroupe les etats interactifs du panneau couleurs.
// ----------------------------------------------------------------------------
struct WidgetColorPanelInteraction {
    // Element actuellement survole par la souris.
    WidgetColorPanelHitTestResult hovered{};

    // Element actuellement presse par la souris.
    WidgetColorPanelHitTestResult pressed{};
};

// ----------------------------------------------------------------------------
// Indique si deux resultats de hit-test representent le meme element.
//
// Parametres :
// - first : premier resultat.
// - second : second resultat.
//
// Retour :
// - true si les deux resultats pointent le meme element.
// ----------------------------------------------------------------------------
bool IsSameWidgetColorPanelHit(const WidgetColorPanelHitTestResult& first, const WidgetColorPanelHitTestResult& second);

// ----------------------------------------------------------------------------
// Regroupe les rectangles utiles au rendu et au hit-testing du panneau.
// ----------------------------------------------------------------------------
struct WidgetColorPanelLayout {
    // Zone totale du panneau.
    D2D1_RECT_F panel_rect{};

    // Rectangles des carres couleur.
    std::array<D2D1_RECT_F, 8> color_swatch_rects{};

    // Rectangles des libelles couleur.
    std::array<D2D1_RECT_F, 8> color_label_rects{};

    // Rectangle du bouton aleatoire.
    D2D1_RECT_F random_button_rect{};

    // Rectangle du bouton fermer.
    D2D1_RECT_F close_button_rect{};
};

// ----------------------------------------------------------------------------
// Retourne le champ couleur associe a un index de panneau.
//
// Parametres :
// - index : index de 0 a 6.
//
// Retour :
// - champ couleur correspondant.
// ----------------------------------------------------------------------------
WidgetColorField WidgetColorFieldFromPanelIndex(size_t index);

// ----------------------------------------------------------------------------
// Retourne le libelle localise d'un champ couleur.
//
// Parametres :
// - field : champ couleur a afficher.
//
// Retour :
// - chaine localisee.
// ----------------------------------------------------------------------------
std::wstring WidgetColorFieldLabel(WidgetColorField field);

// ----------------------------------------------------------------------------
// Calcule la geometrie du panneau depuis la taille client.
//
// Parametres :
// - client_size : taille Direct2D de la zone client.
//
// Retour :
// - rectangles prets pour le rendu et le hit-testing.
// ----------------------------------------------------------------------------
WidgetColorPanelLayout BuildWidgetColorPanelLayout(D2D1_SIZE_F client_size);

// ----------------------------------------------------------------------------
// Teste un clic souris dans le panneau de couleurs.
//
// Parametres :
// - layout : geometrie courante du panneau.
// - point : position souris en coordonnees client.
//
// Retour :
// - action detectee ou None.
// ----------------------------------------------------------------------------
WidgetColorPanelHitTestResult HitTestWidgetColorPanel(const WidgetColorPanelLayout& layout, POINT point);

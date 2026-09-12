// ============================================================================
// Codex Glass - Boutons dessines du widget
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers de rendu des boutons Direct2D utilises par les
// panneaux integres du widget.
// ============================================================================

#pragma once

#include <d2d1.h>
#include <dwrite.h>

// ----------------------------------------------------------------------------
// Liste les etats visuels d'un bouton dessine.
// ----------------------------------------------------------------------------
enum class WidgetButtonVisualState {
    Normal,
    Hovered,
    Pressed,
};

// ----------------------------------------------------------------------------
// Dessine un bouton texte compact.
//
// Parametres :
// - render_target : cible Direct2D active.
// - bounds : rectangle du bouton.
// - text : libelle du bouton.
// - text_format : format DirectWrite centre.
// - text_brush : brosse du texte.
// - border_brush : brosse du contour.
// - state : etat visuel courant.
// ----------------------------------------------------------------------------
void DrawWidgetTextButton(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& bounds,
    const wchar_t* text,
    IDWriteTextFormat* text_format,
    ID2D1Brush* text_brush,
    ID2D1Brush* border_brush,
    WidgetButtonVisualState state
);

// ----------------------------------------------------------------------------
// Dessine un bouton de fermeture compact avec une croix vectorielle.
//
// Parametres :
// - render_target : cible Direct2D active.
// - bounds : rectangle du bouton.
// - icon_brush : brosse de l'icone.
// - border_brush : brosse du contour.
// - state : etat visuel courant.
// ----------------------------------------------------------------------------
void DrawWidgetCloseButton(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& bounds,
    ID2D1Brush* icon_brush,
    ID2D1Brush* border_brush,
    WidgetButtonVisualState state
);

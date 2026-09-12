// ============================================================================
// Codex Glass - Declaration du bouton de masquage dans le tray
// ----------------------------------------------------------------------------
// Ce fichier declare la geometrie, le hit-test et le rendu du petit controle
// de fenetre. L'action de masquage reste orchestree par WidgetApp.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <d2d1.h>
#include <dwrite.h>

// Regroupe les etats visuels du bouton de masquage.
struct WidgetTrayHideButtonInteraction {
    // Indique que le pointeur survole le bouton.
    bool hovered = false;

    // Indique que le bouton gauche est maintenu sur le controle.
    bool pressed = false;

    // Indique que la fenetre suit actuellement WM_MOUSELEAVE.
    bool tracking_mouse_leave = false;
};

// Regroupe la zone interactive et la zone du hint.
struct WidgetTrayHideButtonLayout {
    // Rectangle cliquable du bouton en DIPs.
    D2D1_RECT_F button_rect{};

    // Rectangle du hint affiche sous le bouton en DIPs.
    D2D1_RECT_F hint_rect{};
};

// Regroupe les ressources Direct2D necessaires au controle.
struct WidgetTrayHideButtonRenderContext {
    // Cible Direct2D active.
    ID2D1RenderTarget* render_target = nullptr;

    // Brosse du fond du hint.
    ID2D1Brush* panel_background_brush = nullptr;

    // Brosse du contour discret.
    ID2D1Brush* border_brush = nullptr;

    // Brosse de l'icone et du texte secondaire.
    ID2D1Brush* muted_text_brush = nullptr;

    // Format DirectWrite du hint.
    IDWriteTextFormat* caption_text_format = nullptr;
};

// ----------------------------------------------------------------------------
// Calcule la geometrie du bouton depuis la taille et le mode du widget.
//
// Parametres :
// - size : taille cliente Direct2D du widget.
// - display_mode : mode qui determine la marge interieure du bandeau.
// - hint_text_width : largeur mesuree du libelle localise.
//
// Retour :
// - rectangles du bouton et de son hint.
// ----------------------------------------------------------------------------
WidgetTrayHideButtonLayout BuildWidgetTrayHideButtonLayout(
    D2D1_SIZE_F size,
    WidgetDisplayMode display_mode,
    float hint_text_width
);

// ----------------------------------------------------------------------------
// Indique si un point Direct2D appartient au bouton.
//
// Parametres :
// - layout : geometrie courante du bouton.
// - point : position cliente exprimee en DIPs.
//
// Retour :
// - true lorsque le point cible la zone cliquable.
// ----------------------------------------------------------------------------
bool HitTestWidgetTrayHideButton(
    const WidgetTrayHideButtonLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Dessine le bouton discret et son hint eventuel.
//
// Parametres :
// - context : ressources Direct2D et DirectWrite necessaires.
// - layout : geometrie courante du controle.
// - interaction : etat de survol et de pression.
// ----------------------------------------------------------------------------
void DrawWidgetTrayHideButton(
    const WidgetTrayHideButtonRenderContext& context,
    const WidgetTrayHideButtonLayout& layout,
    const WidgetTrayHideButtonInteraction& interaction
);

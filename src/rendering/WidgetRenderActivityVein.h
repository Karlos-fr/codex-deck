// ============================================================================
// Codex Glass - Rendu de la veine d'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier declare le dessin Direct2D du filament lumineux. La geometrie de
// placement reste fournie par l'orchestrateur de rendu du mode d'affichage.
// ============================================================================

#pragma once

#include "../activity/WidgetActivityController.h"
#include "WidgetRenderTypes.h"

#include <d2d1.h>
#include <dwrite.h>

#include <string>

enum class WidgetDisplayMode;

// ----------------------------------------------------------------------------
// Orientation logique de la progression le long de la veine.
// ----------------------------------------------------------------------------
enum class WidgetActivityVeinOrientation {
    Horizontal,
    Vertical,
};

// ----------------------------------------------------------------------------
// Etat de survol global de la veine, sans ciblage d'une impulsion particuliere.
// ----------------------------------------------------------------------------
struct WidgetActivityVeinInteraction {
    // Indique que le pointeur se trouve dans la zone globale du filament.
    bool hovered = false;
};

// ----------------------------------------------------------------------------
// Regroupe la zone du filament et celle de son hint adaptatif.
// ----------------------------------------------------------------------------
struct WidgetActivityVeinLayout {
    // Rectangle interactif et visuel du filament.
    D2D1_RECT_F vein_rect{};

    // Rectangle du hint global affiche au survol.
    D2D1_RECT_F hint_rect{};

    // Indique que le mode courant possede deja un placement de veine.
    bool available = false;
};

// ----------------------------------------------------------------------------
// Regroupe les ressources et couleurs necessaires au dessin de la veine.
// ----------------------------------------------------------------------------
struct WidgetRenderActivityVeinContext {
    // Factory utilisee pour construire les geometries organiques.
    ID2D1Factory* d2d_factory = nullptr;

    // Cible Direct2D qui recoit le filament et ses halos.
    ID2D1RenderTarget* render_target = nullptr;

    // Palette utilisateur dont sont derivees toutes les teintes.
    Palette palette{};
};

// ----------------------------------------------------------------------------
// Regroupe les ressources necessaires au hint global de la veine.
// ----------------------------------------------------------------------------
struct WidgetRenderActivityVeinHintContext {
    // Cible Direct2D active.
    ID2D1RenderTarget* render_target = nullptr;

    // Brosse du fond opaque du hint.
    ID2D1Brush* panel_background_brush = nullptr;

    // Brosse du contour discret du hint.
    ID2D1Brush* border_brush = nullptr;

    // Brosse du texte secondaire.
    ID2D1Brush* muted_text_brush = nullptr;

    // Format DirectWrite compact du hint.
    IDWriteTextFormat* caption_text_format = nullptr;
};

// ----------------------------------------------------------------------------
// Calcule le placement courant de la veine et de son hint.
//
// Parametres :
// - size : taille cliente Direct2D du widget.
// - display_mode : mode d'affichage courant.
// - hint_text_width : largeur mesuree du texte complet du hint.
//
// Retour :
// - geometrie disponible dans les modes deja integres.
// ----------------------------------------------------------------------------
WidgetActivityVeinLayout BuildWidgetActivityVeinLayout(
    D2D1_SIZE_F size,
    WidgetDisplayMode display_mode,
    float hint_text_width
);

// ----------------------------------------------------------------------------
// Indique si un point appartient a la zone globale de la veine.
//
// Parametres :
// - layout : geometrie courante de la veine.
// - point : position cliente exprimee en DIPs.
//
// Retour :
// - true lorsque le point doit afficher le hint global.
// ----------------------------------------------------------------------------
bool HitTestWidgetActivityVein(
    const WidgetActivityVeinLayout& layout,
    D2D1_POINT_2F point
);

// ----------------------------------------------------------------------------
// Dessine le filament lumineux et les impulsions des sessions actives.
//
// Parametres :
// - context : ressources Direct2D et palette de la frame.
// - bounds : zone reservee a la veine.
// - frame : etats stabilises fournis par le controleur d'activite.
// - orientation : sens principal de progression des impulsions.
// - animation_enabled : autorise le mouvement, sinon produit un rendu stable.
// ----------------------------------------------------------------------------
void DrawWidgetActivityVein(
    const WidgetRenderActivityVeinContext& context,
    const D2D1_RECT_F& bounds,
    const WidgetActivityFrame& frame,
    WidgetActivityVeinOrientation orientation,
    bool animation_enabled
);

// ----------------------------------------------------------------------------
// Dessine le hint global de la veine lorsqu'elle est survolee.
//
// Parametres :
// - context : ressources Direct2D et DirectWrite necessaires.
// - layout : geometrie du filament et du hint.
// - interaction : etat de survol global.
// - text : resume localise des sessions actives.
// ----------------------------------------------------------------------------
void DrawWidgetActivityVeinHint(
    const WidgetRenderActivityVeinHintContext& context,
    const WidgetActivityVeinLayout& layout,
    const WidgetActivityVeinInteraction& interaction,
    const std::wstring& text
);

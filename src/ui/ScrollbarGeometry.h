// ============================================================================
// Codex Deck - Geometrie de scrollbar
// ----------------------------------------------------------------------------
// Ce module calcule les rectangles et conversions de scrollbar sans dependance
// au rendu Direct2D ni aux messages Win32.
// ============================================================================

#pragma once

// ----------------------------------------------------------------------------
// Donnees d'entree d'une scrollbar verticale.
// ----------------------------------------------------------------------------
struct ScrollbarInput {
    // Bord haut de la zone de rendu.
    float top = 0.0F;

    // Bord bas de la zone de rendu.
    float bottom = 0.0F;

    // Bord droit de la zone de rendu.
    float right = 0.0F;

    // Etendue totale du contenu.
    float content_extent = 0.0F;

    // Etendue visible du viewport.
    float viewport_extent = 0.0F;

    // Offset vertical courant.
    float scroll_offset = 0.0F;

    // Largeur de hit de la scrollbar.
    float width = 6.0F;

    // Hauteur minimale du pouce.
    float minimum_thumb_height = 32.0F;
};

// ----------------------------------------------------------------------------
// Geometrie calculee d'une scrollbar verticale.
// ----------------------------------------------------------------------------
struct ScrollbarMetrics {
    // Indique si la scrollbar doit exister.
    bool visible = false;

    // Bord gauche du pouce.
    float left = 0.0F;

    // Bord droit du pouce.
    float right = 0.0F;

    // Bord haut de piste.
    float track_top = 0.0F;

    // Bord bas de piste.
    float track_bottom = 0.0F;

    // Bord haut du pouce.
    float thumb_top = 0.0F;

    // Bord bas du pouce.
    float thumb_bottom = 0.0F;

    // Etendue totale du contenu.
    float content_extent = 0.0F;

    // Etendue visible du viewport.
    float viewport_extent = 0.0F;
};

// ----------------------------------------------------------------------------
// Calcule la geometrie d'une scrollbar verticale.
//
// Parametres :
// - input : donnees de viewport et de contenu.
//
// Retour :
// - metriques de scrollbar.
// ----------------------------------------------------------------------------
ScrollbarMetrics ComputeVerticalScrollbar(const ScrollbarInput& input);

// ----------------------------------------------------------------------------
// Indique si un point touche le pouce de scrollbar.
//
// Parametres :
// - metrics : geometrie calculee.
// - x : position horizontale.
// - y : position verticale.
//
// Retour :
// - true si le point est dans le pouce.
// ----------------------------------------------------------------------------
bool HitTestVerticalScrollbar(const ScrollbarMetrics& metrics, float x, float y);

// ----------------------------------------------------------------------------
// Indique si un point touche la piste interactive de scrollbar.
//
// Parametres :
// - metrics : geometrie calculee.
// - x : position horizontale.
// - y : position verticale.
//
// Retour :
// - true si le point est dans la piste de scrollbar.
// ----------------------------------------------------------------------------
bool HitTestVerticalScrollbarTrack(const ScrollbarMetrics& metrics, float x, float y);

// ----------------------------------------------------------------------------
// Convertit un bord haut de pouce en offset de contenu.
//
// Parametres :
// - metrics : geometrie calculee.
// - thumb_top : nouveau bord haut du pouce.
//
// Retour :
// - offset de scroll borne.
// ----------------------------------------------------------------------------
float ScrollOffsetFromThumbTop(const ScrollbarMetrics& metrics, float thumb_top);

// ============================================================================
// Codex Deck - Layout principal
// ----------------------------------------------------------------------------
// Ce module calcule les rectangles de navigation et Workbench sans connaitre
// Direct2D, Win32 ou l'etat applicatif.
// ============================================================================

#pragma once

// ----------------------------------------------------------------------------
// Taille flottante en DIPs.
// ----------------------------------------------------------------------------
struct SizeF {
    // Largeur.
    float width = 0.0F;

    // Hauteur.
    float height = 0.0F;
};

// ----------------------------------------------------------------------------
// Rectangle flottant en DIPs.
// ----------------------------------------------------------------------------
struct LayoutRect {
    // Bord gauche.
    float left = 0.0F;

    // Bord haut.
    float top = 0.0F;

    // Bord droit.
    float right = 0.0F;

    // Bord bas.
    float bottom = 0.0F;
};

// ----------------------------------------------------------------------------
// Rectangles du layout principal.
// ----------------------------------------------------------------------------
struct MainLayoutRects {
    // Barre d'activite superieure.
    LayoutRect activity_bar;

    // Arbre projets/sessions.
    LayoutRect tree;

    // Separateur visuel redimensionnable.
    LayoutRect splitter;

    // Zone Workbench.
    LayoutRect workbench;

    // Zone composer future en bas du Workbench.
    LayoutRect composer;
};

// ----------------------------------------------------------------------------
// Calcule le layout principal.
//
// Parametres :
// - client : taille cliente totale.
// - tree_width : largeur demandee pour le Tree.
// - activity_height : hauteur de barre d'activite.
// - composer_height : hauteur reservee au composer futur.
//
// Retour :
// - rectangles bornes pour Tree, Workbench et zones associees.
// ----------------------------------------------------------------------------
MainLayoutRects ComputeMainLayout(
    SizeF client,
    float tree_width,
    float activity_height,
    float composer_height
);

// ----------------------------------------------------------------------------
// Borne une largeur de Tree aux limites V1.
//
// Parametres :
// - tree_width : largeur demandee.
//
// Retour :
// - largeur bornee.
// ----------------------------------------------------------------------------
float ClampTreeWidth(float tree_width);

// ----------------------------------------------------------------------------
// Indique si une position horizontale touche le separateur principal.
//
// Parametres :
// - rects : rectangles calcules du layout.
// - x : position horizontale en DIPs.
//
// Retour :
// - true si la position est dans la zone de hit elargie du separateur.
// ----------------------------------------------------------------------------
bool HitTestMainSplitter(const MainLayoutRects& rects, float x);

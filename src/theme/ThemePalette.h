// ============================================================================
// Codex Deck - Palette de theme
// ----------------------------------------------------------------------------
// Ce module decrit les couleurs Direct2D resolues. Il ne lit aucun parametre
// systeme et ne possede aucune ressource graphique.
// ============================================================================

#pragma once

#include <d2d1.h>

// ----------------------------------------------------------------------------
// Regroupe les couleurs semantiques de la coquille native.
// ----------------------------------------------------------------------------
struct ThemePalette {
    // Couleur du fond principal de la fenetre.
    D2D1_COLOR_F window_background{};

    // Couleur des surfaces secondaires.
    D2D1_COLOR_F surface{};

    // Couleur des surfaces secondaires survolees.
    D2D1_COLOR_F surface_hover{};

    // Couleur des bordures discretes.
    D2D1_COLOR_F border{};

    // Couleur du texte principal.
    D2D1_COLOR_F text{};

    // Couleur du texte secondaire.
    D2D1_COLOR_F text_muted{};

    // Couleur d'accent principale.
    D2D1_COLOR_F accent{};

    // Couleur des etats de succes.
    D2D1_COLOR_F success{};

    // Couleur des avertissements.
    D2D1_COLOR_F warning{};

    // Couleur des erreurs.
    D2D1_COLOR_F error{};
};

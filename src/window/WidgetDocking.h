// ============================================================================
// Codex Glass - Aimantation du widget aux bords d'ecran
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers de docking magnetique de la fenetre.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Ajuste un rectangle de deplacement pour l'aimanter aux bords de l'ecran.
//
// Parametres :
// - window_rect : rectangle de fenetre en coordonnees ecran, modifie sur place.
// ----------------------------------------------------------------------------
void DockWindowRectToScreenEdges(RECT& window_rect);

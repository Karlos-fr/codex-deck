// ============================================================================
// Codex Glass - Bandeau du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier declare le rendu et le suivi du bandeau de marque du menu. Il ne
// construit pas les menus et ne gere pas les items owner-draw generiques.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Active le suivi du curseur du bandeau.
// ----------------------------------------------------------------------------
void BeginMenuBannerCursorTracking();

// ----------------------------------------------------------------------------
// Desactive le suivi du curseur du bandeau.
// ----------------------------------------------------------------------------
void EndMenuBannerCursorTracking();

// ----------------------------------------------------------------------------
// Mesure le bandeau de marque en tete du menu.
//
// Parametres :
// - measure_item : structure Win32 a renseigner.
//
// Retour :
// - true si la mesure correspond au bandeau.
// ----------------------------------------------------------------------------
bool MeasureMenuBanner(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Dessine le bandeau de marque en tete du menu.
//
// Parametres :
// - draw_item : structure Win32 de dessin.
// ----------------------------------------------------------------------------
void DrawMenuBanner(DRAWITEMSTRUCT* draw_item);

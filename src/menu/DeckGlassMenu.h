// ============================================================================
// Codex Deck - Menu contextuel Glass
// ----------------------------------------------------------------------------
// Ce module compose les controles owner-drawn de Codex Glass pour regler le
// Workbench. Il ne possede ni le runtime de capture ni le renderer.
// ============================================================================

#pragma once

#include "../glass/DeckGlassSettings.h"

#include <windows.h>

// Message emis par les sliders Glass pendant leur manipulation.
constexpr UINT kDeckGlassSliderChangedMessage = WM_APP + 22;

// ----------------------------------------------------------------------------
// Affiche le menu Glass a la position ecran demandee.
// ----------------------------------------------------------------------------
void ShowDeckGlassMenu(HWND hwnd, POINT point, const DeckGlassSettings& settings, COLORREF accent);

// ----------------------------------------------------------------------------
// Applique une commande discrete du menu aux reglages.
//
// Retour : true si la commande appartenait au menu Glass.
// ----------------------------------------------------------------------------
bool HandleDeckGlassMenuCommand(UINT command_id, DeckGlassSettings& settings);

// ----------------------------------------------------------------------------
// Applique la valeur d'un slider Glass.
//
// Retour : true si le slider appartenait au menu Glass.
// ----------------------------------------------------------------------------
bool HandleDeckGlassSlider(UINT command_id, int value, DeckGlassSettings& settings);

// ----------------------------------------------------------------------------
// Mesure un composant owner-drawn du menu Glass.
// ----------------------------------------------------------------------------
bool MeasureDeckGlassMenuItem(MEASUREITEMSTRUCT* item);

// ----------------------------------------------------------------------------
// Dessine un composant owner-drawn du menu Glass.
// ----------------------------------------------------------------------------
bool DrawDeckGlassMenuItem(DRAWITEMSTRUCT* item);

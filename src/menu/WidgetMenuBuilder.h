// ============================================================================
// Codex Glass - Construction du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier declare la construction metier des menus Win32. Le rendu
// owner-draw et les sliders restent portes par leurs modules dedies.
// ============================================================================

#pragma once

#include "WidgetMenu.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Cree le menu contextuel principal du widget et du tray.
//
// Parametres :
// - state : etat courant necessaire aux coches et libelles.
//
// Retour :
// - handle du menu cree.
// ----------------------------------------------------------------------------
HMENU CreateContextMenu(const WidgetMenuState& state);

// ============================================================================
// Codex Glass - Palette Direct2D du widget
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers de conversion couleur utilises par le rendu,
// sans creer ni posseder de brosses Direct2D.
// ============================================================================

#pragma once

#include "WidgetRenderTypes.h"
#include "../settings/AppSettings.h"

#include <d2d1.h>
#include <windows.h>

// ----------------------------------------------------------------------------
// Convertit une couleur Win32 en couleur Direct2D.
//
// Parametres :
// - color : couleur Win32 a convertir.
// - alpha : opacite Direct2D a appliquer.
//
// Retour :
// - couleur Direct2D normalisee.
// ----------------------------------------------------------------------------
D2D1_COLOR_F ColorRefToD2DColor(COLORREF color, float alpha = 1.0F);

// ----------------------------------------------------------------------------
// Retourne une palette derivee des couleurs actuellement configurees.
//
// Parametres :
// - settings : reglages contenant les couleurs utilisateur.
//
// Retour :
// - palette Direct2D prete pour la creation des brosses.
// ----------------------------------------------------------------------------
Palette ActivePalette(const AppSettings& settings);

// ============================================================================
// Codex Glass - Fenetre outil du panneau de couleurs
// ----------------------------------------------------------------------------
// Ce fichier declare la fenetre flottante ancree au widget. Le rendu et les
// actions de couleur restent respectivement dans rendering et WidgetApp.
// ============================================================================

#pragma once

#include <windows.h>

// Hauteur logique stable de la palette de couleurs en DIPs.
constexpr int kWidgetColorPanelWindowHeight = 204;

// ----------------------------------------------------------------------------
// Enregistre la classe Win32 de la palette de couleurs.
//
// Parametres :
// - instance : instance qui possede la classe.
// - window_proc : procedure de fenetre de la palette.
//
// Retour :
// - true si la classe est disponible.
// ----------------------------------------------------------------------------
bool RegisterWidgetColorPanelWindowClass(HINSTANCE instance, WNDPROC window_proc);

// ----------------------------------------------------------------------------
// Cree la palette de couleurs comme fenetre outil possedee par le widget.
//
// Parametres :
// - instance : instance de l'application.
// - owner : fenetre principale proprietaire.
// - create_parameter : pointeur transmis a WM_NCCREATE.
//
// Retour :
// - handle cree, ou nullptr en cas d'echec.
// ----------------------------------------------------------------------------
HWND CreateWidgetColorPanelWindow(HINSTANCE instance, HWND owner, void* create_parameter);

// ----------------------------------------------------------------------------
// Ancre la palette a droite du widget, ou a gauche si la place manque.
//
// Parametres :
// - panel_hwnd : palette a positionner.
// - owner_hwnd : widget servant d'ancre et de reference DPI.
// ----------------------------------------------------------------------------
void PositionWidgetColorPanelWindow(HWND panel_hwnd, HWND owner_hwnd);

// ----------------------------------------------------------------------------
// Ancre la palette sur un rectangle futur du widget pendant son deplacement.
//
// Parametres :
// - panel_hwnd : palette a positionner.
// - owner_hwnd : widget servant de reference DPI et de z-order.
// - owner_rect : rectangle ecran propose pour le widget.
// ----------------------------------------------------------------------------
void PositionWidgetColorPanelWindowForRect(HWND panel_hwnd, HWND owner_hwnd, const RECT& owner_rect);

// ============================================================================
// Codex Deck - Fenetre principale Win32
// ----------------------------------------------------------------------------
// Ce module isole l'enregistrement et la creation de la fenetre principale. Il
// ne gere ni le rendu, ni l'orchestration applicative.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Retourne la taille minimale du client principal.
//
// Retour :
// - dimensions minimales en pixels logiques Win32.
// ----------------------------------------------------------------------------
SIZE DeckMinimumClientSize();

// ----------------------------------------------------------------------------
// Enregistre la classe Win32 de la fenetre principale.
//
// Parametres :
// - instance : instance du module executable.
// - window_proc : procedure de fenetre fournie par l'orchestrateur.
//
// Retour :
// - true si la classe est disponible ou deja enregistree.
// ----------------------------------------------------------------------------
bool RegisterDeckWindowClass(HINSTANCE instance, WNDPROC window_proc);

// ----------------------------------------------------------------------------
// Cree la fenetre principale redimensionnable.
//
// Parametres :
// - instance : instance du module executable.
// - window_proc : procedure de fenetre associee a la classe.
// - create_parameter : pointeur transmis a WM_NCCREATE.
//
// Retour :
// - handle de fenetre cree, ou nullptr en cas d'erreur Win32.
// ----------------------------------------------------------------------------
HWND CreateDeckMainWindow(HINSTANCE instance, WNDPROC window_proc, void* create_parameter);

// ----------------------------------------------------------------------------
// Applique les attributs systeme correspondant au theme demande.
//
// Parametres :
// - hwnd : fenetre a mettre a jour.
// - dark : true pour demander le mode sombre systeme.
// ----------------------------------------------------------------------------
void ApplyDeckWindowTheme(HWND hwnd, bool dark);

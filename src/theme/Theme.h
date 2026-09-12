// ============================================================================
// Codex Deck - Resolution de theme
// ----------------------------------------------------------------------------
// Ce module convertit le choix utilisateur System/Light/Dark en theme resolu et
// applique les attributs Windows associes. Il ne dessine rien.
// ============================================================================

#pragma once

#include "ThemePalette.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Liste les modes de theme demandes par l'utilisateur.
// ----------------------------------------------------------------------------
enum class ThemeMode {
    // Suit le theme applicatif Windows.
    System,

    // Force la palette claire.
    Light,

    // Force la palette sombre.
    Dark,
};

// ----------------------------------------------------------------------------
// Liste les themes concrets utilises par le rendu.
// ----------------------------------------------------------------------------
enum class ResolvedTheme {
    // Palette claire concrete.
    Light,

    // Palette sombre concrete.
    Dark,
};

// ----------------------------------------------------------------------------
// Resolut un mode utilisateur en theme concret.
//
// Parametres :
// - requested : mode choisi par l'utilisateur.
// - system_dark : etat sombre du theme applicatif Windows.
//
// Retour :
// - theme concret a appliquer.
// ----------------------------------------------------------------------------
ResolvedTheme ResolveTheme(ThemeMode requested, bool system_dark);

// ----------------------------------------------------------------------------
// Indique si Windows demande le theme applicatif sombre.
//
// Retour :
// - true si AppsUseLightTheme vaut zero, false en cas de theme clair ou erreur.
// ----------------------------------------------------------------------------
bool IsSystemDarkTheme();

// ----------------------------------------------------------------------------
// Applique les attributs systeme de fenetre pour un theme resolu.
//
// Parametres :
// - hwnd : fenetre a mettre a jour.
// - theme : theme concret a transmettre a DWM.
// ----------------------------------------------------------------------------
void ApplySystemWindowTheme(HWND hwnd, ResolvedTheme theme);

// ----------------------------------------------------------------------------
// Retourne la palette correspondant au theme resolu.
//
// Parametres :
// - theme : theme concret.
//
// Retour :
// - couleurs semantiques stables pour Direct2D.
// ----------------------------------------------------------------------------
ThemePalette PaletteForTheme(ResolvedTheme theme);

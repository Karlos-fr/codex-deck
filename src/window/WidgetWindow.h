// ============================================================================
// Codex Glass - Fenetre Win32 du widget
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers Win32 lies a la fenetre principale, au DWM,
// au dark mode natif, a la position et au DPI.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"
#include "../usage/UsageSnapshot.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Enregistre la classe Win32 de la fenetre principale.
//
// Parametres :
// - instance : handle de l'instance courante de l'application.
// - window_proc : procedure de fenetre qui traitera les messages.
//
// Retour :
// - true si la classe de fenetre a ete enregistree correctement.
// - false si l'enregistrement a echoue.
// ----------------------------------------------------------------------------
bool RegisterMainWindowClass(HINSTANCE instance, WNDPROC window_proc);

// ----------------------------------------------------------------------------
// Cree la fenetre principale de l'application.
//
// Parametres :
// - instance : handle de l'instance courante de l'application.
// - settings : reglages initiaux de la fenetre.
// - create_parameter : pointeur transmis a WM_NCCREATE.
//
// Retour :
// - handle de la fenetre creee, ou nullptr en cas d'erreur.
// ----------------------------------------------------------------------------
HWND CreateMainWindow(HINSTANCE instance, const AppSettings& settings, void* create_parameter);

// ----------------------------------------------------------------------------
// Calcule une position initiale visible sur l'ecran principal.
//
// Retour :
// - rectangle initial conseille pour le widget.
// ----------------------------------------------------------------------------
RECT CalculateInitialWindowRect();

// ----------------------------------------------------------------------------
// Retourne un rectangle visible, en remplacant un ancien ecran disparu.
//
// Parametres :
// - requested_rect : rectangle charge depuis les reglages.
//
// Retour :
// - rectangle conserve s'il est visible, sinon rectangle initial par defaut.
// ----------------------------------------------------------------------------
RECT EnsureWindowRectVisible(const RECT& requested_rect);

// ----------------------------------------------------------------------------
// Replace le widget si son rectangle courant n'est plus sur un ecran visible.
//
// Parametres :
// - hwnd : handle de la fenetre a verifier.
// - applied_rect : rectangle applique si la fenetre a ete deplacee.
//
// Retour :
// - true si la fenetre a ete replacee.
// - false sinon.
// ----------------------------------------------------------------------------
bool EnsureCurrentWindowVisible(HWND hwnd, RECT& applied_rect);

// ----------------------------------------------------------------------------
// Applique le rectangle conseille par Windows apres un changement de DPI.
//
// Parametres :
// - hwnd : handle de la fenetre a repositionner.
// - suggested_rect : rectangle recommande par le message WM_DPICHANGED.
//
// Retour :
// - true si le rectangle a ete applique.
// - false sinon.
// ----------------------------------------------------------------------------
bool ApplyDpiSuggestedRect(HWND hwnd, const RECT* suggested_rect);

// ----------------------------------------------------------------------------
// Retourne la hauteur de fenetre adaptee au mode courant.
//
// Parametres :
// - settings : reglages d'affichage courants.
// - snapshot : releve utilise pour compter les quotas visibles.
//
// Retour :
// - hauteur en pixels.
// ----------------------------------------------------------------------------
int PreferredWindowHeight(const AppSettings& settings, const UsageSnapshot& snapshot);

// ----------------------------------------------------------------------------
// Ajuste la hauteur de fenetre pour le mode d'affichage courant.
//
// Parametres :
// - hwnd : handle de la fenetre a redimensionner.
// - settings : reglages d'affichage courants.
// ----------------------------------------------------------------------------
void ApplyPreferredWindowHeight(HWND hwnd, const AppSettings& settings, const UsageSnapshot& snapshot);

// ----------------------------------------------------------------------------
// Retourne la largeur de fenetre adaptee au mode courant.
//
// Parametres :
// - settings : reglages d'affichage courants.
// - snapshot : releve utilise pour compter les quotas visibles.
//
// Retour :
// - largeur en pixels.
// ----------------------------------------------------------------------------
int PreferredWindowWidth(const AppSettings& settings, const UsageSnapshot& snapshot);

// ----------------------------------------------------------------------------
// Ajuste la taille de fenetre pour le mode courant.
//
// Parametres :
// - hwnd : handle de la fenetre a redimensionner.
// - settings : reglages d'affichage courants.
// ----------------------------------------------------------------------------
void ApplyPreferredWindowSize(HWND hwnd, const AppSettings& settings, const UsageSnapshot& snapshot);

// ----------------------------------------------------------------------------
// Applique les limites de taille adaptees au mode courant.
//
// Parametres :
// - min_max_info : structure Win32 a renseigner.
// - settings : reglages d'affichage courants.
// ----------------------------------------------------------------------------
void ApplyModeMinimumSize(
    HWND hwnd,
    MINMAXINFO* min_max_info,
    const AppSettings& settings,
    const UsageSnapshot& snapshot
);

// ----------------------------------------------------------------------------
// Applique le reglage de premier plan a la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a mettre a jour.
// - always_on_top : indique si la fenetre doit rester au premier plan.
// ----------------------------------------------------------------------------
void ApplyAlwaysOnTopSetting(HWND hwnd, bool always_on_top);

// ----------------------------------------------------------------------------
// Applique les styles d'interaction et d'opacite a la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a mettre a jour.
// - settings : reglages d'interaction et d'opacite.
// ----------------------------------------------------------------------------
void ApplyClickThroughSetting(HWND hwnd, const AppSettings& settings);

// ----------------------------------------------------------------------------
// Applique le dark mode natif a la fenetre et aux menus Win32.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void ApplyNativeDarkMode(HWND hwnd);

// ----------------------------------------------------------------------------
// Indique si Windows demande le theme sombre natif.
//
// Retour :
// - true si le theme sombre systeme est actif.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsNativeDarkModeEnabled();

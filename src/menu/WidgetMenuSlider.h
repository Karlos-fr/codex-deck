// ============================================================================
// Codex Glass - Sliders dans les menus
// ----------------------------------------------------------------------------
// Ce fichier declare un item owner-drawn reutilisable pour manipuler des valeurs
// en pourcentage directement depuis un menu contextuel Win32.
// ============================================================================

#pragma once

#include <windows.h>

#include <string>
#include <vector>

// Message envoye a la fenetre principale quand le slider d'opacite change.
constexpr UINT kOpacityMenuSliderChangedMessage = WM_APP + 12;

// Message generique envoye quand un parametre continu des Effets Motion change.
constexpr UINT kMotionEffectMenuSliderChangedMessage = WM_APP + 19;

// Message generique envoye quand un parametre GlassEffect change.
constexpr UINT kGlassEffectMenuSliderChangedMessage = WM_APP + 22;

// Message generique envoye quand un parametre de la veine change.
constexpr UINT kActivityVeinMenuSliderChangedMessage = WM_APP + 25;

// ----------------------------------------------------------------------------
// Decrit un slider affiche dans un menu.
// ----------------------------------------------------------------------------
struct WidgetMenuSliderSpec {
    // Commande de l'item owner-drawn.
    UINT command_id = 0;

    // Libelle affiche a gauche.
    std::wstring label{};

    // Message envoye a la fenetre principale quand la valeur change.
    UINT changed_message = 0;

    // Valeur minimale acceptee.
    int minimum_value = 0;

    // Valeur maximale acceptee.
    int maximum_value = 100;

    // Valeur courante affichee.
    int current_value = 100;

    // Suffixe affiche apres la valeur.
    const wchar_t* value_suffix = L" %";
};

// ----------------------------------------------------------------------------
// Ajoute l'item owner-drawn d'un slider a un menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// - command_id : identifiant de l'item owner-drawn.
// ----------------------------------------------------------------------------
void AppendWidgetMenuSliderItem(HMENU menu, UINT command_id);

// ----------------------------------------------------------------------------
// Active le suivi souris des sliders pendant l'affichage du menu.
//
// Parametres :
// - hwnd : fenetre principale qui recevra les changements.
// - sliders : sliders actifs dans le menu et ses sous-menus.
// - active_color : couleur thematique du remplissage actif.
// ----------------------------------------------------------------------------
void BeginWidgetMenuSliderTracking(
    HWND hwnd,
    const std::vector<WidgetMenuSliderSpec>& sliders,
    COLORREF active_color
);

// ----------------------------------------------------------------------------
// Desactive le suivi souris des sliders.
// ----------------------------------------------------------------------------
void EndWidgetMenuSliderTracking();

// ----------------------------------------------------------------------------
// Met a jour la valeur d'un slider visible sans fermer le menu.
//
// Parametres :
// - command_id : commande du slider a synchroniser.
// - value : nouvelle valeur, bornee par la presentation courante.
// ----------------------------------------------------------------------------
void UpdateWidgetMenuSliderValue(UINT command_id, int value);

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les sliders.
//
// Parametres :
// - measure_item : structure Win32 a remplir.
//
// Retour :
// - true si le message concernait un slider.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuSlider(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les sliders.
//
// Parametres :
// - draw_item : structure Win32 a dessiner.
//
// Retour :
// - true si le message concernait un slider.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuSlider(DRAWITEMSTRUCT* draw_item);

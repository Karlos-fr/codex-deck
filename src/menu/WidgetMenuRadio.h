// ============================================================================
// Codex Glass - Boutons radio dans les menus
// ----------------------------------------------------------------------------
// Ce fichier declare un item owner-drawn pour les choix exclusifs. Il reste
// independant des checkboxes booleennes et des sliders.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <windows.h>

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Decrit un bouton radio affiche dans un menu.
// ----------------------------------------------------------------------------
struct WidgetMenuRadioSpec {
    // Commande de l'item owner-drawn.
    UINT command_id = 0;

    // Libelle affiche a droite du bouton radio.
    std::wstring text{};

    // Indique si cette option est selectionnee.
    bool selected = false;

    // Groupe exclusif auquel appartient l'option.
    UINT group_id = 0;
};

// ----------------------------------------------------------------------------
// Ajoute un bouton radio owner-drawn a un menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// - command_id : identifiant de l'option.
// ----------------------------------------------------------------------------
void AppendWidgetMenuRadioItem(HMENU menu, UINT command_id);

// ----------------------------------------------------------------------------
// Active le suivi souris des boutons radio pendant l'affichage du menu.
//
// Parametres :
// - hwnd : fenetre principale qui recevra les commandes.
// - radios : options exclusives actives dans les menus.
// - active_color : couleur thematique de l'option selectionnee.
// ----------------------------------------------------------------------------
void BeginWidgetMenuRadioTracking(
    HWND hwnd,
    const std::vector<WidgetMenuRadioSpec>& radios,
    COLORREF active_color
);

// ----------------------------------------------------------------------------
// Desactive le suivi souris des boutons radio.
// ----------------------------------------------------------------------------
void EndWidgetMenuRadioTracking();

// ----------------------------------------------------------------------------
// Retraduit les libelles des radios du menu encore ouvert.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeWidgetMenuRadios(UiLanguage previous_language);

// ----------------------------------------------------------------------------
// Selectionne un bouton radio visible sans emettre sa commande.
//
// Parametres :
// - command_id : commande du bouton a selectionner.
// ----------------------------------------------------------------------------
void SelectWidgetMenuRadioItem(UINT command_id);

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les boutons radio.
//
// Parametres :
// - measure_item : structure Win32 a remplir.
//
// Retour :
// - true si le message concernait un bouton radio.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuRadio(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les boutons radio.
//
// Parametres :
// - draw_item : structure Win32 a dessiner.
//
// Retour :
// - true si le message concernait un bouton radio.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuRadio(DRAWITEMSTRUCT* draw_item);

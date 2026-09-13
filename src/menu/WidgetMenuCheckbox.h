// ============================================================================
// Codex Glass - Checkboxes dans les menus
// ----------------------------------------------------------------------------
// Ce fichier declare un item owner-drawn reutilisable pour basculer des options
// booleennes depuis un menu Win32 sans fermer le menu contextuel.
// ============================================================================

#pragma once

#include "../localization/Localization.h"

#include <windows.h>

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Mode d'interaction d'une checkbox de menu.
// ----------------------------------------------------------------------------
enum class WidgetMenuCheckboxMode {
    // L'item inverse son etat a chaque clic.
    Toggle,

    // L'item devient coche et decoche les autres items du meme groupe.
    Select,
};

// ----------------------------------------------------------------------------
// Decrit une checkbox affichee dans un menu.
// ----------------------------------------------------------------------------
struct WidgetMenuCheckboxSpec {
    // Commande de l'item owner-drawn.
    UINT command_id = 0;

    // Libelle affiche quand l'option est cochee.
    std::wstring checked_text{};

    // Libelle affiche quand l'option est decochee.
    std::wstring unchecked_text{};

    // Etat courant affiche.
    bool checked = false;

    // Mode d'interaction applique au clic.
    WidgetMenuCheckboxMode mode = WidgetMenuCheckboxMode::Toggle;

    // Groupe logique des items exclusifs.
    UINT group_id = 0;
};

// ----------------------------------------------------------------------------
// Ajoute l'item owner-drawn d'une checkbox a un menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// - command_id : identifiant de l'item owner-drawn.
// ----------------------------------------------------------------------------
void AppendWidgetMenuCheckboxItem(HMENU menu, UINT command_id);

// ----------------------------------------------------------------------------
// Active le suivi souris des checkboxes pendant l'affichage du menu.
//
// Parametres :
// - hwnd : fenetre principale qui recevra les commandes.
// - checkboxes : checkboxes actives dans le menu et ses sous-menus.
// - active_color : couleur thematique des cases cochees.
// ----------------------------------------------------------------------------
void BeginWidgetMenuCheckboxTracking(
    HWND hwnd,
    const std::vector<WidgetMenuCheckboxSpec>& checkboxes,
    COLORREF active_color
);

// ----------------------------------------------------------------------------
// Desactive le suivi souris des checkboxes.
// ----------------------------------------------------------------------------
void EndWidgetMenuCheckboxTracking();

// ----------------------------------------------------------------------------
// Retraduit les libelles des checkboxes du menu encore ouvert.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeWidgetMenuCheckboxes(UiLanguage previous_language);

// ----------------------------------------------------------------------------
// Met a jour une checkbox visible sans emettre sa commande.
//
// Parametres :
// - command_id : commande de la checkbox a synchroniser.
// - checked : nouvel etat coche.
// ----------------------------------------------------------------------------
void UpdateWidgetMenuCheckboxChecked(UINT command_id, bool checked);

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les checkboxes.
//
// Parametres :
// - measure_item : structure Win32 a remplir.
//
// Retour :
// - true si le message concernait une checkbox.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuCheckbox(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les checkboxes.
//
// Parametres :
// - draw_item : structure Win32 a dessiner.
//
// Retour :
// - true si le message concernait une checkbox.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuCheckbox(DRAWITEMSTRUCT* draw_item);

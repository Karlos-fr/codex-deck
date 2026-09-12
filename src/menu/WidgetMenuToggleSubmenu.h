// ============================================================================
// Codex Glass - Sous-menu activable
// ----------------------------------------------------------------------------
// Ce fichier declare un item owner-drawn combinant une checkbox et un sous-menu.
// Seule la checkbox bascule l'etat ; le libelle conserve le comportement du sous-menu.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <windows.h>

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Decrit un sous-menu dont la fonctionnalite peut etre activee independamment.
// ----------------------------------------------------------------------------
struct WidgetMenuToggleSubmenuSpec {
    // Commande envoyee lorsque l'etat d'activation est inverse.
    UINT command_id = 0;

    // Libelle affiche quand la fonctionnalite est activee.
    std::wstring checked_text{};

    // Libelle affiche quand la fonctionnalite est desactivee.
    std::wstring unchecked_text{};

    // Etat d'activation courant.
    bool checked = false;

    // Indique si l'item accepte les interactions.
    bool enabled = true;
};

// ----------------------------------------------------------------------------
// Ajoute un sous-menu activable owner-drawn a un menu parent.
//
// Parametres :
// - menu : menu parent a alimenter.
// - submenu : sous-menu deja cree et transfere au menu parent.
// - command_id : commande de bascule associee a l'item.
// ----------------------------------------------------------------------------
void AppendWidgetMenuToggleSubmenuItem(HMENU menu, HMENU submenu, UINT command_id);

// ----------------------------------------------------------------------------
// Active le suivi souris des sous-menus activables pendant TrackPopupMenu.
//
// Parametres :
// - hwnd : fenetre qui recoit les commandes de bascule.
// - items : descriptions des sous-menus activables de l'affichage courant.
// - active_color : couleur thematique des cases cochees.
// ----------------------------------------------------------------------------
void BeginWidgetMenuToggleSubmenuTracking(
    HWND hwnd,
    const std::vector<WidgetMenuToggleSubmenuSpec>& items,
    COLORREF active_color
);

// ----------------------------------------------------------------------------
// Desactive le suivi souris et libere l'etat temporaire du composant.
// ----------------------------------------------------------------------------
void EndWidgetMenuToggleSubmenuTracking();

// ----------------------------------------------------------------------------
// Retraduit les libelles des sous-menus activables encore ouverts.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeWidgetMenuToggleSubmenus(UiLanguage previous_language);

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les sous-menus activables.
//
// Parametres :
// - measure_item : structure Win32 a renseigner.
//
// Retour :
// - true si l'item appartient a ce composant.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuToggleSubmenu(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les sous-menus activables.
//
// Parametres :
// - draw_item : structure Win32 de dessin.
//
// Retour :
// - true si l'item appartient a ce composant.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuToggleSubmenu(DRAWITEMSTRUCT* draw_item);

// ============================================================================
// Codex Glass - Rendu owner-draw du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers de creation et de peinture des items de menu
// owner-draw. Il ne connait pas la composition metier du menu.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Reinitialise l'etat temporaire des items owner-draw.
// ----------------------------------------------------------------------------
void ResetOwnerMenuDrawState();

// ----------------------------------------------------------------------------
// Active le polissage des fenetres de menu creees pendant TrackPopupMenu.
// ----------------------------------------------------------------------------
void BeginOwnerMenuWindowPolish();

// ----------------------------------------------------------------------------
// Desactive le polissage des fenetres de menu.
// ----------------------------------------------------------------------------
void EndOwnerMenuWindowPolish();

// ----------------------------------------------------------------------------
// Retraduit les items owner-draw et redessine les popups visibles.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeOwnerMenuItems(UiLanguage previous_language);

// ----------------------------------------------------------------------------
// Cree un popup menu configure pour le rendu owner-draw.
//
// Retour :
// - handle du menu cree.
// ----------------------------------------------------------------------------
HMENU CreateOwnerPopupMenu();

// ----------------------------------------------------------------------------
// Ajoute une commande simple au menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// - command_id : identifiant de commande envoye par WM_COMMAND.
// - text : libelle affiche.
// - checked : indique si l'entree doit etre cochee.
// ----------------------------------------------------------------------------
void AppendCommandMenuItem(HMENU menu, UINT command_id, const wchar_t* text, bool checked = false);

// ----------------------------------------------------------------------------
// Ajoute une commande simple dont la largeur suit son libelle.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// - command_id : identifiant de commande envoye par WM_COMMAND.
// - text : libelle affiche.
// - checked : indique si l'entree doit etre cochee.
// ----------------------------------------------------------------------------
void AppendCompactCommandMenuItem(
    HMENU menu,
    UINT command_id,
    const wchar_t* text,
    bool checked = false
);

// ----------------------------------------------------------------------------
// Ajoute un separateur owner-draw au menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// ----------------------------------------------------------------------------
void AppendMenuSeparator(HMENU menu);

// ----------------------------------------------------------------------------
// Ajoute un sous-menu au menu parent.
//
// Parametres :
// - menu : menu parent.
// - submenu : sous-menu deja cree.
// - text : libelle affiche pour le sous-menu.
// - enabled : indique si le sous-menu est utilisable.
// ----------------------------------------------------------------------------
void AppendSubMenu(HMENU menu, HMENU submenu, const wchar_t* text, bool enabled = true);

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les items owner-drawn du menu contextuel.
//
// Parametres :
// - measure_item : structure Win32 a remplir.
//
// Retour :
// - true si le message a ete traite.
// ----------------------------------------------------------------------------
bool MeasureOwnerMenuItem(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les items owner-drawn du menu contextuel.
//
// Parametres :
// - draw_item : structure Win32 a dessiner.
//
// Retour :
// - true si le message a ete traite.
// ----------------------------------------------------------------------------
bool DrawOwnerMenuItem(DRAWITEMSTRUCT* draw_item);

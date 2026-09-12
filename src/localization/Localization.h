// ============================================================================
// Codex Glass - Chargement des textes localises
// ----------------------------------------------------------------------------
// Ce fichier declare la petite couche de localisation chargeant les chaines
// embarquees dans les ressources Windows de l'executable.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <string>

// ----------------------------------------------------------------------------
// Applique la langue d'interface demandee.
//
// Parametres :
// - language : langue configuree par l'utilisateur ou mode automatique.
// ----------------------------------------------------------------------------
void SetUiLanguage(UiLanguage language);

// ----------------------------------------------------------------------------
// Retourne la langue d'interface demandee.
//
// Retour :
// - langue configuree par l'utilisateur ou mode automatique.
// ----------------------------------------------------------------------------
UiLanguage CurrentUiLanguage();

// ----------------------------------------------------------------------------
// Retourne la langue effectivement utilisee pour les ressources.
//
// Retour :
// - langue francaise ou anglaise apres resolution du mode automatique.
// ----------------------------------------------------------------------------
UiLanguage ActiveUiLanguage();

// ----------------------------------------------------------------------------
// Charge un texte localise depuis les ressources embarquees.
//
// Parametres :
// - resource_id : identifiant de chaine defini dans ResourceIds.h.
//
// Retour :
// - texte localise ou libelle technique de secours.
// ----------------------------------------------------------------------------
std::wstring T(unsigned int resource_id);

// ----------------------------------------------------------------------------
// Retraduit un texte provenant de la langue precedente vers la langue active.
//
// Parametres :
// - text : texte potentiellement charge depuis une ressource localisee.
// - previous_language : langue effective utilisee avant le changement.
//
// Retour :
// - traduction courante lorsque la ressource est identifiable sans ambiguite,
//   sinon texte original.
// ----------------------------------------------------------------------------
std::wstring RelocalizeText(const std::wstring& text, UiLanguage previous_language);

// ----------------------------------------------------------------------------
// Charge un texte localise puis remplace un parametre texte.
//
// Parametres :
// - resource_id : identifiant de chaine defini dans ResourceIds.h.
// - value : valeur injectee dans le premier parametre %s.
//
// Retour :
// - texte localise formate.
// ----------------------------------------------------------------------------
std::wstring FormatT(unsigned int resource_id, const std::wstring& value);

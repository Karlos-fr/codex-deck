// ============================================================================
// Codex Glass - Messages shell Windows
// ----------------------------------------------------------------------------
// Ce fichier centralise les identifiants de messages Win32 utilises pour les
// integrations shell. Il garde ces numeros hors de WidgetApp, qui se limite a
// orchestrer les reactions applicatives.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Retourne le message prive envoye par l'icone de notification a la fenetre.
//
// Retour :
// - identifiant Win32 du message tray.
// ----------------------------------------------------------------------------
UINT TrayIconMessage();

// ----------------------------------------------------------------------------
// Retourne le message diffuse par Windows quand la barre des taches est creee.
//
// Retour :
// - identifiant Win32 du message TaskbarCreated.
// ----------------------------------------------------------------------------
UINT TaskbarCreatedMessage();

// ----------------------------------------------------------------------------
// Indique si un message Win32 correspond a la recreation de la barre des taches.
//
// Parametres :
// - message : identifiant du message recu.
//
// Retour :
// - true si le message est TaskbarCreated.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsTaskbarCreatedMessage(UINT message);

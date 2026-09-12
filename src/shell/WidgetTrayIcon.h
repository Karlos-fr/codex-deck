// ============================================================================
// Codex Glass - Icone de notification Windows
// ----------------------------------------------------------------------------
// Ce fichier declare l'integration tray icon du widget. Il encapsule les appels
// Shell_NotifyIcon et laisse WidgetApp decider du comportement associe aux
// messages souris recus depuis la zone de notification.
// ============================================================================

#pragma once

#include <windows.h>

#include <string_view>

// ----------------------------------------------------------------------------
// Ajoute l'icone de l'application dans la zone de notification Windows.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire de l'icone.
// - tooltip : texte compact affiche au survol de l'icone.
// ----------------------------------------------------------------------------
void AddTrayIcon(HWND hwnd, std::wstring_view tooltip);

// ----------------------------------------------------------------------------
// Retire l'icone de l'application de la zone de notification Windows.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire de l'icone.
// ----------------------------------------------------------------------------
void RemoveTrayIcon(HWND hwnd);

// ----------------------------------------------------------------------------
// Met a jour le libelle de l'icone de notification deja installee.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire de l'icone.
// - tooltip : nouveau texte compact affiche au survol de l'icone.
// ----------------------------------------------------------------------------
void RefreshTrayIcon(HWND hwnd, std::wstring_view tooltip);

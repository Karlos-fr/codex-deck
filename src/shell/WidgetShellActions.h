// ============================================================================
// Codex Glass - Actions shell Windows
// ----------------------------------------------------------------------------
// Ce fichier declare les actions deleguees au shell Windows, comme l'ouverture
// d'une URL externe. Il evite aux modules applicatifs de manipuler directement
// ShellExecuteW.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Ouvre le depot GitHub de Codex Glass dans le navigateur par defaut.
//
// Parametres :
// - owner : fenetre proprietaire eventuelle de l'action shell.
//
// Retour :
// - true si Windows accepte la demande d'ouverture.
// - false sinon.
// ----------------------------------------------------------------------------
bool OpenCodexGlassRepository(HWND owner);

// ============================================================================
// Codex Glass - Icone applicative partagee
// ----------------------------------------------------------------------------
// Ce fichier declare le chargement de l'icone embarquee de l'application.
// Il sert de frontiere commune entre la fenetre native et les integrations
// shell qui ont toutes deux besoin de la meme ressource graphique.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Charge l'icone applicative embarquee avec une taille adaptee au contexte.
//
// Parametres :
// - width : largeur demandee en pixels, ou 0 pour la taille par defaut.
// - height : hauteur demandee en pixels, ou 0 pour la taille par defaut.
//
// Retour :
// - handle partage de l'icone applicative, ou icone systeme par defaut en repli.
// ----------------------------------------------------------------------------
HICON LoadApplicationIcon(int width, int height);

// ----------------------------------------------------------------------------
// Charge l'icone simplifiee destinee a la zone de notification.
//
// Parametres :
// - width : largeur demandee en pixels, ou 0 pour la taille par defaut.
// - height : hauteur demandee en pixels, ou 0 pour la taille par defaut.
//
// Retour :
// - handle partage de l'icone tray, ou icone principale en repli.
// ----------------------------------------------------------------------------
HICON LoadTrayIcon(int width, int height);

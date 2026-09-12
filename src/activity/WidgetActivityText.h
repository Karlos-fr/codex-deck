// ============================================================================
// Codex Glass - Textes de l'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier declare la construction des libelles et hints localises a partir
// des seuls compteurs agreges produits par le controleur visuel.
// ============================================================================

#pragma once

#include "WidgetActivityController.h"

#include <string>

// ----------------------------------------------------------------------------
// Construit le libelle permanent compact de l'activite.
//
// Parametres :
// - frame : resume visuel multisession courant.
//
// Retour :
// - etat ou nombre de sessions sans identifiant technique.
// ----------------------------------------------------------------------------
std::wstring BuildWidgetActivityLabel(const WidgetActivityFrame& frame);

// ----------------------------------------------------------------------------
// Construit le hint detaille de la veine.
//
// Parametres :
// - frame : resume visuel multisession courant.
//
// Retour :
// - texte localise combinant etat et compteurs non sensibles.
// ----------------------------------------------------------------------------
std::wstring BuildWidgetActivityHint(const WidgetActivityFrame& frame);

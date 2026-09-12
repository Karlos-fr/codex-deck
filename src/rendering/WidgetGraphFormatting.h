// ============================================================================
// Codex Glass - Formatage partage des graphes
// ----------------------------------------------------------------------------
// Ce fichier declare les libelles temporels et numeriques communs aux vues
// Quotas et Tokens. Il ne dessine rien et ne lit aucune source de donnees.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

// ----------------------------------------------------------------------------
// Convertit une plage de graphe en duree.
//
// Parametres :
// - range : plage de graphe a convertir.
//
// Retour :
// - duree correspondant a la plage.
// ----------------------------------------------------------------------------
std::chrono::seconds GraphRangeDuration(GraphRange range);

// ----------------------------------------------------------------------------
// Retourne le libelle court localise d'une plage de graphe.
//
// Parametres :
// - range : plage a presenter dans un selecteur.
//
// Retour :
// - libelle compact de la plage.
// ----------------------------------------------------------------------------
std::wstring FormatGraphRangeLabel(GraphRange range);

// ----------------------------------------------------------------------------
// Retourne le nombre de buckets horaires demande par une plage Tokens.
//
// Parametres :
// - range : plage horaire selectionnee.
//
// Retour :
// - nombre de buckets compris entre un et vingt-quatre.
// ----------------------------------------------------------------------------
std::size_t TokenGraphBucketCount(GraphRange range);

// ----------------------------------------------------------------------------
// Formate une date locale courte a partir d'un instant systeme.
//
// Parametres :
// - value : instant a convertir dans le fuseau local.
//
// Retour :
// - date courte localisee.
// ----------------------------------------------------------------------------
std::wstring FormatGraphDate(std::chrono::system_clock::time_point value);

// ----------------------------------------------------------------------------
// Formate une cle civile YYYY-MM-DD en date locale courte.
//
// Parametres :
// - key : cle civile normalisee.
//
// Retour :
// - date courte localisee ou cle d'origine si elle est invalide.
// ----------------------------------------------------------------------------
std::wstring FormatGraphDateKey(const std::string& key);

// ----------------------------------------------------------------------------
// Formate une graduation temporelle adaptee a la plage Quotas.
//
// Parametres :
// - value : instant a convertir.
// - range : plage determinant la precision.
//
// Retour :
// - heure ou date courte.
// ----------------------------------------------------------------------------
std::wstring FormatQuotaAxisTime(
    std::chrono::system_clock::time_point value,
    GraphRange range
);

// ----------------------------------------------------------------------------
// Formate l'instant d'un tooltip adapte a la plage Quotas.
//
// Parametres :
// - value : instant a convertir.
// - range : plage determinant la precision.
//
// Retour :
// - heure, date, ou date et heure.
// ----------------------------------------------------------------------------
std::wstring FormatQuotaTooltipTime(
    std::chrono::system_clock::time_point value,
    GraphRange range
);

// ----------------------------------------------------------------------------
// Formate un pourcentage entier pour un tooltip de quota.
//
// Parametres :
// - value : pourcentage a arrondir.
//
// Retour :
// - pourcentage entier suivi de son unite.
// ----------------------------------------------------------------------------
std::wstring FormatGraphPercent(double value);

// ----------------------------------------------------------------------------
// Formate un total de tokens en millions avec une decimale localisee.
//
// Parametres :
// - value : total exact de tokens.
//
// Retour :
// - valeur abregee en millions.
// ----------------------------------------------------------------------------
std::wstring FormatGraphTokenMillions(std::uint64_t value);

// ----------------------------------------------------------------------------
// Formate une graduation entiere de tokens en millions.
//
// Parametres :
// - value : valeur exacte de la graduation.
//
// Retour :
// - libelle entier abrege en millions.
// ----------------------------------------------------------------------------
std::wstring FormatGraphTokenAxisValue(std::uint64_t value);

// ----------------------------------------------------------------------------
// Formate l'abscisse d'un bucket Tokens selon la granularite choisie.
// ----------------------------------------------------------------------------
std::wstring FormatTokenBucketAxis(
    std::chrono::system_clock::time_point bucket_start,
    GraphRange range
);

// ----------------------------------------------------------------------------
// Formate le tooltip d'un bucket Tokens selon sa duree reelle.
// ----------------------------------------------------------------------------
std::wstring FormatTokenBucketTooltip(
    std::chrono::system_clock::time_point bucket_start,
    GraphRange range,
    std::uint64_t total_tokens
);

// ----------------------------------------------------------------------------
// Formate l'heure locale courte d'un bucket horaire.
//
// Parametres :
// - bucket_start : debut UTC non ambigu du bucket.
//
// Retour :
// - libelle localise adapte a l'abscisse.
// ----------------------------------------------------------------------------
std::wstring FormatTokenHourAxis(
    std::chrono::system_clock::time_point bucket_start
);

// ----------------------------------------------------------------------------
// Formate le tooltip complet d'une cellule quotidienne.
//
// Parametres :
// - date_key : date civile locale au format YYYY-MM-DD.
// - total_tokens : total exact a abreger.
//
// Retour :
// - date locale et total abrege.
// ----------------------------------------------------------------------------
std::wstring FormatTokenDayTooltip(
    const std::string& date_key,
    std::uint64_t total_tokens
);

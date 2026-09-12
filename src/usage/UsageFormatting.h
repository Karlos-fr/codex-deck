// ============================================================================
// Codex Glass - Formatage des textes d'usage
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers qui transforment les donnees d'usage Codex en
// libelles affichables par le widget.
// ============================================================================

#pragma once

#include "UsageSnapshot.h"

#include <chrono>
#include <optional>
#include <string>

// ----------------------------------------------------------------------------
// Convertit un pourcentage utilise en texte de pourcentage restant.
//
// Parametres :
// - percent : valeur de pourcentage utilisee a inverser et formater.
//
// Retour :
// - texte du pourcentage restant pret pour DirectWrite.
// ----------------------------------------------------------------------------
std::wstring FormatRemainingPercent(double percent);

// ----------------------------------------------------------------------------
// Formate un pourcentage utilise optionnel en texte de pourcentage restant.
//
// Parametres :
// - percent : valeur de pourcentage utilisee a inverser et formater.
// - available : indique si la valeur est disponible.
//
// Retour :
// - texte du pourcentage restant ou libelle d'indisponibilite.
// ----------------------------------------------------------------------------
std::wstring FormatOptionalRemainingPercent(double percent, bool available);

// ----------------------------------------------------------------------------
// Convertit un pourcentage utilise en valeur normalisee restante pour une barre.
//
// Parametres :
// - percent : valeur de pourcentage utilisee a inverser et convertir.
//
// Retour :
// - valeur comprise entre 0 et 1.
// ----------------------------------------------------------------------------
float NormalizeRemainingPercent(double percent);

// ----------------------------------------------------------------------------
// Formate l'heure locale du dernier releve d'usage.
//
// Parametres :
// - sampled_at : date du dernier releve.
//
// Retour :
// - texte court au format localise, par exemple "Maj 14:05:32".
// ----------------------------------------------------------------------------
std::wstring FormatLastUpdateTime(std::chrono::system_clock::time_point sampled_at);

// ----------------------------------------------------------------------------
// Formate un delai relatif ou absolu pour le texte de reset.
//
// Parametres :
// - reset_at : date de reset a comparer au moment courant.
//
// Retour :
// - texte de reset localise.
// ----------------------------------------------------------------------------
std::wstring FormatResetDelay(std::chrono::system_clock::time_point reset_at);

// ----------------------------------------------------------------------------
// Formate un delai de reset optionnel pour le texte secondaire.
//
// Parametres :
// - reset_at : date de reset a comparer au moment courant.
// - available : indique si la fenetre de quota est disponible.
//
// Retour :
// - texte de reset ou libelle d'indisponibilite.
// ----------------------------------------------------------------------------
std::wstring FormatOptionalResetDelay(std::chrono::system_clock::time_point reset_at, bool available);

// ----------------------------------------------------------------------------
// Formate une reinitialisation dans le style court des metadonnees de quota.
//
// Parametres :
// - reset_at : date de reinitialisation a afficher.
// - available : indique si la date est exploitable.
//
// Retour :
// - date courte localisee ou libelle d'indisponibilite.
// ----------------------------------------------------------------------------
std::wstring FormatQuotaResetMetadata(
    std::chrono::system_clock::time_point reset_at,
    bool available
);

// ----------------------------------------------------------------------------
// Retourne le libelle d'etat associe a la fraicheur du snapshot.
//
// Parametres :
// - snapshot : releve d'usage a qualifier.
//
// Retour :
// - texte court affiche en haut a droite.
// ----------------------------------------------------------------------------
std::wstring FormatFreshnessLabel(const UsageSnapshot& snapshot);

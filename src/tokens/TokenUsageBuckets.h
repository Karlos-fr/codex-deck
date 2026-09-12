// ============================================================================
// Codex Glass - Buckets temporels de consommation locale
// ----------------------------------------------------------------------------
// Ce fichier declare les conversions d'heures civiles locales utilisees par le
// scanner, le stockage et les tests, sans lire de sessions ni dessiner l'UI.
// ============================================================================

#pragma once

#include "TokenUsageTypes.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

// ----------------------------------------------------------------------------
// Retourne le debut UTC de l'heure civile locale contenant un instant.
//
// Parametres :
// - value : instant a classer.
//
// Retour :
// - borne horaire stable, y compris pendant les changements d'heure.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point TokenLocalHourStart(
    std::chrono::system_clock::time_point value
);

// ----------------------------------------------------------------------------
// Convertit une borne horaire en cle entiere SQLite et JSON.
//
// Parametres :
// - value : borne horaire a convertir.
//
// Retour :
// - secondes Unix correspondantes.
// ----------------------------------------------------------------------------
std::int64_t TokenHourKey(std::chrono::system_clock::time_point value);

// ----------------------------------------------------------------------------
// Convertit une cle entiere en borne horaire.
//
// Parametres :
// - value : secondes Unix a convertir.
//
// Retour :
// - borne horaire correspondante.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point TokenHourFromKey(std::int64_t value);

// ----------------------------------------------------------------------------
// Construit les derniers buckets horaires, heure courante incluse.
//
// Parametres :
// - now : instant determinant l'heure courante.
// - count : nombre de buckets demandes.
//
// Retour :
// - serie ordonnee contenant aussi les heures sans consommation.
// ----------------------------------------------------------------------------
std::vector<TokenHourlyUsage> BuildRecentTokenHours(
    std::chrono::system_clock::time_point now,
    std::size_t count
);

// ----------------------------------------------------------------------------
// Retourne le debut de la tranche de cinq minutes contenant un instant.
//
// Parametres :
// - value : instant a classer.
//
// Retour :
// - borne UTC stable de la tranche.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point TokenFiveMinuteStart(
    std::chrono::system_clock::time_point value
);

// ----------------------------------------------------------------------------
// Construit les dernieres tranches de cinq minutes, tranche courante incluse.
//
// Parametres :
// - now : instant determinant la tranche courante.
// - count : nombre de buckets demandes.
//
// Retour :
// - serie ordonnee contenant aussi les tranches sans consommation.
// ----------------------------------------------------------------------------
std::vector<TokenFiveMinuteUsage> BuildRecentTokenFiveMinutes(
    std::chrono::system_clock::time_point now,
    std::size_t count
);

// ----------------------------------------------------------------------------
// Construit les tranches de cinq minutes depuis minuit local jusqu'a maintenant.
//
// Parametres :
// - now : instant situe dans la journee locale a construire.
//
// Retour :
// - serie chronologique de la journee, tranche courante incluse.
// ----------------------------------------------------------------------------
std::vector<TokenFiveMinuteUsage> BuildTodayTokenFiveMinutes(
    std::chrono::system_clock::time_point now
);

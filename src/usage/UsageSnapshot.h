// ============================================================================
// Codex Glass - Modele de donnees d'usage
// ----------------------------------------------------------------------------
// Ce fichier definit les structures metier qui representent un releve d'usage
// Codex independamment de son origine, du rendu et du stockage.
// ============================================================================

#pragma once

#include "UsageProviderTypes.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Represente l'etat de fraicheur ou de validite des donnees d'usage.
// ----------------------------------------------------------------------------
enum class UsageFreshness {
    Fresh,
    Refreshing,
    Stale,
    Error,
};

// ----------------------------------------------------------------------------
// Regroupe les valeurs observees lors d'un releve d'usage Codex.
// ----------------------------------------------------------------------------
struct UsageSnapshot {
    // Identite et forfait du fournisseur courant.
    ProviderIdentity identity{};

    // Credits optionnels du compte courant.
    ProviderCredits credits{};

    // Limites supplementaires retournees par le fournisseur.
    std::vector<ProviderAdditionalRateLimit> additional_rate_limits{};

    // Controle de depense optionnel retourne par le fournisseur.
    std::optional<ProviderSpendControl> spend_control;

    // Date et heure auxquelles le releve a ete produit.
    std::chrono::system_clock::time_point sampled_at{};

    // Pourcentage utilise dans la fenetre de quota 5 h.
    double five_hour_used_percent = 0.0;

    // Indique si la fenetre de quota 5 h est presente dans la source.
    bool five_hour_available = true;

    // Pourcentage utilise dans la fenetre de quota hebdomadaire.
    double weekly_used_percent = 0.0;

    // Indique si la fenetre de quota hebdomadaire est presente dans la source.
    bool weekly_available = true;

    // Date et heure de reset de la fenetre de quota 5 h.
    std::chrono::system_clock::time_point five_hour_reset_at{};

    // Date et heure de reset de la fenetre de quota hebdomadaire.
    std::chrono::system_clock::time_point weekly_reset_at{};

    // Etat de fraicheur ou de validite du releve.
    UsageFreshness freshness = UsageFreshness::Fresh;

    // Message d'erreur optionnel lorsque le releve n'est pas exploitable.
    std::optional<std::wstring> error_message;
};

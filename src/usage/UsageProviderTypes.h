// ============================================================================
// Codex Glass - Modeles generiques des fournisseurs d'usage
// ----------------------------------------------------------------------------
// Ce fichier definit les donnees partagees entre providers, workers et UI. Il
// ne contient aucun detail d'authentification ou de transport propre a Codex.
// ============================================================================

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Identifie un fournisseur de donnees d'usage.
// ----------------------------------------------------------------------------
enum class UsageProviderId {
    Codex,
};

// ----------------------------------------------------------------------------
// Decrit l'identite publique et le forfait d'un compte provider.
// ----------------------------------------------------------------------------
struct ProviderIdentity {
    // Fournisseur ayant produit les donnees.
    UsageProviderId provider_id = UsageProviderId::Codex;

    // Type de forfait brut, conserve meme s'il est inconnu de l'application.
    std::optional<std::wstring> plan_type;

    // Adresse de compte optionnelle, non requise par l'interface actuelle.
    std::optional<std::wstring> account_email;
};

// ----------------------------------------------------------------------------
// Decrit les credits optionnels d'un compte provider.
// ----------------------------------------------------------------------------
struct ProviderCredits {
    // Indique si l'objet credits etait present dans la source.
    bool available = false;

    // Indique si le compte possede un mecanisme de credits.
    bool has_credits = false;

    // Indique si les credits sont annonces comme illimites.
    bool unlimited = false;

    // Solde de credits lorsqu'il est fourni.
    std::optional<double> balance;
};

// ----------------------------------------------------------------------------
// Represente une fenetre temporelle de limite provider.
// ----------------------------------------------------------------------------
struct ProviderRateLimitWindow {
    // Indique si la fenetre est exploitable.
    bool available = false;

    // Pourcentage deja utilise dans cette fenetre.
    double used_percent = 0.0;

    // Duree de la fenetre en secondes.
    int window_seconds = 0;

    // Date de reinitialisation lorsqu'elle est connue.
    std::optional<std::chrono::system_clock::time_point> reset_at;
};

// ----------------------------------------------------------------------------
// Regroupe les fenetres d'une limite supplementaire nommee.
// ----------------------------------------------------------------------------
struct ProviderAdditionalRateLimit {
    // Identifiant stable derive de la feature metree ou du nom.
    std::wstring id;

    // Libelle retourne par le provider, ou libelle de secours.
    std::wstring label;

    // Feature metree brute lorsqu'elle est fournie.
    std::optional<std::wstring> metered_feature;

    // Fenetre principale optionnelle.
    ProviderRateLimitWindow primary;

    // Fenetre secondaire optionnelle.
    ProviderRateLimitWindow secondary;
};

// ----------------------------------------------------------------------------
// Decrit un plafond individuel ou mensuel de depense.
// ----------------------------------------------------------------------------
struct ProviderSpendControl {
    // Montant maximal configure.
    std::optional<double> limit;

    // Montant deja utilise.
    std::optional<double> used;

    // Pourcentage restant retourne par la source.
    std::optional<double> remaining_percent;

    // Date de reinitialisation optionnelle.
    std::optional<std::chrono::system_clock::time_point> reset_at;
};

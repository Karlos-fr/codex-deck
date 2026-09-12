// ============================================================================
// Codex Glass - Modeles de consommation locale de tokens
// ----------------------------------------------------------------------------
// Ce fichier definit les agregats locaux issus des sessions LLM. Il reste
// independant du provider distant, du stockage et du rendu.
// ============================================================================

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Nombre maximal de tranches horaires chargees pour l'histogramme local.
constexpr std::size_t kTokenHourlyGraphBucketCount = 24;

// Nombre de tranches de cinq minutes affichees pour la plage d'une heure.
constexpr std::size_t kTokenFiveMinuteGraphBucketCount = 12;

// Nombre maximal de jours civils conserves pour la heatmap locale.
constexpr int kTokenHeatmapCoverageDays = 365;

// Nombre d'heures recentes conservees dans le cache incremental par fichier.
constexpr int kTokenHourlyCacheRetentionHours = 48;

// Nombre de minutes conservees pour couvrir une journee locale longue avec DST.
constexpr int kTokenFiveMinuteCacheRetentionMinutes = 1560;

// ----------------------------------------------------------------------------
// Decrit l'etat courant de la collecte locale des tokens.
// ----------------------------------------------------------------------------
enum class TokenUsageFreshness {
    Loading,
    Fresh,
    Empty,
    Error,
};

// ----------------------------------------------------------------------------
// Regroupe les categories de tokens observees.
// ----------------------------------------------------------------------------
struct TokenUsageCounts {
    // Tokens d'entree totaux, cache inclus selon le format Codex.
    std::uint64_t input_tokens = 0;

    // Sous-ensemble des tokens d'entree lu depuis le cache.
    std::uint64_t cached_input_tokens = 0;

    // Tokens de sortie totaux, raisonnement inclus selon le format Codex.
    std::uint64_t output_tokens = 0;

    // Sous-ensemble des tokens de sortie attribue au raisonnement.
    std::uint64_t reasoning_output_tokens = 0;

    // ------------------------------------------------------------------------
    // Retourne le total sans recompter les sous-ensembles cache et raisonnement.
    // ------------------------------------------------------------------------
    std::uint64_t Total() const {
        return input_tokens > std::numeric_limits<std::uint64_t>::max() - output_tokens
            ? std::numeric_limits<std::uint64_t>::max()
            : input_tokens + output_tokens;
    }
};

// ----------------------------------------------------------------------------
// Regroupe les tokens attribues a un modele pour une periode.
// ----------------------------------------------------------------------------
struct TokenModelUsage {
    // Nom de modele observe dans les sessions.
    std::wstring model;

    // Compteurs attribues a ce modele.
    TokenUsageCounts counts{};

    // Nombre de requetes ou tours comptabilises.
    std::uint64_t request_count = 0;
};

// ----------------------------------------------------------------------------
// Regroupe la consommation d'une session locale.
// ----------------------------------------------------------------------------
struct TokenSessionUsage {
    // Identifiant local de session, sans contenu de conversation.
    std::wstring session_id;

    // Derniere activite observee dans la session.
    std::chrono::system_clock::time_point last_activity{};

    // Compteurs cumules de la session.
    TokenUsageCounts counts{};

    // Nombre de requetes ou tours comptabilises.
    std::uint64_t request_count = 0;
};

// ----------------------------------------------------------------------------
// Regroupe la consommation d'un jour civil local.
// ----------------------------------------------------------------------------
struct TokenDailyUsage {
    // Cle locale stable au format YYYY-MM-DD.
    std::string date_key;

    // Compteurs du jour.
    TokenUsageCounts counts{};

    // Nombre de requetes ou tours du jour.
    std::uint64_t request_count = 0;

    // Repartition optionnelle par modele.
    std::vector<TokenModelUsage> models{};
};

// ----------------------------------------------------------------------------
// Regroupe la consommation d'une heure civile locale non ambigue.
// ----------------------------------------------------------------------------
struct TokenHourlyUsage {
    // Debut UTC de l'heure civile locale representee par ce bucket.
    std::chrono::system_clock::time_point bucket_start{};

    // Compteurs de l'heure.
    TokenUsageCounts counts{};

    // Nombre de requetes ou tours de l'heure.
    std::uint64_t request_count = 0;

    // Repartition optionnelle par modele.
    std::vector<TokenModelUsage> models{};
};

// ----------------------------------------------------------------------------
// Regroupe la consommation d'une tranche de cinq minutes.
// ----------------------------------------------------------------------------
struct TokenFiveMinuteUsage {
    // Debut UTC de la tranche representee par ce bucket.
    std::chrono::system_clock::time_point bucket_start{};

    // Compteurs de la tranche.
    TokenUsageCounts counts{};

    // Nombre de requetes ou tours de la tranche.
    std::uint64_t request_count = 0;

    // Repartition optionnelle par modele.
    std::vector<TokenModelUsage> models{};
};

// ----------------------------------------------------------------------------
// Snapshot complet de consommation locale de tokens.
// ----------------------------------------------------------------------------
struct TokenUsageSnapshot {
    // Etat du dernier scan local ou de celui actuellement en cours.
    TokenUsageFreshness freshness = TokenUsageFreshness::Loading;

    // Indique si un repertoire Codex local a pu etre inspecte.
    bool available = false;

    // Date de fin du scan.
    std::chrono::system_clock::time_point updated_at{};

    // Nombre de jours calendaires couverts.
    int coverage_days = 30;

    // Serie quotidienne ordonnee, jours a zero inclus.
    std::vector<TokenDailyUsage> daily{};

    // Serie horaire ordonnee, heures sans consommation incluses.
    std::vector<TokenHourlyUsage> hourly{};

    // Serie par tranches de cinq minutes, tranches sans consommation incluses.
    std::vector<TokenFiveMinuteUsage> five_minute{};

    // Sessions locales observees dans la couverture.
    std::vector<TokenSessionUsage> sessions{};

    // Erreur non sensible du dernier scan.
    std::optional<std::wstring> error_message;
};

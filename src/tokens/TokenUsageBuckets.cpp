// ============================================================================
// Codex Glass - Buckets temporels de consommation locale
// ----------------------------------------------------------------------------
// Ce fichier calcule des bornes horaires locales non ambigues en conservant un
// instant UTC comme identite, notamment pendant les transitions saisonnieres.
// ============================================================================

#include "TokenUsageBuckets.h"

#include <algorithm>
#include <ctime>
#include <utility>

// ----------------------------------------------------------------------------
// Retourne le debut UTC de l'heure civile locale contenant un instant.
//
// Parametres :
// - value : instant a classer dans son heure civile locale.
//
// Retour :
// - instant UTC stable correspondant au debut du bucket.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point TokenLocalHourStart(
    std::chrono::system_clock::time_point value
) {
    const auto precise_seconds = std::chrono::time_point_cast<std::chrono::seconds>(value);
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(precise_seconds);
    std::tm local_time{};
    localtime_s(&local_time, &raw_time);
    return precise_seconds
        - std::chrono::minutes{local_time.tm_min}
        - std::chrono::seconds{local_time.tm_sec};
}

// ----------------------------------------------------------------------------
// Convertit une borne horaire en cle entiere SQLite et JSON.
//
// Parametres :
// - value : borne horaire UTC a convertir.
//
// Retour :
// - nombre de secondes ecoulees depuis l'epoque Unix.
// ----------------------------------------------------------------------------
std::int64_t TokenHourKey(std::chrono::system_clock::time_point value) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        value.time_since_epoch()
    ).count();
}

// ----------------------------------------------------------------------------
// Convertit une cle entiere en borne horaire.
//
// Parametres :
// - value : secondes Unix de la borne horaire.
//
// Retour :
// - instant systeme correspondant.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point TokenHourFromKey(std::int64_t value) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{value}};
}

// ----------------------------------------------------------------------------
// Construit les derniers buckets horaires, heure courante incluse.
//
// Parametres :
// - now : instant determinant l'heure courante.
// - count : nombre de buckets demandes.
//
// Retour :
// - serie chronologique contenant aussi les heures sans consommation.
// ----------------------------------------------------------------------------
std::vector<TokenHourlyUsage> BuildRecentTokenHours(
    std::chrono::system_clock::time_point now,
    std::size_t count
) {
    std::vector<TokenHourlyUsage> result;
    result.reserve(count);
    const auto current_hour = TokenLocalHourStart(now);
    for (std::size_t reverse_index = count; reverse_index > 0; --reverse_index) {
        TokenHourlyUsage bucket{};
        bucket.bucket_start = current_hour - std::chrono::hours{reverse_index - 1};
        result.push_back(std::move(bucket));
    }
    return result;
}

// ----------------------------------------------------------------------------
// Retourne le debut UTC de la tranche de cinq minutes contenant un instant.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point TokenFiveMinuteStart(
    std::chrono::system_clock::time_point value
) {
    // Duree exacte d'une tranche de cinq minutes en secondes.
    constexpr std::int64_t kBucketSeconds = 5 * 60;
    const std::int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(
        value.time_since_epoch()
    ).count();
    return std::chrono::system_clock::time_point{
        std::chrono::seconds{seconds - (seconds % kBucketSeconds)}
    };
}

// ----------------------------------------------------------------------------
// Construit les dernieres tranches de cinq minutes, tranche courante incluse.
// ----------------------------------------------------------------------------
std::vector<TokenFiveMinuteUsage> BuildRecentTokenFiveMinutes(
    std::chrono::system_clock::time_point now,
    std::size_t count
) {
    std::vector<TokenFiveMinuteUsage> result;
    result.reserve(count);
    const auto current_bucket = TokenFiveMinuteStart(now);
    for (std::size_t reverse_index = count; reverse_index > 0; --reverse_index) {
        TokenFiveMinuteUsage bucket{};
        bucket.bucket_start = current_bucket - std::chrono::minutes{5 * (reverse_index - 1)};
        result.push_back(std::move(bucket));
    }
    return result;
}

// ----------------------------------------------------------------------------
// Construit les tranches de cinq minutes depuis minuit local jusqu'a maintenant.
// ----------------------------------------------------------------------------
std::vector<TokenFiveMinuteUsage> BuildTodayTokenFiveMinutes(
    std::chrono::system_clock::time_point now
) {
    const std::time_t raw_now = std::chrono::system_clock::to_time_t(now);
    std::tm local_midnight{};
    localtime_s(&local_midnight, &raw_now);
    local_midnight.tm_hour = 0;
    local_midnight.tm_min = 0;
    local_midnight.tm_sec = 0;
    local_midnight.tm_isdst = -1;
    const auto midnight = std::chrono::system_clock::from_time_t(
        std::mktime(&local_midnight)
    );
    const auto current_bucket = TokenFiveMinuteStart(now);
    const auto elapsed_minutes = std::max(
        std::chrono::minutes{0},
        std::chrono::duration_cast<std::chrono::minutes>(current_bucket - midnight)
    );
    const std::size_t bucket_count = static_cast<std::size_t>(elapsed_minutes.count() / 5) + 1U;
    return BuildRecentTokenFiveMinutes(now, bucket_count);
}

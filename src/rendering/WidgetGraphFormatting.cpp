// ============================================================================
// Codex Glass - Formatage partage des graphes
// ----------------------------------------------------------------------------
// Ce fichier produit les dates, heures, pourcentages et tokens localises des
// graphes sans dependre de Direct2D ni des couches de collecte.
// ============================================================================

#include "WidgetGraphFormatting.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../tokens/TokenUsageTypes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <cwchar>
#include <iomanip>
#include <sstream>

namespace {

// ----------------------------------------------------------------------------
// Retourne le mois abrege localise correspondant a un index std::tm.
//
// Parametres :
// - month : index compris entre zero et onze.
//
// Retour :
// - libelle localise, janvier servant de repli hors limites.
// ----------------------------------------------------------------------------
std::wstring LocalizedGraphMonth(int month) {
    // Identifiants des mois dans l'ordre attendu par std::tm.
    static constexpr std::array<unsigned int, 12> kMonthIds = {
        IDS_MONTH_JAN, IDS_MONTH_FEB, IDS_MONTH_MAR, IDS_MONTH_APR,
        IDS_MONTH_MAY, IDS_MONTH_JUN, IDS_MONTH_JUL, IDS_MONTH_AUG,
        IDS_MONTH_SEP, IDS_MONTH_OCT, IDS_MONTH_NOV, IDS_MONTH_DEC,
    };
    const int safe_month = std::clamp(month, 0, static_cast<int>(kMonthIds.size()) - 1);
    return T(kMonthIds[static_cast<std::size_t>(safe_month)]);
}

// ----------------------------------------------------------------------------
// Formate uniquement l'heure locale d'un instant.
//
// Parametres :
// - value : instant systeme a convertir.
//
// Retour :
// - heure au format HH:mm.
// ----------------------------------------------------------------------------
std::wstring FormatGraphClock(std::chrono::system_clock::time_point value) {
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(value);
    std::tm local_time{};
    localtime_s(&local_time, &raw_time);
    wchar_t text[6]{};
    swprintf_s(text, L"%02d:%02d", local_time.tm_hour, local_time.tm_min);
    return text;
}

// ----------------------------------------------------------------------------
// Formate l'heure locale precise d'un instant pour une plage tres courte.
//
// Parametres :
// - value : instant systeme a convertir.
//
// Retour :
// - heure au format HH:mm:ss.
// ----------------------------------------------------------------------------
std::wstring FormatGraphClockWithSeconds(std::chrono::system_clock::time_point value) {
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(value);
    std::tm local_time{};
    localtime_s(&local_time, &raw_time);
    wchar_t text[9]{};
    swprintf_s(text, L"%02d:%02d:%02d", local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    return text;
}

} // namespace

// ----------------------------------------------------------------------------
// Convertit une plage de graphe en duree.
//
// Parametres :
// - range : plage a convertir.
//
// Retour :
// - duree correspondante.
// ----------------------------------------------------------------------------
std::chrono::seconds GraphRangeDuration(GraphRange range) {
    switch (range) {
    case GraphRange::Minutes5:
        return std::chrono::minutes{5};
    case GraphRange::Hours1:
        return std::chrono::hours{1};
    case GraphRange::Hours5:
        return std::chrono::hours{5};
    case GraphRange::Days7:
        return std::chrono::hours{24 * 7};
    case GraphRange::Days30:
        return std::chrono::hours{24 * 30};
    case GraphRange::Hours24:
    default:
        return std::chrono::hours{24};
    }
}

// ----------------------------------------------------------------------------
// Retourne le libelle court localise d'une plage de graphe.
//
// Parametres :
// - range : plage a presenter dans le selecteur.
//
// Retour :
// - libelle compact localise.
// ----------------------------------------------------------------------------
std::wstring FormatGraphRangeLabel(GraphRange range) {
    switch (range) {
    case GraphRange::Minutes5:
        return T(IDS_GRAPH_RANGE_5MIN);
    case GraphRange::Hours1:
        return T(IDS_GRAPH_RANGE_1H);
    case GraphRange::Hours5:
        return T(IDS_GRAPH_RANGE_5H);
    case GraphRange::Hours24:
        return T(IDS_GRAPH_RANGE_24H);
    case GraphRange::Days7:
        return T(IDS_GRAPH_RANGE_7D);
    case GraphRange::Days30:
        return T(IDS_GRAPH_RANGE_30D);
    default:
        return T(IDS_GRAPH_RANGE_30D);
    }
}

// ----------------------------------------------------------------------------
// Retourne le nombre de buckets horaires demande par une plage Tokens.
//
// Parametres :
// - range : plage horaire selectionnee.
//
// Retour :
// - un, cinq ou vingt-quatre buckets.
// ----------------------------------------------------------------------------
std::size_t TokenGraphBucketCount(GraphRange range) {
    switch (range) {
    case GraphRange::Hours1:
        return kTokenFiveMinuteGraphBucketCount;
    case GraphRange::Hours24:
        return 24;
    case GraphRange::Hours5:
    default:
        return 5;
    }
}

// ----------------------------------------------------------------------------
// Formate une date locale courte a partir d'un instant systeme.
//
// Parametres :
// - value : instant a convertir.
//
// Retour :
// - date courte localisee.
// ----------------------------------------------------------------------------
std::wstring FormatGraphDate(std::chrono::system_clock::time_point value) {
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(value);
    std::tm local_time{};
    localtime_s(&local_time, &raw_time);
    return std::to_wstring(local_time.tm_mday) + L" " + LocalizedGraphMonth(local_time.tm_mon);
}

// ----------------------------------------------------------------------------
// Formate une cle civile YYYY-MM-DD en date locale courte.
//
// Parametres :
// - key : cle civile normalisee.
//
// Retour :
// - date courte localisee ou cle d'origine si elle est invalide.
// ----------------------------------------------------------------------------
std::wstring FormatGraphDateKey(const std::string& key) {
    if (key.size() != 10 || key[4] != '-' || key[7] != '-') {
        return std::wstring(key.begin(), key.end());
    }
    try {
        const int month = std::stoi(key.substr(5, 2));
        const int day = std::stoi(key.substr(8, 2));
        return std::to_wstring(day) + L" " + LocalizedGraphMonth(month - 1);
    } catch (...) {
        return std::wstring(key.begin(), key.end());
    }
}

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
) {
    return range == GraphRange::Minutes5
        || range == GraphRange::Hours1
        || range == GraphRange::Hours5
        || range == GraphRange::Hours24
        ? FormatGraphClock(value)
        : FormatGraphDate(value);
}

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
) {
    if (range == GraphRange::Minutes5) {
        return FormatGraphClockWithSeconds(value);
    }
    if (range == GraphRange::Hours1 || range == GraphRange::Hours5) {
        return FormatGraphClock(value);
    }
    if (range == GraphRange::Hours24) {
        return FormatGraphDate(value) + L" \267 " + FormatGraphClock(value);
    }
    return FormatGraphDate(value);
}

// ----------------------------------------------------------------------------
// Formate un pourcentage entier pour un tooltip de quota.
//
// Parametres :
// - value : pourcentage a arrondir.
//
// Retour :
// - pourcentage entier suivi de son unite.
// ----------------------------------------------------------------------------
std::wstring FormatGraphPercent(double value) {
    return std::to_wstring(static_cast<int>(std::lround(value))) + L" %";
}

// ----------------------------------------------------------------------------
// Formate un total de tokens en millions avec une decimale localisee.
//
// Parametres :
// - value : total exact de tokens.
//
// Retour :
// - valeur abregee en millions.
// ----------------------------------------------------------------------------
std::wstring FormatGraphTokenMillions(std::uint64_t value) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(1)
        << (static_cast<double>(value) / 1'000'000.0);
    std::wstring text = stream.str();
    if (ActiveUiLanguage() == UiLanguage::French) {
        std::replace(text.begin(), text.end(), L'.', L',');
    }
    return text + L" M";
}

// ----------------------------------------------------------------------------
// Formate une graduation entiere de tokens en millions.
//
// Parametres :
// - value : valeur exacte de la graduation.
//
// Retour :
// - libelle entier abrege en millions.
// ----------------------------------------------------------------------------
std::wstring FormatGraphTokenAxisValue(std::uint64_t value) {
    if (value == 0) {
        return L"0";
    }
    if (value >= 1'000'000ULL) {
        return std::to_wstring(value / 1'000'000ULL) + L" M";
    }
    if (value >= 1'000ULL) {
        return std::to_wstring(value / 1'000ULL) + L" k";
    }
    return std::to_wstring(value);
}

// ----------------------------------------------------------------------------
// Formate l'abscisse d'un bucket Tokens selon la granularite choisie.
// ----------------------------------------------------------------------------
std::wstring FormatTokenBucketAxis(
    std::chrono::system_clock::time_point bucket_start,
    GraphRange range
) {
    return range == GraphRange::Hours1
        ? FormatGraphClock(bucket_start)
        : FormatTokenHourAxis(bucket_start);
}

// ----------------------------------------------------------------------------
// Formate le tooltip d'un bucket Tokens selon sa duree reelle.
// ----------------------------------------------------------------------------
std::wstring FormatTokenBucketTooltip(
    std::chrono::system_clock::time_point bucket_start,
    GraphRange range,
    std::uint64_t total_tokens
) {
    const auto duration = range == GraphRange::Hours1
        ? std::chrono::minutes{5}
        : std::chrono::minutes{60};
    return FormatGraphDate(bucket_start) + L" \267 "
        + FormatGraphClock(bucket_start) + L"-"
        + FormatGraphClock(bucket_start + duration) + L" \267 "
        + FormatGraphTokenMillions(total_tokens) + L" " + T(IDS_GRAPH_TOKEN_SHORT_SUFFIX);
}

// ----------------------------------------------------------------------------
// Formate l'heure locale courte d'un bucket horaire.
// ----------------------------------------------------------------------------
std::wstring FormatTokenHourAxis(
    std::chrono::system_clock::time_point bucket_start
) {
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(bucket_start);
    std::tm local_time{};
    localtime_s(&local_time, &raw_time);
    return ActiveUiLanguage() == UiLanguage::French
        ? std::to_wstring(local_time.tm_hour) + L" h"
        : FormatGraphClock(bucket_start);
}

// ----------------------------------------------------------------------------
// Formate le tooltip complet d'une cellule quotidienne.
// ----------------------------------------------------------------------------
std::wstring FormatTokenDayTooltip(
    const std::string& date_key,
    std::uint64_t total_tokens
) {
    return FormatGraphDateKey(date_key) + L" \267 "
        + FormatGraphTokenMillions(total_tokens) + L" " + T(IDS_GRAPH_TOKEN_SHORT_SUFFIX);
}

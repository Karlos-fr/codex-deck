// ============================================================================
// Codex Glass - Implementation du formatage des textes d'usage
// ----------------------------------------------------------------------------
// Ce fichier regroupe les conversions de pourcentages, dates, resets et rythmes
// de consommation vers les libelles localises du widget.
// ============================================================================

#include "UsageFormatting.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <array>
#include <ctime>

namespace {

// ----------------------------------------------------------------------------
// Retourne le nom localise d'un jour de semaine.
//
// Parametres :
// - week_day : index tm_wday compris entre 0 et 6.
//
// Retour :
// - nom du jour localise.
// ----------------------------------------------------------------------------
std::wstring LocalizedWeekDayName(int week_day) {
    // Identifiants de ressources des jours, dans l'ordre attendu par tm_wday.
    static constexpr std::array<unsigned int, 7> kWeekDayResourceIds = {
        IDS_DAY_SUNDAY,
        IDS_DAY_MONDAY,
        IDS_DAY_TUESDAY,
        IDS_DAY_WEDNESDAY,
        IDS_DAY_THURSDAY,
        IDS_DAY_FRIDAY,
        IDS_DAY_SATURDAY,
    };

    if (week_day < 0 || week_day >= static_cast<int>(kWeekDayResourceIds.size())) {
        week_day = 0;
    }

    return T(kWeekDayResourceIds[week_day]);
}

// ----------------------------------------------------------------------------
// Retourne le nom localise abrege d'un jour de semaine.
//
// Parametres :
// - week_day : index tm_wday compris entre 0 et 6.
//
// Retour :
// - nom court du jour utilise dans la ligne de metadonnees.
// ----------------------------------------------------------------------------
std::wstring LocalizedShortWeekDayName(int week_day) {
    // Identifiants des jours courts, dans l'ordre attendu par tm_wday.
    static constexpr std::array<unsigned int, 7> kShortWeekDayResourceIds = {
        IDS_DAY_SUNDAY_SHORT,
        IDS_DAY_MONDAY_SHORT,
        IDS_DAY_TUESDAY_SHORT,
        IDS_DAY_WEDNESDAY_SHORT,
        IDS_DAY_THURSDAY_SHORT,
        IDS_DAY_FRIDAY_SHORT,
        IDS_DAY_SATURDAY_SHORT,
    };
    if (week_day < 0 || week_day >= static_cast<int>(kShortWeekDayResourceIds.size())) {
        week_day = 0;
    }
    return T(kShortWeekDayResourceIds[week_day]);
}

// ----------------------------------------------------------------------------
// Retourne le nom localise abrege d'un mois.
//
// Parametres :
// - month : index tm_mon compris entre 0 et 11.
//
// Retour :
// - nom abrege du mois localise.
// ----------------------------------------------------------------------------
std::wstring LocalizedMonthName(int month) {
    // Identifiants de ressources des mois, dans l'ordre attendu par tm_mon.
    static constexpr std::array<unsigned int, 12> kMonthResourceIds = {
        IDS_MONTH_JAN,
        IDS_MONTH_FEB,
        IDS_MONTH_MAR,
        IDS_MONTH_APR,
        IDS_MONTH_MAY,
        IDS_MONTH_JUN,
        IDS_MONTH_JUL,
        IDS_MONTH_AUG,
        IDS_MONTH_SEP,
        IDS_MONTH_OCT,
        IDS_MONTH_NOV,
        IDS_MONTH_DEC,
    };

    if (month < 0 || month >= static_cast<int>(kMonthResourceIds.size())) {
        month = 0;
    }

    return T(kMonthResourceIds[month]);
}

} // namespace

// ----------------------------------------------------------------------------
// Convertit un pourcentage utilise en texte de pourcentage restant.
//
// Parametres :
// - percent : valeur de pourcentage utilisee a inverser et formater.
//
// Retour :
// - texte du pourcentage restant pret pour DirectWrite.
// ----------------------------------------------------------------------------
std::wstring FormatRemainingPercent(double percent) {
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%.0f %%", 100.0 - std::clamp(percent, 0.0, 100.0));
    return buffer;
}

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
std::wstring FormatOptionalRemainingPercent(double percent, bool available) {
    if (!available) {
        return T(IDS_NOT_AVAILABLE);
    }

    return FormatRemainingPercent(percent);
}

// ----------------------------------------------------------------------------
// Convertit un pourcentage utilise en valeur normalisee restante pour une barre.
//
// Parametres :
// - percent : valeur de pourcentage utilisee a inverser et convertir.
//
// Retour :
// - valeur comprise entre 0 et 1.
// ----------------------------------------------------------------------------
float NormalizeRemainingPercent(double percent) {
    return static_cast<float>((100.0 - std::clamp(percent, 0.0, 100.0)) / 100.0);
}

// ----------------------------------------------------------------------------
// Formate l'heure locale du dernier releve d'usage.
//
// Parametres :
// - sampled_at : date du dernier releve.
//
// Retour :
// - texte court au format localise, par exemple "Maj 14:05:32".
// ----------------------------------------------------------------------------
std::wstring FormatLastUpdateTime(std::chrono::system_clock::time_point sampled_at) {
    if (sampled_at.time_since_epoch().count() == 0) {
        return T(IDS_STATUS_UPDATE_UNAVAILABLE);
    }

    const std::time_t sampled_time = std::chrono::system_clock::to_time_t(sampled_at);
    std::tm local_time{};
    localtime_s(&local_time, &sampled_time);

    wchar_t buffer[96]{};
    swprintf_s(
        buffer,
        T(IDS_STATUS_FRESH).c_str(),
        local_time.tm_hour,
        local_time.tm_min,
        local_time.tm_sec
    );
    return buffer;
}

// ----------------------------------------------------------------------------
// Formate un delai relatif ou absolu pour le texte de reset.
//
// Parametres :
// - reset_at : date de reset a comparer au moment courant.
//
// Retour :
// - texte de reset localise.
// ----------------------------------------------------------------------------
std::wstring FormatResetDelay(std::chrono::system_clock::time_point reset_at) {
    const auto now = std::chrono::system_clock::now();
    if (reset_at <= now) {
        return T(IDS_RESET_IMMINENT);
    }

    const auto remaining = std::chrono::duration_cast<std::chrono::minutes>(reset_at - now);
    wchar_t buffer[96]{};
    if (remaining < std::chrono::hours{24}) {
        const int hours = static_cast<int>(std::chrono::duration_cast<std::chrono::hours>(remaining).count());
        const int minutes = static_cast<int>(remaining.count() % 60);
        swprintf_s(buffer, T(IDS_RESET_IN_DURATION).c_str(), hours, minutes);
        return buffer;
    }

    const std::time_t reset_time = std::chrono::system_clock::to_time_t(reset_at);
    std::tm local_time{};
    localtime_s(&local_time, &reset_time);

    const std::wstring week_day = LocalizedWeekDayName(local_time.tm_wday);
    const std::wstring month = LocalizedMonthName(local_time.tm_mon);
    swprintf_s(
        buffer,
        T(IDS_RESET_AT_DATE).c_str(),
        week_day.c_str(),
        local_time.tm_mday,
        month.c_str(),
        local_time.tm_year + 1900,
        local_time.tm_hour,
        local_time.tm_min
    );
    return buffer;
}

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
std::wstring FormatOptionalResetDelay(std::chrono::system_clock::time_point reset_at, bool available) {
    if (!available) {
        return T(IDS_NOT_PROVIDED_BY_CODEX);
    }

    return FormatResetDelay(reset_at);
}

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
) {
    if (!available || reset_at.time_since_epoch().count() == 0) {
        return T(IDS_RESET_METADATA_UNAVAILABLE);
    }

    const std::time_t reset_time = std::chrono::system_clock::to_time_t(reset_at);
    std::tm local_time{};
    localtime_s(&local_time, &reset_time);
    const std::wstring week_day = LocalizedShortWeekDayName(local_time.tm_wday);
    const std::wstring month = LocalizedMonthName(local_time.tm_mon);
    wchar_t buffer[128]{};
    swprintf_s(
        buffer,
        T(IDS_RESET_METADATA).c_str(),
        week_day.c_str(),
        local_time.tm_mday,
        month.c_str(),
        local_time.tm_hour,
        local_time.tm_min
    );
    return buffer;
}

// ----------------------------------------------------------------------------
// Retourne le libelle d'etat associe a la fraicheur du snapshot.
//
// Parametres :
// - snapshot : releve d'usage a qualifier.
//
// Retour :
// - texte court affiche en haut a droite.
// ----------------------------------------------------------------------------
std::wstring FormatFreshnessLabel(const UsageSnapshot& snapshot) {
    switch (snapshot.freshness) {
    case UsageFreshness::Fresh:
    case UsageFreshness::Stale:
        return FormatLastUpdateTime(snapshot.sampled_at);

    case UsageFreshness::Refreshing:
        return T(IDS_STATUS_REFRESHING);

    case UsageFreshness::Error:
        return T(IDS_STATUS_ERROR);

    default:
        return T(IDS_STATUS_UNKNOWN);
    }
}

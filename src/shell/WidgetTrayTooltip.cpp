// ============================================================================
// Codex Glass - Formatage de l'infobulle de l'icone tray
// ----------------------------------------------------------------------------
// Ce fichier transforme les quotas visibles en lignes courtes compatibles avec
// la limite Win32. La gestion de Shell_NotifyIcon reste dans WidgetTrayIcon.
// ============================================================================

#include "WidgetTrayTooltip.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../usage/UsageFormatting.h"

#include <algorithm>
#include <cwctype>
#include <string_view>
#include <utility>

namespace {

// Nombre maximal de caracteres utiles du champ NOTIFYICONDATA::szTip.
constexpr std::size_t kMaximumTrayTooltipLength = 127;

// Duree maximale d'une fenetre consideree comme un quota court.
constexpr int kShortWindowMaximumSeconds = 12 * 60 * 60;

// Separateur compact entre deux quotas d'une meme famille.
constexpr std::wstring_view kQuotaSeparator = L" \u00b7 ";

// Ellipse utilisee lorsque le fournisseur expose trop de quotas pour Windows.
constexpr wchar_t kTooltipEllipsis = L'\u2026';

// ----------------------------------------------------------------------------
// Indique si une chaine mentionne Spark sans tenir compte de la casse.
//
// Parametre :
// - value : identifiant ou libelle a examiner.
//
// Retour : true si la chaine contient le mot Spark.
// ----------------------------------------------------------------------------
bool ContainsSpark(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value.find(L"spark") != std::wstring::npos;
}

// ----------------------------------------------------------------------------
// Indique si une limite supplementaire appartient a la famille Spark.
//
// Parametre :
// - limit : limite provider a identifier.
//
// Retour : true pour une limite Spark.
// ----------------------------------------------------------------------------
bool IsSparkLimit(const ProviderAdditionalRateLimit& limit) {
    return ContainsSpark(limit.id)
        || ContainsSpark(limit.label)
        || (limit.metered_feature.has_value() && ContainsSpark(*limit.metered_feature));
}

// ----------------------------------------------------------------------------
// Indique si une fenetre provider correspond a une duree courte.
//
// Parametre :
// - window : fenetre temporelle a classifier.
//
// Retour : true pour une duree positive inferieure ou egale a douze heures.
// ----------------------------------------------------------------------------
bool IsShortWindow(const ProviderRateLimitWindow& window) {
    return window.window_seconds > 0 && window.window_seconds <= kShortWindowMaximumSeconds;
}

// ----------------------------------------------------------------------------
// Ajoute une valeur a une ligne compacte de quotas.
//
// Parametres :
// - line : ligne completee sur place.
// - label : libelle court du quota.
// - value : pourcentage restant ou valeur indisponible.
//
// Effet de bord : insere un separateur lorsque la ligne contient deja un quota.
// ----------------------------------------------------------------------------
void AppendQuota(std::wstring& line, const std::wstring& label, const std::wstring& value) {
    if (!line.empty()) {
        line.append(kQuotaSeparator);
    }
    line.append(label).append(L" ").append(value);
}

// ----------------------------------------------------------------------------
// Ajoute une ligne non vide a l'infobulle.
//
// Parametres :
// - tooltip : infobulle completee sur place.
// - line : ligne a ajouter.
// ----------------------------------------------------------------------------
void AppendLine(std::wstring& tooltip, const std::wstring& line) {
    if (!line.empty()) {
        tooltip.append(L"\n").append(line);
    }
}

// ----------------------------------------------------------------------------
// Reduit une infobulle a la capacite du champ Win32.
//
// Parametre :
// - tooltip : texte potentiellement trop long.
//
// Retour : texte intact ou termine par une ellipse sans depasser la limite.
// ----------------------------------------------------------------------------
std::wstring FitTrayTooltip(std::wstring tooltip) {
    if (tooltip.size() <= kMaximumTrayTooltipLength) {
        return tooltip;
    }
    tooltip.resize(kMaximumTrayTooltipLength);
    tooltip.back() = kTooltipEllipsis;
    return tooltip;
}

} // namespace

// ----------------------------------------------------------------------------
// Construit l'infobulle compacte de l'icone tray.
//
// Parametres :
// - snapshot : dernier releve de quotas disponible.
// - visibility : choix des lignes affichees dans le widget.
//
// Retour : titre de l'application suivi des pourcentages restants visibles.
// ----------------------------------------------------------------------------
std::wstring BuildWidgetTrayTooltip(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
) {
    std::wstring tooltip = T(IDS_APP_TITLE);
    std::wstring main_line;
    if (visibility.five_hour) {
        AppendQuota(
            main_line,
            T(IDS_MENU_QUOTA_5H),
            FormatOptionalRemainingPercent(snapshot.five_hour_used_percent, snapshot.five_hour_available)
        );
    }
    if (visibility.weekly) {
        AppendQuota(
            main_line,
            T(IDS_MENU_QUOTA_WEEKLY),
            FormatOptionalRemainingPercent(snapshot.weekly_used_percent, snapshot.weekly_available)
        );
    }
    AppendLine(tooltip, main_line);

    std::wstring additional_line;
    for (const ProviderAdditionalRateLimit& limit : snapshot.additional_rate_limits) {
        const bool spark = IsSparkLimit(limit);
        const auto append_window = [&](const ProviderRateLimitWindow& window) {
            if (!window.available) {
                return;
            }
            const bool short_window = IsShortWindow(window);
            if (spark && !(short_window ? visibility.spark_five_hour : visibility.spark_weekly)) {
                return;
            }
            const std::wstring label = spark
                ? T(short_window ? IDS_MENU_QUOTA_SPARK_5H : IDS_MENU_QUOTA_SPARK_WEEKLY)
                : limit.label + L" " + T(short_window ? IDS_PROVIDER_WINDOW_SHORT : IDS_PROVIDER_WINDOW_LONG);
            AppendQuota(
                additional_line,
                label,
                FormatOptionalRemainingPercent(window.used_percent, window.available)
            );
        };
        append_window(limit.primary);
        append_window(limit.secondary);
    }
    AppendLine(tooltip, additional_line);
    return FitTrayTooltip(std::move(tooltip));
}

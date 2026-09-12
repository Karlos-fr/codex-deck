// ============================================================================
// Codex Glass - Rendu des informations provider
// ----------------------------------------------------------------------------
// Ce fichier transforme les donnees generiques du provider en libelles courts
// et reutilise les lignes d'usage existantes pour les limites supplementaires.
// ============================================================================

#include "WidgetRenderProviderInfo.h"

#include "WidgetRenderConstants.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../usage/UsageFormatting.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <cwchar>
#include <iomanip>
#include <sstream>
#include <wrl/client.h>

namespace {

// Duree maximale utilisee pour nommer une fenetre comme courte.
constexpr int kShortWindowMaxSeconds = 12 * 60 * 60;

// Marge de comparaison des soldes de credits avec une valeur entiere.
constexpr double kCreditComparisonEpsilon = 0.0001;

// ----------------------------------------------------------------------------
// Met en forme un type de forfait brut pour l'affichage.
// ----------------------------------------------------------------------------
std::wstring DisplayPlanType(std::wstring value) {
    std::replace(value.begin(), value.end(), L'_', L' ');
    bool capitalize = true;
    for (wchar_t& character : value) {
        if (character == L' ') {
            capitalize = true;
        } else if (capitalize && character >= L'a' && character <= L'z') {
            character = static_cast<wchar_t>(character - L'a' + L'A');
            capitalize = false;
        } else {
            capitalize = false;
        }
    }
    return value;
}

// ----------------------------------------------------------------------------
// Formate un solde avec au plus deux decimales.
// ----------------------------------------------------------------------------
std::wstring FormatCreditBalance(double value) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    std::wstring text = stream.str();
    while (!text.empty() && text.back() == L'0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == L'.') {
        text.pop_back();
    }
    return text;
}

// ----------------------------------------------------------------------------
// Indique si le nom d'un credit doit etre affiche au singulier.
//
// Parametres :
// - value : solde de credits a qualifier.
//
// Retour :
// - true si les regles de la langue active demandent le singulier.
// ----------------------------------------------------------------------------
bool UsesSingularCreditLabel(double value) {
    if (ActiveUiLanguage() == UiLanguage::French) {
        return value < 2.0;
    }
    return std::abs(value - 1.0) <= kCreditComparisonEpsilon;
}

// ----------------------------------------------------------------------------
// Mesure la largeur d'un texte avec le format du resume provider.
//
// Parametres :
// - context : ressources DirectWrite disponibles.
// - text : texte a mesurer.
//
// Retour :
// - largeur en DIPs, ou zero si la mesure echoue.
// ----------------------------------------------------------------------------
float MeasureProviderTextWidth(
    const WidgetRenderUsageContext& context,
    const std::wstring& text
) {
    if (context.dwrite_factory == nullptr || context.provider_summary_text_format == nullptr) {
        return 0.0F;
    }
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    const HRESULT result = context.dwrite_factory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        context.provider_summary_text_format,
        1000.0F,
        100.0F,
        layout.GetAddressOf()
    );
    if (FAILED(result)) {
        return 0.0F;
    }
    DWRITE_TEXT_METRICS metrics{};
    return SUCCEEDED(layout->GetMetrics(&metrics)) ? metrics.widthIncludingTrailingWhitespace : 0.0F;
}

// ----------------------------------------------------------------------------
// Reduit un resume provider avec une ellipse pour respecter sa largeur.
//
// Parametres :
// - context : ressources DirectWrite disponibles.
// - text : resume complet a ajuster.
// - maximum_width : largeur disponible en DIPs.
//
// Retour :
// - texte complet, abrege ou vide si aucun caractere ne peut etre affiche.
// ----------------------------------------------------------------------------
std::wstring FitProviderSummary(
    const WidgetRenderUsageContext& context,
    const std::wstring& text,
    float maximum_width
) {
    if (text.empty() || maximum_width <= 0.0F) {
        return {};
    }
    const float full_width = MeasureProviderTextWidth(context, text);
    if (full_width <= 0.0F || full_width <= maximum_width) {
        return text;
    }

    const std::wstring ellipsis = L"\u2026";
    if (MeasureProviderTextWidth(context, ellipsis) > maximum_width) {
        return {};
    }
    std::size_t low = 0;
    std::size_t high = text.size();
    while (low < high) {
        const std::size_t middle = low + ((high - low + 1) / 2);
        if (MeasureProviderTextWidth(context, text.substr(0, middle) + ellipsis) <= maximum_width) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return text.substr(0, low) + ellipsis;
}

// ----------------------------------------------------------------------------
// Indique si une fenetre doit utiliser le libelle court 5 h.
// ----------------------------------------------------------------------------
bool IsShortWindow(const ProviderRateLimitWindow& window) {
    return window.window_seconds > 0 && window.window_seconds <= kShortWindowMaxSeconds;
}

// ----------------------------------------------------------------------------
// Indique si une chaine mentionne Spark sans tenir compte de la casse.
//
// Parametres :
// - value : identifiant, feature ou libelle a examiner.
//
// Retour :
// - true lorsque le mot Spark est present.
// ----------------------------------------------------------------------------
bool ContainsSpark(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return value.find(L"spark") != std::wstring::npos;
}

// ----------------------------------------------------------------------------
// Indique si une limite additionnelle correspond a la famille Spark.
//
// Parametres :
// - limit : limite provider a identifier.
//
// Retour :
// - true pour un identifiant, une feature ou un libelle Spark.
// ----------------------------------------------------------------------------
bool IsSparkLimit(const ProviderAdditionalRateLimit& limit) {
    return ContainsSpark(limit.id)
        || ContainsSpark(limit.label)
        || (limit.metered_feature.has_value() && ContainsSpark(*limit.metered_feature));
}

// ----------------------------------------------------------------------------
// Indique si une fenetre additionnelle doit etre affichee.
//
// Parametres :
// - limit : limite provider proprietaire de la fenetre.
// - window : fenetre courte ou longue a filtrer.
// - visibility : choix utilisateur propres aux lignes Spark.
//
// Retour :
// - true pour une fenetre disponible et autorisee.
// ----------------------------------------------------------------------------
bool IsAdditionalWindowVisible(
    const ProviderAdditionalRateLimit& limit,
    const ProviderRateLimitWindow& window,
    const WidgetQuotaVisibilitySettings& visibility
) {
    if (!window.available) {
        return false;
    }
    if (!IsSparkLimit(limit)) {
        return true;
    }
    return IsShortWindow(window) ? visibility.spark_five_hour : visibility.spark_weekly;
}

// ----------------------------------------------------------------------------
// Construit le libelle complet d'une fenetre supplementaire.
// ----------------------------------------------------------------------------
std::wstring AdditionalWindowLabel(
    const ProviderAdditionalRateLimit& limit,
    const ProviderRateLimitWindow& window
) {
    return limit.label + L" - " + T(IsShortWindow(window) ? IDS_PROVIDER_WINDOW_SHORT : IDS_PROVIDER_WINDOW_LONG);
}

// ----------------------------------------------------------------------------
// Dessine un texte simple avec le contexte d'usage.
// ----------------------------------------------------------------------------
void DrawText(
    const WidgetRenderUsageContext& context,
    const std::wstring& text,
    const D2D1_RECT_F& bounds
) {
    context.render_target->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        context.provider_summary_text_format,
        bounds,
        context.muted_text_brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
}

// ----------------------------------------------------------------------------
// Dessine une fenetre supplementaire et avance la position verticale.
// ----------------------------------------------------------------------------
float DrawAdditionalWindow(
    const WidgetRenderUsageContext& context,
    const ProviderAdditionalRateLimit& limit,
    const ProviderRateLimitWindow& window,
    float top,
    float left,
    float right,
    ID2D1Brush* brush
) {
    const std::wstring label = AdditionalWindowLabel(limit, window);
    const std::wstring percent = FormatOptionalRemainingPercent(window.used_percent, window.available);
    const std::wstring reset = FormatQuotaResetMetadata(
        window.reset_at.value_or(std::chrono::system_clock::time_point{}),
        window.available && window.reset_at.has_value()
    );
    DrawUsageRow(
        context,
        label.c_str(),
        percent.c_str(),
        WidgetRollingNumberFrame{percent, percent, 1.0, false},
        window.available,
        reset.c_str(),
        NormalizeRemainingPercent(window.used_percent),
        top,
        left,
        right,
        brush
    );
    return top + kRowSpacing;
}

} // namespace

// ----------------------------------------------------------------------------
// Compte les fenetres supplementaires visibles d'un snapshot.
// ----------------------------------------------------------------------------
std::size_t CountVisibleAdditionalRateLimitRows(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
) {
    std::size_t count = 0;
    for (const auto& limit : snapshot.additional_rate_limits) {
        count += IsAdditionalWindowVisible(limit, limit.primary, visibility) ? 1U : 0U;
        count += IsAdditionalWindowVisible(limit, limit.secondary, visibility) ? 1U : 0U;
    }
    return count;
}

// ----------------------------------------------------------------------------
// Compte toutes les lignes de quota visibles pour un mode d'affichage.
// ----------------------------------------------------------------------------
std::size_t CountVisibleQuotaRows(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility,
    bool include_additional
) {
    std::size_t count = visibility.five_hour ? 1U : 0U;
    count += visibility.weekly ? 1U : 0U;
    if (include_additional) {
        count += CountVisibleAdditionalRateLimitRows(snapshot, visibility);
    }
    return count;
}

// ----------------------------------------------------------------------------
// Construit les limites additionnelles visibles pour un rendu condense.
// ----------------------------------------------------------------------------
std::vector<WidgetCompactRateLimitSummary> BuildCompactAdditionalRateLimits(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
) {
    std::vector<WidgetCompactRateLimitSummary> summaries{};
    summaries.reserve(CountVisibleAdditionalRateLimitRows(snapshot, visibility));
    for (const auto& limit : snapshot.additional_rate_limits) {
        const auto append_window = [&](const ProviderRateLimitWindow& window) {
            if (!IsAdditionalWindowVisible(limit, window, visibility)) {
                return;
            }
            const bool short_window = IsShortWindow(window);
            const std::wstring label = IsSparkLimit(limit)
                ? T(short_window ? IDS_USAGE_SPARK_5H_SHORT : IDS_USAGE_SPARK_WEEK_SHORT)
                : AdditionalWindowLabel(limit, window);
            summaries.push_back(WidgetCompactRateLimitSummary{
                label,
                FormatOptionalRemainingPercent(window.used_percent, window.available),
                NormalizeRemainingPercent(window.used_percent),
                short_window,
            });
        };
        append_window(limit.primary);
        append_window(limit.secondary);
    }
    return summaries;
}

// ----------------------------------------------------------------------------
// Formate la ligne concise de forfait et de credits.
// ----------------------------------------------------------------------------
std::wstring FormatProviderSummary(const UsageSnapshot& snapshot) {
    std::wstring summary;
    if (snapshot.identity.plan_type.has_value() && !snapshot.identity.plan_type->empty()) {
        summary = DisplayPlanType(*snapshot.identity.plan_type);
    }
    if (!snapshot.credits.available) {
        return summary;
    }

    std::wstring credits;
    if (snapshot.credits.unlimited) {
        credits = T(IDS_PROVIDER_CREDITS_UNLIMITED);
    } else if (snapshot.credits.balance.has_value()) {
        const unsigned int credit_label = UsesSingularCreditLabel(*snapshot.credits.balance)
            ? IDS_PROVIDER_CREDIT_SINGULAR
            : IDS_PROVIDER_CREDITS_PLURAL;
        credits = FormatCreditBalance(*snapshot.credits.balance) + L" " + T(credit_label);
    } else if (snapshot.credits.has_credits) {
        credits = T(IDS_PROVIDER_CREDITS_AVAILABLE);
    }
    if (credits.empty()) {
        return summary;
    }
    return summary.empty() ? credits : summary + L" \u00b7 " + credits;
}

// ----------------------------------------------------------------------------
// Dessine la ligne concise de forfait et de credits.
// ----------------------------------------------------------------------------
void DrawProviderSummary(
    const WidgetRenderUsageContext& context,
    const UsageSnapshot& snapshot,
    const D2D1_RECT_F& bounds
) {
    const std::wstring summary = FormatProviderSummary(snapshot);
    if (!summary.empty()) {
        DrawText(context, FitProviderSummary(context, summary, bounds.right - bounds.left), bounds);
    }
}

// ----------------------------------------------------------------------------
// Dessine toutes les fenetres supplementaires et retourne leur bas.
// ----------------------------------------------------------------------------
float DrawAdditionalRateLimits(
    const WidgetRenderUsageContext& context,
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility,
    float first_top,
    float left,
    float right,
    ID2D1Brush* short_window_brush,
    ID2D1Brush* long_window_brush
) {
    float top = first_top;
    for (const auto& limit : snapshot.additional_rate_limits) {
        if (IsAdditionalWindowVisible(limit, limit.primary, visibility)) {
            const bool short_window = IsShortWindow(limit.primary);
            top = DrawAdditionalWindow(
                context,
                limit,
                limit.primary,
                top,
                left,
                right,
                short_window ? short_window_brush : long_window_brush
            );
        }
        if (IsAdditionalWindowVisible(limit, limit.secondary, visibility)) {
            const bool short_window = IsShortWindow(limit.secondary);
            top = DrawAdditionalWindow(
                context,
                limit,
                limit.secondary,
                top,
                left,
                right,
                short_window ? short_window_brush : long_window_brush
            );
        }
    }
    return CountVisibleAdditionalRateLimitRows(snapshot, visibility) == 0
        ? first_top
        : top - kRowSpacing + kUsageRowHeight;
}

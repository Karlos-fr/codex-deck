// ============================================================================
// Codex Glass - Implementation des reglages de vibration du widget
// ----------------------------------------------------------------------------
// Ce fichier convertit et valide les options persistantes de vibration. Il ne
// contient aucune logique de detection ni d'animation.
// ============================================================================

#include "WidgetVibrationSettings.h"

#include <algorithm>

namespace {

// Valeur minimale acceptee pour un seuil de baisse de quota.
constexpr int kMinimumUsageDropThresholdPercent = 1;

// Valeur maximale acceptee pour un seuil de baisse de quota.
constexpr int kMaximumUsageDropThresholdPercent = 50;

// Valeur minimale acceptee pour le delai entre deux vibrations.
constexpr int kMinimumVibrationIntervalSeconds = 5;

// Valeur maximale acceptee pour le delai entre deux vibrations.
constexpr int kMaximumVibrationIntervalSeconds = 3600;

// Intensite minimale acceptee pour la vibration.
constexpr int kMinimumVibrationIntensityPercent = 25;

// Intensite maximale acceptee pour la vibration.
constexpr int kMaximumVibrationIntensityPercent = 300;

// Duree minimale acceptee pour une vibration.
constexpr int kMinimumVibrationDurationMs = 100;

// Duree maximale acceptee pour une vibration.
constexpr int kMaximumVibrationDurationMs = 2000;

// Valeur INI du style Glass.
constexpr wchar_t kWidgetVibrationStyleGlassValue[] = L"glass";

// Valeur INI du style None.
constexpr wchar_t kWidgetVibrationStyleNoneValue[] = L"none";

// Valeur INI du style Pulse.
constexpr wchar_t kWidgetVibrationStylePulseValue[] = L"pulse";

// Valeur INI du style Wave.
constexpr wchar_t kWidgetVibrationStyleWaveValue[] = L"wave";

}  // namespace

// ----------------------------------------------------------------------------
// Retourne le style de vibration utilise quand aucun reglage valide n'existe.
//
// Retour :
// - style de vibration par defaut.
// ----------------------------------------------------------------------------
WidgetVibrationStyle DefaultWidgetVibrationStyle() {
    return WidgetVibrationStyle::Glass;
}

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en style de vibration.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - style correspondant, ou style par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
WidgetVibrationStyle WidgetVibrationStyleFromString(const std::wstring& value) {
    if (value == kWidgetVibrationStyleNoneValue) {
        return WidgetVibrationStyle::None;
    }
    if (value == kWidgetVibrationStyleGlassValue) {
        return WidgetVibrationStyle::Glass;
    }
    if (value == kWidgetVibrationStylePulseValue) {
        return WidgetVibrationStyle::Pulse;
    }
    if (value == kWidgetVibrationStyleWaveValue) {
        return WidgetVibrationStyle::Wave;
    }

    return DefaultWidgetVibrationStyle();
}

// ----------------------------------------------------------------------------
// Convertit un style de vibration en chaine persistante.
//
// Parametres :
// - style : style a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring WidgetVibrationStyleToString(WidgetVibrationStyle style) {
    switch (style) {
    case WidgetVibrationStyle::None:
        return kWidgetVibrationStyleNoneValue;
    case WidgetVibrationStyle::Pulse:
        return kWidgetVibrationStylePulseValue;
    case WidgetVibrationStyle::Wave:
        return kWidgetVibrationStyleWaveValue;
    case WidgetVibrationStyle::Glass:
    default:
        return kWidgetVibrationStyleGlassValue;
    }
}

// ----------------------------------------------------------------------------
// Valide et borne les reglages de vibration.
//
// Parametres :
// - settings : reglages a normaliser.
//
// Retour :
// - reglages corriges avec bornes robustes.
// ----------------------------------------------------------------------------
WidgetVibrationSettings NormalizeWidgetVibrationSettings(WidgetVibrationSettings settings) {
    settings.usage_drop_threshold_percent = std::clamp(
        settings.usage_drop_threshold_percent,
        kMinimumUsageDropThresholdPercent,
        kMaximumUsageDropThresholdPercent
    );
    settings.minimum_interval_seconds = std::clamp(
        settings.minimum_interval_seconds,
        kMinimumVibrationIntervalSeconds,
        kMaximumVibrationIntervalSeconds
    );
    settings.intensity_percent = std::clamp(
        settings.intensity_percent,
        kMinimumVibrationIntensityPercent,
        kMaximumVibrationIntensityPercent
    );
    settings.duration_ms = std::clamp(
        settings.duration_ms,
        kMinimumVibrationDurationMs,
        kMaximumVibrationDurationMs
    );

    if (settings.enabled && !settings.motion_enabled && !settings.glass_effect_enabled) {
        settings.motion_enabled = true;
    }

    return settings;
}

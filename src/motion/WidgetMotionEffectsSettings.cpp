// ============================================================================
// Codex Glass - Persistance des Effets Motion
// ----------------------------------------------------------------------------
// Ce fichier normalise, migre et persiste le modele cumulable des Effets Motion.
// Il ne contient aucun calcul de timeline, de menu ou de rendu.
// ============================================================================

#include "WidgetMotionEffectsSettings.h"

#include <windows.h>

#include <algorithm>
#include <random>

namespace {

// Section INI partagee avec les autres reglages de l'application.
constexpr wchar_t kSettingsSection[] = L"Settings";

// Version courante du schema INI des Effets Motion.
constexpr int kMotionEffectsSchemaVersion = 2;

// Intensite minimale acceptee pour un effet.
constexpr int kMinimumIntensityPercent = 25;

// Intensite maximale acceptee pour un effet.
constexpr int kMaximumIntensityPercent = 300;

// Duree minimale acceptee pour un effet.
constexpr int kMinimumDurationMs = 100;

// Duree maximale acceptee pour un effet.
constexpr int kMaximumDurationMs = 5000;

// Valeur minimale des reglages relatifs de vitesse ou frequence.
constexpr int kMinimumRatePercent = 25;

// Valeur maximale des reglages relatifs de vitesse ou frequence.
constexpr int kMaximumRatePercent = 300;

// Valeur minimale des reglages normalises en pourcentage.
constexpr int kMinimumUnitPercent = 0;

// Valeur maximale des reglages normalises en pourcentage.
constexpr int kMaximumUnitPercent = 100;

// Refraction minimale acceptee.
constexpr int kMinimumRefractionPercent = 0;

// Refraction maximale acceptee.
constexpr int kMaximumRefractionPercent = 300;

// Ondulation minimale acceptee pour le front Wave.
constexpr int kMinimumFrontUndulationPercent = 0;

// Ondulation maximale acceptee pour le front Wave.
constexpr int kMaximumFrontUndulationPercent = 100;

// Nombre minimal d'ondes dans un train.
constexpr int kMinimumWaveCount = 1;

// Nombre maximal d'ondes dans un train.
constexpr int kMaximumWaveCount = 6;

// Seuil minimal de baisse de quota.
constexpr int kMinimumUsageDropThresholdPercent = 1;

// Seuil maximal de baisse de quota.
constexpr int kMaximumUsageDropThresholdPercent = 50;

// Cooldown minimal entre deux declenchements.
constexpr int kMinimumIntervalSeconds = 5;

// Cooldown maximal entre deux declenchements.
constexpr int kMaximumIntervalSeconds = 3600;

// ----------------------------------------------------------------------------
// Lit un entier dans la section de reglages.
//
// Parametres :
// - path : chemin INI.
// - key : cle a lire.
// - default_value : valeur de secours.
//
// Retour :
// - entier lu ou valeur de secours.
// ----------------------------------------------------------------------------
int ReadInt(const std::wstring& path, const wchar_t* key, int default_value) {
    return static_cast<int>(GetPrivateProfileIntW(
        kSettingsSection,
        key,
        default_value,
        path.c_str()
    ));
}

// ----------------------------------------------------------------------------
// Lit un booleen dans la section de reglages.
//
// Parametres :
// - path : chemin INI.
// - key : cle a lire.
// - default_value : valeur de secours.
//
// Retour :
// - true pour toute valeur non nulle.
// ----------------------------------------------------------------------------
bool ReadBool(const std::wstring& path, const wchar_t* key, bool default_value) {
    return ReadInt(path, key, default_value ? 1 : 0) != 0;
}

// ----------------------------------------------------------------------------
// Ecrit un entier dans la section de reglages.
//
// Parametres :
// - path : chemin INI.
// - key : cle a ecrire.
// - value : valeur a persister.
//
// Retour :
// - true si Win32 accepte l'ecriture.
// ----------------------------------------------------------------------------
bool WriteInt(const std::wstring& path, const wchar_t* key, int value) {
    const std::wstring text = std::to_wstring(value);
    return WritePrivateProfileStringW(
        kSettingsSection,
        key,
        text.c_str(),
        path.c_str()
    ) != FALSE;
}

// ----------------------------------------------------------------------------
// Ecrit un booleen dans la section de reglages.
//
// Parametres :
// - path : chemin INI.
// - key : cle a ecrire.
// - value : valeur a persister.
//
// Retour :
// - true si Win32 accepte l'ecriture.
// ----------------------------------------------------------------------------
bool WriteBool(const std::wstring& path, const wchar_t* key, bool value) {
    return WriteInt(path, key, value ? 1 : 0);
}

// ----------------------------------------------------------------------------
// Convertit une direction valide en entier persistant.
//
// Parametres :
// - direction : direction a convertir.
//
// Retour :
// - entier stable du schema INI.
// ----------------------------------------------------------------------------
int WaveDirectionToInt(WidgetMotionWaveDirection direction) {
    return static_cast<int>(direction);
}

// ----------------------------------------------------------------------------
// Convertit un entier persistant en direction valide.
//
// Parametres :
// - value : entier lu depuis l'INI.
//
// Retour :
// - direction correspondante ou Right en secours.
// ----------------------------------------------------------------------------
WidgetMotionWaveDirection WaveDirectionFromInt(int value) {
    switch (value) {
    case static_cast<int>(WidgetMotionWaveDirection::Left):
        return WidgetMotionWaveDirection::Left;
    case static_cast<int>(WidgetMotionWaveDirection::Up):
        return WidgetMotionWaveDirection::Up;
    case static_cast<int>(WidgetMotionWaveDirection::Down):
        return WidgetMotionWaveDirection::Down;
    case static_cast<int>(WidgetMotionWaveDirection::Radial):
        return WidgetMotionWaveDirection::Radial;
    case static_cast<int>(WidgetMotionWaveDirection::Right):
    default:
        return WidgetMotionWaveDirection::Right;
    }
}

// ----------------------------------------------------------------------------
// Convertit un entier en nombre de repetitions Pulse valide.
//
// Parametres :
// - value : entier lu depuis l'INI.
//
// Retour :
// - valeur One, Two ou Three.
// ----------------------------------------------------------------------------
WidgetMotionPulseRepetitions PulseRepetitionsFromInt(int value) {
    if (value == static_cast<int>(WidgetMotionPulseRepetitions::Two)) {
        return WidgetMotionPulseRepetitions::Two;
    }
    if (value == static_cast<int>(WidgetMotionPulseRepetitions::Three)) {
        return WidgetMotionPulseRepetitions::Three;
    }
    return WidgetMotionPulseRepetitions::One;
}

// ----------------------------------------------------------------------------
// Retourne le generateur pseudo-aleatoire partage par ce thread.
//
// Retour :
// - generateur initialise depuis une source non deterministe.
// ----------------------------------------------------------------------------
std::mt19937& RandomGenerator() {
    static thread_local std::mt19937 generator{std::random_device{}()};
    return generator;
}

// ----------------------------------------------------------------------------
// Tire un entier dans une plage inclusive.
//
// Parametres :
// - minimum : borne basse inclusive.
// - maximum : borne haute inclusive.
//
// Retour :
// - entier pseudo-aleatoire compris dans la plage.
// ----------------------------------------------------------------------------
int RandomInt(int minimum, int maximum) {
    return std::uniform_int_distribution<int>{minimum, maximum}(RandomGenerator());
}

} // namespace

// ----------------------------------------------------------------------------
// Restaure les parametres Vibration par defaut en conservant son activation.
//
// Parametres :
// - current : reglages dont seul l'etat active doit etre conserve.
//
// Retour :
// - reglages Vibration par defaut avec le meme etat active.
// ----------------------------------------------------------------------------
WidgetMotionVibrationSettings ResetWidgetMotionVibrationSettings(
    const WidgetMotionVibrationSettings& current
) {
    WidgetMotionVibrationSettings defaults{};
    defaults.enabled = current.enabled;
    return defaults;
}

// ----------------------------------------------------------------------------
// Genere des reglages Vibration aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
WidgetMotionVibrationSettings RandomizeWidgetMotionVibrationSettings(
    const WidgetMotionVibrationSettings& current
) {
    WidgetMotionVibrationSettings settings{};
    settings.enabled = current.enabled;
    settings.intensity_percent = RandomInt(25, 300);
    settings.duration_ms = RandomInt(100, 3000);
    settings.horizontal_enabled = RandomInt(0, 1) != 0;
    settings.vertical_enabled = RandomInt(0, 1) != 0;
    if (!settings.horizontal_enabled && !settings.vertical_enabled) {
        settings.horizontal_enabled = true;
    }
    settings.frequency_percent = RandomInt(25, 300);
    settings.refraction_percent = RandomInt(0, 300);
    settings.damping_percent = RandomInt(0, 100);
    return settings;
}

// ----------------------------------------------------------------------------
// Restaure les parametres Pulse par defaut en conservant son activation.
//
// Parametres :
// - current : reglages dont seul l'etat active doit etre conserve.
//
// Retour :
// - reglages Pulse par defaut avec le meme etat active.
// ----------------------------------------------------------------------------
WidgetMotionPulseSettings ResetWidgetMotionPulseSettings(
    const WidgetMotionPulseSettings& current
) {
    WidgetMotionPulseSettings defaults{};
    defaults.enabled = current.enabled;
    return defaults;
}

// ----------------------------------------------------------------------------
// Genere des reglages Pulse aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
WidgetMotionPulseSettings RandomizeWidgetMotionPulseSettings(
    const WidgetMotionPulseSettings& current
) {
    WidgetMotionPulseSettings settings{};
    settings.enabled = current.enabled;
    settings.intensity_percent = RandomInt(25, 300);
    settings.duration_ms = RandomInt(100, 3000);
    settings.repetitions = PulseRepetitionsFromInt(RandomInt(1, 3));
    settings.softness_percent = RandomInt(0, 100);
    settings.extent_percent = RandomInt(0, 100);
    return settings;
}

// ----------------------------------------------------------------------------
// Restaure les parametres Wave par defaut en conservant son activation.
//
// Parametres :
// - current : reglages dont seul l'etat active doit etre conserve.
//
// Retour :
// - reglages Wave par defaut avec le meme etat active.
// ----------------------------------------------------------------------------
WidgetMotionWaveSettings ResetWidgetMotionWaveSettings(
    const WidgetMotionWaveSettings& current
) {
    WidgetMotionWaveSettings defaults{};
    defaults.enabled = current.enabled;
    return defaults;
}

// ----------------------------------------------------------------------------
// Genere des reglages Wave aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
WidgetMotionWaveSettings RandomizeWidgetMotionWaveSettings(
    const WidgetMotionWaveSettings& current
) {
    WidgetMotionWaveSettings settings{};
    settings.enabled = current.enabled;
    settings.intensity_percent = RandomInt(25, 300);
    settings.duration_ms = RandomInt(200, 5000);
    settings.direction = WaveDirectionFromInt(RandomInt(0, 4));
    settings.speed_percent = RandomInt(25, 300);
    settings.wavelength_percent = RandomInt(25, 300);
    settings.wave_count = RandomInt(1, 6);
    settings.damping_percent = RandomInt(0, 100);
    settings.refraction_percent = RandomInt(0, 300);
    settings.front_undulation_percent = RandomInt(0, 100);
    return settings;
}

// ----------------------------------------------------------------------------
// Valide et borne tous les reglages des Effets Motion.
// ----------------------------------------------------------------------------
WidgetMotionEffectsSettings NormalizeWidgetMotionEffectsSettings(
    WidgetMotionEffectsSettings settings
) {
    settings.usage_drop_threshold_percent = std::clamp(
        settings.usage_drop_threshold_percent,
        kMinimumUsageDropThresholdPercent,
        kMaximumUsageDropThresholdPercent
    );
    settings.minimum_interval_seconds = std::clamp(
        settings.minimum_interval_seconds,
        kMinimumIntervalSeconds,
        kMaximumIntervalSeconds
    );

    settings.vibration.intensity_percent = std::clamp(
        settings.vibration.intensity_percent,
        kMinimumIntensityPercent,
        kMaximumIntensityPercent
    );
    settings.vibration.duration_ms = std::clamp(
        settings.vibration.duration_ms,
        kMinimumDurationMs,
        kMaximumDurationMs
    );
    settings.vibration.frequency_percent = std::clamp(
        settings.vibration.frequency_percent,
        kMinimumRatePercent,
        kMaximumRatePercent
    );
    settings.vibration.refraction_percent = std::clamp(
        settings.vibration.refraction_percent,
        kMinimumRefractionPercent,
        kMaximumRefractionPercent
    );
    settings.vibration.damping_percent = std::clamp(
        settings.vibration.damping_percent,
        kMinimumUnitPercent,
        kMaximumUnitPercent
    );
    if (!settings.vibration.horizontal_enabled && !settings.vibration.vertical_enabled) {
        settings.vibration.horizontal_enabled = true;
    }

    settings.pulse.intensity_percent = std::clamp(
        settings.pulse.intensity_percent,
        kMinimumIntensityPercent,
        kMaximumIntensityPercent
    );
    settings.pulse.duration_ms = std::clamp(
        settings.pulse.duration_ms,
        kMinimumDurationMs,
        kMaximumDurationMs
    );
    settings.pulse.repetitions = PulseRepetitionsFromInt(
        static_cast<int>(settings.pulse.repetitions)
    );
    settings.pulse.softness_percent = std::clamp(
        settings.pulse.softness_percent,
        kMinimumUnitPercent,
        kMaximumUnitPercent
    );
    settings.pulse.extent_percent = std::clamp(
        settings.pulse.extent_percent,
        kMinimumUnitPercent,
        kMaximumUnitPercent
    );

    settings.wave.intensity_percent = std::clamp(
        settings.wave.intensity_percent,
        kMinimumIntensityPercent,
        kMaximumIntensityPercent
    );
    settings.wave.duration_ms = std::clamp(
        settings.wave.duration_ms,
        kMinimumDurationMs,
        kMaximumDurationMs
    );
    settings.wave.direction = WaveDirectionFromInt(static_cast<int>(settings.wave.direction));
    settings.wave.speed_percent = std::clamp(
        settings.wave.speed_percent,
        kMinimumRatePercent,
        kMaximumRatePercent
    );
    settings.wave.wavelength_percent = std::clamp(
        settings.wave.wavelength_percent,
        kMinimumRatePercent,
        kMaximumRatePercent
    );
    settings.wave.wave_count = std::clamp(
        settings.wave.wave_count,
        kMinimumWaveCount,
        kMaximumWaveCount
    );
    settings.wave.damping_percent = std::clamp(
        settings.wave.damping_percent,
        kMinimumUnitPercent,
        kMaximumUnitPercent
    );
    settings.wave.refraction_percent = std::clamp(
        settings.wave.refraction_percent,
        kMinimumRefractionPercent,
        kMaximumRefractionPercent
    );
    settings.wave.front_undulation_percent = std::clamp(
        settings.wave.front_undulation_percent,
        kMinimumFrontUndulationPercent,
        kMaximumFrontUndulationPercent
    );
    return settings;
}

// ----------------------------------------------------------------------------
// Convertit les anciens reglages exclusifs vers le modele cumulable.
// ----------------------------------------------------------------------------
WidgetMotionEffectsSettings MigrateLegacyWidgetVibrationSettings(
    const WidgetVibrationSettings& legacy
) {
    const WidgetVibrationSettings normalized_legacy = NormalizeWidgetVibrationSettings(legacy);
    WidgetMotionEffectsSettings settings{};
    settings.enabled = normalized_legacy.enabled;
    settings.usage_drop_threshold_percent = 1;
    settings.minimum_interval_seconds = normalized_legacy.minimum_interval_seconds;
    settings.vibration.enabled = false;
    settings.pulse.enabled = false;
    settings.wave.enabled = false;

    switch (normalized_legacy.style) {
    case WidgetVibrationStyle::None:
        break;
    case WidgetVibrationStyle::Pulse:
        settings.pulse.enabled = true;
        settings.pulse.intensity_percent = normalized_legacy.intensity_percent;
        settings.pulse.duration_ms = normalized_legacy.duration_ms;
        break;
    case WidgetVibrationStyle::Wave:
        settings.wave.enabled = true;
        settings.wave.intensity_percent = normalized_legacy.intensity_percent;
        settings.wave.duration_ms = normalized_legacy.duration_ms;
        break;
    case WidgetVibrationStyle::Glass:
    default:
        settings.vibration.enabled = true;
        settings.vibration.intensity_percent = normalized_legacy.intensity_percent;
        settings.vibration.duration_ms = normalized_legacy.duration_ms;
        settings.vibration.horizontal_enabled = normalized_legacy.motion_enabled;
        settings.vibration.vertical_enabled = false;
        settings.vibration.refraction_percent = normalized_legacy.glass_effect_enabled
            ? normalized_legacy.intensity_percent
            : 0;
        break;
    }
    return NormalizeWidgetMotionEffectsSettings(settings);
}

// ----------------------------------------------------------------------------
// Charge les Effets Motion depuis un fichier INI ou migre les valeurs legacy.
// ----------------------------------------------------------------------------
WidgetMotionEffectsSettings LoadWidgetMotionEffectsSettings(
    const std::wstring& path,
    const WidgetVibrationSettings& legacy
) {
    const int schema_version = ReadInt(path, L"motion_effects_schema_version", 0);
    if (schema_version < 1) {
        return MigrateLegacyWidgetVibrationSettings(legacy);
    }

    WidgetMotionEffectsSettings settings{};
    settings.enabled = ReadBool(path, L"motion_effects_enabled", settings.enabled);
    settings.trigger_on_usage_drop = ReadBool(
        path,
        L"motion_effects_trigger_on_usage_drop",
        settings.trigger_on_usage_drop
    );
    settings.trigger_on_quota_reset = ReadBool(
        path,
        L"motion_effects_trigger_on_quota_reset",
        settings.trigger_on_quota_reset
    );
    settings.usage_drop_threshold_percent = ReadInt(
        path,
        L"motion_effects_usage_drop_threshold_percent",
        settings.usage_drop_threshold_percent
    );
    if (schema_version < kMotionEffectsSchemaVersion) {
        settings.usage_drop_threshold_percent = 1;
    }
    settings.minimum_interval_seconds = ReadInt(
        path,
        L"motion_effects_minimum_interval_seconds",
        settings.minimum_interval_seconds
    );
    settings.debug_menu_enabled = ReadBool(
        path,
        L"motion_effects_debug_menu_enabled",
        settings.debug_menu_enabled
    );

    settings.vibration.enabled = ReadBool(path, L"motion_vibration_enabled", settings.vibration.enabled);
    settings.vibration.intensity_percent = ReadInt(path, L"motion_vibration_intensity_percent", settings.vibration.intensity_percent);
    settings.vibration.duration_ms = ReadInt(path, L"motion_vibration_duration_ms", settings.vibration.duration_ms);
    settings.vibration.horizontal_enabled = ReadBool(path, L"motion_vibration_horizontal_enabled", settings.vibration.horizontal_enabled);
    settings.vibration.vertical_enabled = ReadBool(path, L"motion_vibration_vertical_enabled", settings.vibration.vertical_enabled);
    settings.vibration.frequency_percent = ReadInt(path, L"motion_vibration_frequency_percent", settings.vibration.frequency_percent);
    settings.vibration.refraction_percent = ReadInt(path, L"motion_vibration_refraction_percent", settings.vibration.refraction_percent);
    settings.vibration.damping_percent = ReadInt(path, L"motion_vibration_damping_percent", settings.vibration.damping_percent);

    settings.pulse.enabled = ReadBool(path, L"motion_pulse_enabled", settings.pulse.enabled);
    settings.pulse.intensity_percent = ReadInt(path, L"motion_pulse_intensity_percent", settings.pulse.intensity_percent);
    settings.pulse.duration_ms = ReadInt(path, L"motion_pulse_duration_ms", settings.pulse.duration_ms);
    settings.pulse.repetitions = PulseRepetitionsFromInt(ReadInt(path, L"motion_pulse_repetitions", static_cast<int>(settings.pulse.repetitions)));
    settings.pulse.softness_percent = ReadInt(path, L"motion_pulse_softness_percent", settings.pulse.softness_percent);
    settings.pulse.extent_percent = ReadInt(path, L"motion_pulse_extent_percent", settings.pulse.extent_percent);

    settings.wave.enabled = ReadBool(path, L"motion_wave_enabled", settings.wave.enabled);
    settings.wave.intensity_percent = ReadInt(path, L"motion_wave_intensity_percent", settings.wave.intensity_percent);
    settings.wave.duration_ms = ReadInt(path, L"motion_wave_duration_ms", settings.wave.duration_ms);
    settings.wave.direction = WaveDirectionFromInt(ReadInt(path, L"motion_wave_direction", WaveDirectionToInt(settings.wave.direction)));
    settings.wave.speed_percent = ReadInt(path, L"motion_wave_speed_percent", settings.wave.speed_percent);
    settings.wave.wavelength_percent = ReadInt(path, L"motion_wave_wavelength_percent", settings.wave.wavelength_percent);
    settings.wave.wave_count = ReadInt(path, L"motion_wave_count", settings.wave.wave_count);
    settings.wave.damping_percent = ReadInt(path, L"motion_wave_damping_percent", settings.wave.damping_percent);
    settings.wave.refraction_percent = ReadInt(path, L"motion_wave_refraction_percent", settings.wave.refraction_percent);
    settings.wave.front_undulation_percent = ReadInt(
        path,
        L"motion_wave_front_undulation_percent",
        settings.wave.front_undulation_percent
    );
    return NormalizeWidgetMotionEffectsSettings(settings);
}

// ----------------------------------------------------------------------------
// Sauvegarde les Effets Motion et leur version de schema dans un fichier INI.
// ----------------------------------------------------------------------------
bool SaveWidgetMotionEffectsSettings(
    const std::wstring& path,
    const WidgetMotionEffectsSettings& source
) {
    const WidgetMotionEffectsSettings settings = NormalizeWidgetMotionEffectsSettings(source);
    bool success = true;
    success = WriteInt(path, L"motion_effects_schema_version", kMotionEffectsSchemaVersion) && success;
    success = WriteBool(path, L"motion_effects_enabled", settings.enabled) && success;
    success = WriteBool(path, L"motion_effects_trigger_on_usage_drop", settings.trigger_on_usage_drop) && success;
    success = WriteBool(path, L"motion_effects_trigger_on_quota_reset", settings.trigger_on_quota_reset) && success;
    success = WriteInt(path, L"motion_effects_usage_drop_threshold_percent", settings.usage_drop_threshold_percent) && success;
    success = WriteInt(path, L"motion_effects_minimum_interval_seconds", settings.minimum_interval_seconds) && success;
    success = WriteBool(path, L"motion_effects_debug_menu_enabled", settings.debug_menu_enabled) && success;

    success = WriteBool(path, L"motion_vibration_enabled", settings.vibration.enabled) && success;
    success = WriteInt(path, L"motion_vibration_intensity_percent", settings.vibration.intensity_percent) && success;
    success = WriteInt(path, L"motion_vibration_duration_ms", settings.vibration.duration_ms) && success;
    success = WriteBool(path, L"motion_vibration_horizontal_enabled", settings.vibration.horizontal_enabled) && success;
    success = WriteBool(path, L"motion_vibration_vertical_enabled", settings.vibration.vertical_enabled) && success;
    success = WriteInt(path, L"motion_vibration_frequency_percent", settings.vibration.frequency_percent) && success;
    success = WriteInt(path, L"motion_vibration_refraction_percent", settings.vibration.refraction_percent) && success;
    success = WriteInt(path, L"motion_vibration_damping_percent", settings.vibration.damping_percent) && success;

    success = WriteBool(path, L"motion_pulse_enabled", settings.pulse.enabled) && success;
    success = WriteInt(path, L"motion_pulse_intensity_percent", settings.pulse.intensity_percent) && success;
    success = WriteInt(path, L"motion_pulse_duration_ms", settings.pulse.duration_ms) && success;
    success = WriteInt(path, L"motion_pulse_repetitions", static_cast<int>(settings.pulse.repetitions)) && success;
    success = WriteInt(path, L"motion_pulse_softness_percent", settings.pulse.softness_percent) && success;
    success = WriteInt(path, L"motion_pulse_extent_percent", settings.pulse.extent_percent) && success;

    success = WriteBool(path, L"motion_wave_enabled", settings.wave.enabled) && success;
    success = WriteInt(path, L"motion_wave_intensity_percent", settings.wave.intensity_percent) && success;
    success = WriteInt(path, L"motion_wave_duration_ms", settings.wave.duration_ms) && success;
    success = WriteInt(path, L"motion_wave_direction", WaveDirectionToInt(settings.wave.direction)) && success;
    success = WriteInt(path, L"motion_wave_speed_percent", settings.wave.speed_percent) && success;
    success = WriteInt(path, L"motion_wave_wavelength_percent", settings.wave.wavelength_percent) && success;
    success = WriteInt(path, L"motion_wave_count", settings.wave.wave_count) && success;
    success = WriteInt(path, L"motion_wave_damping_percent", settings.wave.damping_percent) && success;
    success = WriteInt(path, L"motion_wave_refraction_percent", settings.wave.refraction_percent) && success;
    success = WriteInt(
        path,
        L"motion_wave_front_undulation_percent",
        settings.wave.front_undulation_percent
    ) && success;
    return success;
}

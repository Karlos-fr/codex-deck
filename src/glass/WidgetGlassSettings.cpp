// ============================================================================
// Codex Glass - Implementation des reglages de l'effet glass
// ----------------------------------------------------------------------------
// Ce fichier convertit les modes glass entre enum interne et valeurs persistantes
// stables. Il ne contient aucun code de rendu ni d'effet systeme.
// ============================================================================

#include "WidgetGlassSettings.h"

#include <algorithm>
#include <random>

namespace {

// Valeur INI du mode sans effet glass.
constexpr wchar_t kGlassModeOffValue[] = L"off";

// Valeur INI du mode GlassEffect actif.
constexpr wchar_t kGlassModeGlassEffectValue[] = L"glass_effect";

// Valeur INI du preset GlassEffect clair.
constexpr wchar_t kGlassEffectPresetClearValue[] = L"clear";

// Valeur INI du preset GlassEffect givree.
constexpr wchar_t kGlassEffectPresetFrostedValue[] = L"frosted";

// Valeur INI du preset GlassEffect subtil.
constexpr wchar_t kGlassEffectPresetSubtleValue[] = L"subtle";

// Valeur INI du preset GlassEffect fort.
constexpr wchar_t kGlassEffectPresetStrongValue[] = L"strong";

// Valeur INI d'une apparence GlassEffect personnalisee.
constexpr wchar_t kGlassEffectPresetCustomValue[] = L"custom";

// Valeur INI du mode d'animation GlassEffect desactive.
constexpr wchar_t kGlassEffectAnimationOffValue[] = L"off";

// Valeur INI du mode d'animation GlassEffect eau calme.
constexpr wchar_t kGlassEffectAnimationCalmWaterValue[] = L"calm_water";

// Valeur INI du mode d'animation GlassEffect liquide.
constexpr wchar_t kGlassEffectAnimationLiquidValue[] = L"liquid";

// Valeur INI du mode d'animation GlassEffect pluie.
constexpr wchar_t kGlassEffectAnimationRainValue[] = L"rain";

// Pourcentage minimal commun aux sliders d'animation GlassEffect.
constexpr int kMinimumGlassEffectAnimationPercent = 25;

// Pourcentage maximal de l'intensite des animations GlassEffect.
constexpr int kMaximumGlassEffectAnimationIntensityPercent = 200;

// Pourcentage maximal de vitesse des animations GlassEffect.
constexpr int kMaximumGlassEffectAnimationSpeedPercent = 200;

// Pourcentage maximal de densite des impacts Rain.
constexpr int kMaximumGlassEffectRainDensityPercent = 250;

// Pourcentage minimal de taille des anneaux Rain.
constexpr int kMinimumGlassEffectRainRingSizePercent = 50;

// Pourcentage maximal de taille des anneaux Rain.
constexpr int kMaximumGlassEffectRainRingSizePercent = 200;

// Version courante du schema GlassEffect cumulable.
constexpr int kCurrentGlassEffectSchemaVersion = 1;

// Compare deux structures d'apparence champ par champ.
bool SameAppearance(
    const GlassEffectAppearanceSettings& first,
    const GlassEffectAppearanceSettings& second
) {
    return first.diffusion_percent == second.diffusion_percent
        && first.tint_percent == second.tint_percent
        && first.grain_percent == second.grain_percent
        && first.edge_refraction_percent == second.edge_refraction_percent
        && first.edge_width_percent == second.edge_width_percent
        && first.chromatic_aberration_percent == second.chromatic_aberration_percent
        && first.indicator_refraction_percent == second.indicator_refraction_percent
        && first.indicator_width_percent == second.indicator_width_percent
        && first.element_glass_enabled == second.element_glass_enabled
        && first.element_softness_percent == second.element_softness_percent;
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

}  // namespace

// ----------------------------------------------------------------------------
// Retourne le mode glass utilise quand aucun reglage valide n'existe.
//
// Retour :
// - mode glass par defaut.
// ----------------------------------------------------------------------------
GlassEffectMode DefaultGlassEffectMode() {
    return GlassEffectMode::Off;
}

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en mode glass.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - mode glass correspondant, ou mode par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
GlassEffectMode GlassEffectModeFromString(const std::wstring& value) {
    if (value == kGlassModeGlassEffectValue) {
        return GlassEffectMode::Enabled;
    }

    return DefaultGlassEffectMode();
}

// ----------------------------------------------------------------------------
// Convertit un mode glass en chaine persistante.
//
// Parametres :
// - mode : mode glass a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring GlassEffectModeToString(GlassEffectMode mode) {
    switch (mode) {
    case GlassEffectMode::Enabled:
        return kGlassModeGlassEffectValue;

    case GlassEffectMode::Off:
    default:
        return kGlassModeOffValue;
    }
}

// ----------------------------------------------------------------------------
// Retourne le preset GlassEffect utilise quand aucun reglage valide n'existe.
//
// Retour :
// - preset GlassEffect par defaut.
// ----------------------------------------------------------------------------
GlassEffectPreset DefaultGlassEffectPreset() {
    return GlassEffectPreset::Frosted;
}

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en preset GlassEffect.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - preset GlassEffect correspondant, ou preset par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
GlassEffectPreset GlassEffectPresetFromString(const std::wstring& value) {
    if (value == kGlassEffectPresetClearValue) {
        return GlassEffectPreset::Clear;
    }

    if (value == kGlassEffectPresetSubtleValue) {
        return GlassEffectPreset::Subtle;
    }

    if (value == kGlassEffectPresetStrongValue) {
        return GlassEffectPreset::Strong;
    }

    if (value == kGlassEffectPresetFrostedValue) {
        return GlassEffectPreset::Frosted;
    }

    if (value == kGlassEffectPresetCustomValue) {
        return GlassEffectPreset::Custom;
    }

    return DefaultGlassEffectPreset();
}

// ----------------------------------------------------------------------------
// Convertit un preset GlassEffect en chaine persistante.
//
// Parametres :
// - preset : preset GlassEffect a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring GlassEffectPresetToString(GlassEffectPreset preset) {
    switch (preset) {
    case GlassEffectPreset::Clear:
        return kGlassEffectPresetClearValue;

    case GlassEffectPreset::Subtle:
        return kGlassEffectPresetSubtleValue;

    case GlassEffectPreset::Strong:
        return kGlassEffectPresetStrongValue;

    case GlassEffectPreset::Custom:
        return kGlassEffectPresetCustomValue;

    case GlassEffectPreset::Frosted:
    default:
        return kGlassEffectPresetFrostedValue;
    }
}

// ----------------------------------------------------------------------------
// Retourne le mode d'animation GlassEffect utilise par defaut.
//
// Retour :
// - mode d'animation GlassEffect par defaut.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationMode DefaultLegacyGlassEffectAnimationMode() {
    return LegacyGlassEffectAnimationMode::Off;
}

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en mode d'animation GlassEffect.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - mode d'animation correspondant, ou mode par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationMode LegacyGlassEffectAnimationModeFromString(const std::wstring& value) {
    if (value == kGlassEffectAnimationCalmWaterValue) {
        return LegacyGlassEffectAnimationMode::CalmWater;
    }

    if (value == kGlassEffectAnimationLiquidValue) {
        return LegacyGlassEffectAnimationMode::Liquid;
    }

    if (value == kGlassEffectAnimationRainValue) {
        return LegacyGlassEffectAnimationMode::Rain;
    }

    return DefaultLegacyGlassEffectAnimationMode();
}

// ----------------------------------------------------------------------------
// Convertit un mode d'animation GlassEffect en chaine persistante.
//
// Parametres :
// - mode : mode d'animation a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring LegacyGlassEffectAnimationModeToString(LegacyGlassEffectAnimationMode mode) {
    switch (mode) {
    case LegacyGlassEffectAnimationMode::CalmWater:
        return kGlassEffectAnimationCalmWaterValue;

    case LegacyGlassEffectAnimationMode::Liquid:
        return kGlassEffectAnimationLiquidValue;

    case LegacyGlassEffectAnimationMode::Rain:
        return kGlassEffectAnimationRainValue;

    case LegacyGlassEffectAnimationMode::Off:
    default:
        return kGlassEffectAnimationOffValue;
    }
}

// ----------------------------------------------------------------------------
// Retourne les options d'animation GlassEffect par defaut.
//
// Retour :
// - options d'animation utilisees quand aucun reglage valide n'existe.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationOptions DefaultLegacyGlassEffectAnimationOptions() {
    return LegacyGlassEffectAnimationOptions{};
}

// ----------------------------------------------------------------------------
// Encadre les options d'animation GlassEffect dans les plages supportees.
//
// Parametres :
// - options : options a normaliser.
//
// Retour :
// - options avec valeurs utilisables par le rendu et le menu.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationOptions NormalizeLegacyGlassEffectAnimationOptions(
    LegacyGlassEffectAnimationOptions options
) {
    options.intensity_percent = std::clamp(
        options.intensity_percent,
        kMinimumGlassEffectAnimationPercent,
        kMaximumGlassEffectAnimationIntensityPercent
    );
    options.speed_percent = std::clamp(
        options.speed_percent,
        kMinimumGlassEffectAnimationPercent,
        kMaximumGlassEffectAnimationSpeedPercent
    );
    options.rain_density_percent = std::clamp(
        options.rain_density_percent,
        kMinimumGlassEffectAnimationPercent,
        kMaximumGlassEffectRainDensityPercent
    );
    options.rain_ring_size_percent = std::clamp(
        options.rain_ring_size_percent,
        kMinimumGlassEffectRainRingSizePercent,
        kMaximumGlassEffectRainRingSizePercent
    );
    return options;
}

// ----------------------------------------------------------------------------
// Retourne l'apparence complete associee a un preset.
// ----------------------------------------------------------------------------
GlassEffectAppearanceSettings GlassEffectAppearanceForPreset(GlassEffectPreset preset) {
    switch (preset) {
    case GlassEffectPreset::Clear:
        return GlassEffectAppearanceSettings{46, 54, 40, 61, 72, 52, 63, 67};
    case GlassEffectPreset::Subtle:
        return GlassEffectAppearanceSettings{71, 79, 67, 78, 83, 70, 75, 83};
    case GlassEffectPreset::Strong:
        return GlassEffectAppearanceSettings{121, 115, 133, 133, 121, 122, 138, 125};
    case GlassEffectPreset::Frosted:
    case GlassEffectPreset::Custom:
    default:
        return GlassEffectAppearanceSettings{};
    }
}

// ----------------------------------------------------------------------------
// Retourne le bloc GlassEffect utilise par defaut.
// ----------------------------------------------------------------------------
GlassEffectSettings DefaultGlassEffectSettings() {
    GlassEffectSettings settings{};
    settings.schema_version = kCurrentGlassEffectSchemaVersion;
    settings.preset = DefaultGlassEffectPreset();
    settings.appearance = GlassEffectAppearanceForPreset(settings.preset);
    return settings;
}

// ----------------------------------------------------------------------------
// Normalise les reglages d'apparence dans les bornes du renderer.
// ----------------------------------------------------------------------------
GlassEffectAppearanceSettings NormalizeGlassEffectAppearanceSettings(
    GlassEffectAppearanceSettings settings
) {
    settings.diffusion_percent = std::clamp(settings.diffusion_percent, 0, 200);
    settings.tint_percent = std::clamp(settings.tint_percent, 0, 200);
    settings.grain_percent = std::clamp(settings.grain_percent, 0, 200);
    settings.edge_refraction_percent = std::clamp(settings.edge_refraction_percent, 0, 200);
    settings.edge_width_percent = std::clamp(settings.edge_width_percent, 25, 200);
    settings.chromatic_aberration_percent = std::clamp(
        settings.chromatic_aberration_percent, 0, 200
    );
    settings.indicator_refraction_percent = std::clamp(
        settings.indicator_refraction_percent, -200, 200
    );
    settings.indicator_width_percent = std::clamp(settings.indicator_width_percent, 25, 250);
    settings.element_softness_percent = std::clamp(settings.element_softness_percent, 25, 200);
    return settings;
}

// ----------------------------------------------------------------------------
// Genere une apparence Glass aleatoire dans les bornes du menu.
// ----------------------------------------------------------------------------
GlassEffectAppearanceSettings RandomizeGlassEffectAppearanceSettings() {
    GlassEffectAppearanceSettings settings{};
    settings.diffusion_percent = RandomInt(0, 200);
    settings.tint_percent = RandomInt(0, 200);
    settings.grain_percent = RandomInt(0, 200);
    settings.edge_refraction_percent = RandomInt(0, 200);
    settings.edge_width_percent = RandomInt(25, 200);
    settings.chromatic_aberration_percent = RandomInt(0, 200);
    settings.indicator_refraction_percent = RandomInt(-200, 200);
    settings.indicator_width_percent = RandomInt(25, 250);
    settings.element_glass_enabled = RandomInt(0, 1) != 0;
    settings.element_softness_percent = RandomInt(25, 200);
    return NormalizeGlassEffectAppearanceSettings(settings);
}

// ----------------------------------------------------------------------------
// Genere une opacite de fenetre aleatoire exprimee en pourcentage.
// ----------------------------------------------------------------------------
int RandomGlassEffectOpacityPercent() {
    return RandomInt(10, 100);
}

// ----------------------------------------------------------------------------
// Normalise les reglages Calm Water.
// ----------------------------------------------------------------------------
GlassEffectCalmWaterSettings NormalizeGlassEffectCalmWaterSettings(
    GlassEffectCalmWaterSettings settings
) {
    settings.intensity_percent = std::clamp(settings.intensity_percent, 0, 200);
    settings.speed_percent = std::clamp(settings.speed_percent, 25, 200);
    settings.wavelength_percent = std::clamp(settings.wavelength_percent, 25, 300);
    settings.noise_percent = std::clamp(settings.noise_percent, 0, 200);
    return settings;
}

// ----------------------------------------------------------------------------
// Genere des reglages Calm Water aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
GlassEffectCalmWaterSettings RandomizeGlassEffectCalmWaterSettings(
    GlassEffectCalmWaterSettings current
) {
    const bool enabled = current.enabled;
    current.intensity_percent = RandomInt(25, 200);
    current.speed_percent = RandomInt(25, 200);
    current.wavelength_percent = RandomInt(25, 300);
    current.noise_percent = RandomInt(0, 200);
    current.enabled = enabled;
    return NormalizeGlassEffectCalmWaterSettings(current);
}

// ----------------------------------------------------------------------------
// Normalise les reglages Liquid.
// ----------------------------------------------------------------------------
GlassEffectLiquidSettings NormalizeGlassEffectLiquidSettings(
    GlassEffectLiquidSettings settings
) {
    settings.intensity_percent = std::clamp(settings.intensity_percent, 0, 200);
    settings.speed_percent = std::clamp(settings.speed_percent, 25, 200);
    settings.wavelength_percent = std::clamp(settings.wavelength_percent, 25, 300);
    settings.fluidity_percent = std::clamp(settings.fluidity_percent, 25, 200);
    settings.noise_percent = std::clamp(settings.noise_percent, 0, 200);
    return settings;
}

// ----------------------------------------------------------------------------
// Genere des reglages Liquid aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
GlassEffectLiquidSettings RandomizeGlassEffectLiquidSettings(
    GlassEffectLiquidSettings current
) {
    const bool enabled = current.enabled;
    current.intensity_percent = RandomInt(25, 200);
    current.speed_percent = RandomInt(25, 200);
    current.wavelength_percent = RandomInt(25, 300);
    current.fluidity_percent = RandomInt(25, 200);
    current.noise_percent = RandomInt(0, 200);
    current.enabled = enabled;
    return NormalizeGlassEffectLiquidSettings(current);
}

// ----------------------------------------------------------------------------
// Normalise les reglages Rain.
// ----------------------------------------------------------------------------
GlassEffectRainSettings NormalizeGlassEffectRainSettings(GlassEffectRainSettings settings) {
    settings.intensity_percent = std::clamp(settings.intensity_percent, 0, 200);
    settings.speed_percent = std::clamp(settings.speed_percent, 25, 200);
    settings.density_percent = std::clamp(settings.density_percent, 25, 250);
    settings.ring_size_percent = std::clamp(settings.ring_size_percent, 50, 200);
    settings.fade_percent = std::clamp(settings.fade_percent, 25, 200);
    return settings;
}

// ----------------------------------------------------------------------------
// Genere des reglages Rain aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
GlassEffectRainSettings RandomizeGlassEffectRainSettings(GlassEffectRainSettings current) {
    const bool enabled = current.enabled;
    current.intensity_percent = RandomInt(25, 200);
    current.speed_percent = RandomInt(25, 200);
    current.density_percent = RandomInt(25, 250);
    current.ring_size_percent = RandomInt(50, 200);
    current.fade_percent = RandomInt(25, 200);
    current.enabled = enabled;
    return NormalizeGlassEffectRainSettings(current);
}

// ----------------------------------------------------------------------------
// Retourne le preset exact correspondant a une apparence.
// ----------------------------------------------------------------------------
GlassEffectPreset MatchingGlassEffectPreset(const GlassEffectAppearanceSettings& settings) {
    const GlassEffectAppearanceSettings normalized = NormalizeGlassEffectAppearanceSettings(settings);
    for (const GlassEffectPreset preset : {
            GlassEffectPreset::Clear,
            GlassEffectPreset::Frosted,
            GlassEffectPreset::Subtle,
            GlassEffectPreset::Strong,
        }) {
        if (SameAppearance(normalized, GlassEffectAppearanceForPreset(preset))) {
            return preset;
        }
    }
    return GlassEffectPreset::Custom;
}

// ----------------------------------------------------------------------------
// Normalise l'ensemble du bloc GlassEffect et recalcule son preset affiche.
// ----------------------------------------------------------------------------
GlassEffectSettings NormalizeGlassEffectSettings(GlassEffectSettings settings) {
    settings.schema_version = kCurrentGlassEffectSchemaVersion;
    settings.appearance = NormalizeGlassEffectAppearanceSettings(settings.appearance);
    settings.calm_water = NormalizeGlassEffectCalmWaterSettings(settings.calm_water);
    settings.liquid = NormalizeGlassEffectLiquidSettings(settings.liquid);
    settings.rain = NormalizeGlassEffectRainSettings(settings.rain);
    settings.preset = MatchingGlassEffectPreset(settings.appearance);
    return settings;
}

// ----------------------------------------------------------------------------
// Indique si au moins une animation GlassEffect continue est active.
// ----------------------------------------------------------------------------
bool HasActiveGlassEffectAnimation(const GlassEffectSettings& settings) {
    return settings.calm_water.enabled || settings.liquid.enabled || settings.rain.enabled;
}

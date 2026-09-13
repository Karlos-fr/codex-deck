// ============================================================================
// Codex Deck - Implementation du menu contextuel Glass
// ----------------------------------------------------------------------------
// Ce fichier assemble presets, sliders, checkboxes et sous-menus activables en
// reprenant les composants owner-drawn eprouves de Codex Glass.
// ============================================================================

#include "DeckGlassMenu.h"

#include "WidgetMenuCheckbox.h"
#include "WidgetMenuOwnerDraw.h"
#include "WidgetMenuRadio.h"
#include "WidgetMenuSlider.h"
#include "WidgetMenuToggleSubmenu.h"

#include <algorithm>
#include <vector>

namespace {

// Identifiants prives des commandes du menu Glass.
enum GlassMenuCommand : UINT {
    PresetClear = 41001,
    PresetFrosted,
    PresetSubtle,
    PresetStrong,
    PresetCustom,
    ToggleElementGlass,
    ToggleCalmWater,
    ToggleLiquid,
    ToggleRain,
    RandomizeAppearance,
    ResetAppearance,
    RandomizeCalm,
    ResetCalm,
    RandomizeLiquid,
    ResetLiquid,
    RandomizeRain,
    ResetRain,
    Opacity,
    Diffusion,
    Tint,
    Grain,
    EdgeRefraction,
    EdgeWidth,
    ChromaticAberration,
    ElementZoom,
    ElementExtent,
    ElementSoftness,
    CalmIntensity,
    CalmSpeed,
    CalmWavelength,
    CalmNoise,
    LiquidIntensity,
    LiquidSpeed,
    LiquidWavelength,
    LiquidFluidity,
    LiquidNoise,
    RainIntensity,
    RainSpeed,
    RainDensity,
    RainRingSize,
    RainFade,
};

// Groupe exclusif des presets d'apparence.
constexpr UINT kPresetGroup = 1;

// Retourne l'identifiant radio du preset courant.
UINT PresetCommand(GlassEffectPreset preset) {
    switch (preset) {
    case GlassEffectPreset::Clear: return PresetClear;
    case GlassEffectPreset::Frosted: return PresetFrosted;
    case GlassEffectPreset::Subtle: return PresetSubtle;
    case GlassEffectPreset::Strong: return PresetStrong;
    case GlassEffectPreset::Custom:
    default: return PresetCustom;
    }
}

// Ajoute les sliders d'apparence et leurs descriptions de suivi.
void AppendAppearanceControls(HMENU menu, const DeckGlassSettings& settings, std::vector<WidgetMenuSliderSpec>& sliders) {
    const auto add = [&](UINT id, const wchar_t* label, int minimum, int maximum, int value) {
        sliders.push_back(WidgetMenuSliderSpec{id, label, kDeckGlassSliderChangedMessage, minimum, maximum, value, L" %"});
        AppendWidgetMenuSliderItem(menu, id);
    };
    add(Opacity, L"Opacity", 20, 100, settings.opacity_percent);
    add(Diffusion, L"Diffusion", 0, 200, settings.effect.appearance.diffusion_percent);
    add(Tint, L"Tint", 0, 200, settings.effect.appearance.tint_percent);
    add(Grain, L"Grain", 0, 200, settings.effect.appearance.grain_percent);
    add(EdgeRefraction, L"Edge refraction", 0, 200, settings.effect.appearance.edge_refraction_percent);
    add(EdgeWidth, L"Edge width", 25, 200, settings.effect.appearance.edge_width_percent);
    add(ChromaticAberration, L"Chromatic aberration", 0, 200, settings.effect.appearance.chromatic_aberration_percent);
    AppendMenuSeparator(menu);
    AppendWidgetMenuCheckboxItem(menu, ToggleElementGlass);
    add(ElementZoom, L"Zoom", -200, 200, settings.effect.appearance.indicator_refraction_percent);
    add(ElementExtent, L"Local glass extent", 25, 250, settings.effect.appearance.indicator_width_percent);
    add(ElementSoftness, L"Softness", 25, 200, settings.effect.appearance.element_softness_percent);
}

// Ajoute un slider d'animation et sa description de suivi.
void AppendAnimationSlider(
    HMENU menu,
    std::vector<WidgetMenuSliderSpec>& sliders,
    UINT id,
    const wchar_t* label,
    int minimum,
    int maximum,
    int value
) {
    sliders.push_back(WidgetMenuSliderSpec{id, label, kDeckGlassSliderChangedMessage, minimum, maximum, value, L" %"});
    AppendWidgetMenuSliderItem(menu, id);
}

// Applique un preset d'apparence sans modifier les animations actives.
void ApplyPreset(DeckGlassSettings& settings, GlassEffectPreset preset) {
    settings.effect.preset = preset;
    settings.effect.appearance = GlassEffectAppearanceForPreset(preset);
}

}  // namespace

// ----------------------------------------------------------------------------
// Affiche le menu Glass a la position ecran demandee.
// ----------------------------------------------------------------------------
void ShowDeckGlassMenu(HWND hwnd, POINT point, const DeckGlassSettings& source, COLORREF accent) {
    const DeckGlassSettings settings{source};
    ResetOwnerMenuDrawState();

    std::vector<WidgetMenuSliderSpec> sliders;
    sliders.reserve(24);
    const std::vector<WidgetMenuCheckboxSpec> checkboxes{
        {ToggleElementGlass, L"Local element glass", L"Local element glass", settings.effect.appearance.element_glass_enabled},
    };
    const UINT selected_preset = PresetCommand(settings.effect.preset);
    const std::vector<WidgetMenuRadioSpec> radios{
        {PresetClear, L"Clear", selected_preset == PresetClear, kPresetGroup},
        {PresetFrosted, L"Frosted", selected_preset == PresetFrosted, kPresetGroup},
        {PresetSubtle, L"Subtle", selected_preset == PresetSubtle, kPresetGroup},
        {PresetStrong, L"Strong", selected_preset == PresetStrong, kPresetGroup},
        {PresetCustom, L"Custom", selected_preset == PresetCustom, kPresetGroup},
    };
    const std::vector<WidgetMenuToggleSubmenuSpec> toggles{
        {ToggleCalmWater, L"Calm Water", L"Calm Water", settings.effect.calm_water.enabled},
        {ToggleLiquid, L"Liquid", L"Liquid", settings.effect.liquid.enabled},
        {ToggleRain, L"Rain", L"Rain", settings.effect.rain.enabled},
    };

    BeginWidgetMenuCheckboxTracking(hwnd, checkboxes, accent);
    BeginWidgetMenuRadioTracking(hwnd, radios, accent);
    BeginWidgetMenuToggleSubmenuTracking(hwnd, toggles, accent);
    BeginWidgetMenuSliderTracking(hwnd, sliders, accent);
    BeginOwnerMenuWindowPolish();

    HMENU root = CreateOwnerPopupMenu();
    HMENU appearance = CreateOwnerPopupMenu();
    HMENU presets = CreateOwnerPopupMenu();
    for (const WidgetMenuRadioSpec& radio : radios) {
        AppendWidgetMenuRadioItem(presets, radio.command_id);
    }
    AppendSubMenu(appearance, presets, L"Preset");
    AppendMenuSeparator(appearance);
    AppendAppearanceControls(appearance, settings, sliders);
    AppendMenuSeparator(appearance);
    AppendCommandMenuItem(appearance, RandomizeAppearance, L"Randomize settings");
    AppendCommandMenuItem(appearance, ResetAppearance, L"Reset");
    AppendSubMenu(root, appearance, L"Glass appearance");

    HMENU calm = CreateOwnerPopupMenu();
    AppendAnimationSlider(calm, sliders, CalmIntensity, L"Intensity", 0, 200, settings.effect.calm_water.intensity_percent);
    AppendAnimationSlider(calm, sliders, CalmSpeed, L"Speed", 25, 200, settings.effect.calm_water.speed_percent);
    AppendAnimationSlider(calm, sliders, CalmWavelength, L"Wavelength", 25, 300, settings.effect.calm_water.wavelength_percent);
    AppendAnimationSlider(calm, sliders, CalmNoise, L"Noise", 0, 200, settings.effect.calm_water.noise_percent);
    AppendMenuSeparator(calm);
    AppendCommandMenuItem(calm, RandomizeCalm, L"Randomize settings");
    AppendCommandMenuItem(calm, ResetCalm, L"Reset");
    AppendWidgetMenuToggleSubmenuItem(root, calm, ToggleCalmWater);

    HMENU liquid = CreateOwnerPopupMenu();
    AppendAnimationSlider(liquid, sliders, LiquidIntensity, L"Intensity", 0, 200, settings.effect.liquid.intensity_percent);
    AppendAnimationSlider(liquid, sliders, LiquidSpeed, L"Speed", 25, 200, settings.effect.liquid.speed_percent);
    AppendAnimationSlider(liquid, sliders, LiquidWavelength, L"Wavelength", 25, 300, settings.effect.liquid.wavelength_percent);
    AppendAnimationSlider(liquid, sliders, LiquidFluidity, L"Fluidity", 25, 200, settings.effect.liquid.fluidity_percent);
    AppendAnimationSlider(liquid, sliders, LiquidNoise, L"Noise", 0, 200, settings.effect.liquid.noise_percent);
    AppendMenuSeparator(liquid);
    AppendCommandMenuItem(liquid, RandomizeLiquid, L"Randomize settings");
    AppendCommandMenuItem(liquid, ResetLiquid, L"Reset");
    AppendWidgetMenuToggleSubmenuItem(root, liquid, ToggleLiquid);

    HMENU rain = CreateOwnerPopupMenu();
    AppendAnimationSlider(rain, sliders, RainIntensity, L"Intensity", 0, 200, settings.effect.rain.intensity_percent);
    AppendAnimationSlider(rain, sliders, RainSpeed, L"Speed", 25, 200, settings.effect.rain.speed_percent);
    AppendAnimationSlider(rain, sliders, RainDensity, L"Density", 25, 250, settings.effect.rain.density_percent);
    AppendAnimationSlider(rain, sliders, RainRingSize, L"Ring size", 50, 200, settings.effect.rain.ring_size_percent);
    AppendAnimationSlider(rain, sliders, RainFade, L"Fade", 25, 200, settings.effect.rain.fade_percent);
    AppendMenuSeparator(rain);
    AppendCommandMenuItem(rain, RandomizeRain, L"Randomize settings");
    AppendCommandMenuItem(rain, ResetRain, L"Reset");
    AppendWidgetMenuToggleSubmenuItem(root, rain, ToggleRain);

    EndWidgetMenuSliderTracking();
    BeginWidgetMenuSliderTracking(hwnd, sliders, accent);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(root, TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hwnd, nullptr);

    EndOwnerMenuWindowPolish();
    EndWidgetMenuSliderTracking();
    EndWidgetMenuToggleSubmenuTracking();
    EndWidgetMenuRadioTracking();
    EndWidgetMenuCheckboxTracking();
    DestroyMenu(root);
    ResetOwnerMenuDrawState();
}

// ----------------------------------------------------------------------------
// Applique une commande discrete du menu aux reglages.
// ----------------------------------------------------------------------------
bool HandleDeckGlassMenuCommand(UINT id, DeckGlassSettings& settings) {
    switch (id) {
    case PresetClear: ApplyPreset(settings, GlassEffectPreset::Clear); break;
    case PresetFrosted: ApplyPreset(settings, GlassEffectPreset::Frosted); break;
    case PresetSubtle: ApplyPreset(settings, GlassEffectPreset::Subtle); break;
    case PresetStrong: ApplyPreset(settings, GlassEffectPreset::Strong); break;
    case PresetCustom: settings.effect.preset = GlassEffectPreset::Custom; break;
    case ToggleElementGlass: settings.effect.appearance.element_glass_enabled = !settings.effect.appearance.element_glass_enabled; break;
    case ToggleCalmWater: settings.effect.calm_water.enabled = !settings.effect.calm_water.enabled; break;
    case ToggleLiquid: settings.effect.liquid.enabled = !settings.effect.liquid.enabled; break;
    case ToggleRain: settings.effect.rain.enabled = !settings.effect.rain.enabled; break;
    case RandomizeAppearance: settings.effect.appearance = RandomizeGlassEffectAppearanceSettings(); settings.effect.preset = GlassEffectPreset::Custom; break;
    case ResetAppearance: ApplyPreset(settings, DefaultGlassEffectPreset()); break;
    case RandomizeCalm: settings.effect.calm_water = RandomizeGlassEffectCalmWaterSettings(settings.effect.calm_water); break;
    case ResetCalm: { const bool enabled = settings.effect.calm_water.enabled; settings.effect.calm_water = {}; settings.effect.calm_water.enabled = enabled; break; }
    case RandomizeLiquid: settings.effect.liquid = RandomizeGlassEffectLiquidSettings(settings.effect.liquid); break;
    case ResetLiquid: { const bool enabled = settings.effect.liquid.enabled; settings.effect.liquid = {}; settings.effect.liquid.enabled = enabled; break; }
    case RandomizeRain: settings.effect.rain = RandomizeGlassEffectRainSettings(settings.effect.rain); break;
    case ResetRain: { const bool enabled = settings.effect.rain.enabled; settings.effect.rain = {}; settings.effect.rain.enabled = enabled; break; }
    default: return false;
    }
    settings.effect = NormalizeGlassEffectSettings(settings.effect);
    return true;
}

// ----------------------------------------------------------------------------
// Applique la valeur d'un slider Glass.
// ----------------------------------------------------------------------------
bool HandleDeckGlassSlider(UINT id, int value, DeckGlassSettings& settings) {
    switch (id) {
    case Opacity: settings.opacity_percent = std::clamp(value, 20, 100); return true;
    case Diffusion: settings.effect.appearance.diffusion_percent = value; break;
    case Tint: settings.effect.appearance.tint_percent = value; break;
    case Grain: settings.effect.appearance.grain_percent = value; break;
    case EdgeRefraction: settings.effect.appearance.edge_refraction_percent = value; break;
    case EdgeWidth: settings.effect.appearance.edge_width_percent = value; break;
    case ChromaticAberration: settings.effect.appearance.chromatic_aberration_percent = value; break;
    case ElementZoom: settings.effect.appearance.indicator_refraction_percent = value; break;
    case ElementExtent: settings.effect.appearance.indicator_width_percent = value; break;
    case ElementSoftness: settings.effect.appearance.element_softness_percent = value; break;
    case CalmIntensity: settings.effect.calm_water.intensity_percent = value; break;
    case CalmSpeed: settings.effect.calm_water.speed_percent = value; break;
    case CalmWavelength: settings.effect.calm_water.wavelength_percent = value; break;
    case CalmNoise: settings.effect.calm_water.noise_percent = value; break;
    case LiquidIntensity: settings.effect.liquid.intensity_percent = value; break;
    case LiquidSpeed: settings.effect.liquid.speed_percent = value; break;
    case LiquidWavelength: settings.effect.liquid.wavelength_percent = value; break;
    case LiquidFluidity: settings.effect.liquid.fluidity_percent = value; break;
    case LiquidNoise: settings.effect.liquid.noise_percent = value; break;
    case RainIntensity: settings.effect.rain.intensity_percent = value; break;
    case RainSpeed: settings.effect.rain.speed_percent = value; break;
    case RainDensity: settings.effect.rain.density_percent = value; break;
    case RainRingSize: settings.effect.rain.ring_size_percent = value; break;
    case RainFade: settings.effect.rain.fade_percent = value; break;
    default: return false;
    }
    settings.effect.preset = GlassEffectPreset::Custom;
    settings.effect = NormalizeGlassEffectSettings(settings.effect);
    SelectWidgetMenuRadioItem(PresetCustom);
    return true;
}

// Mesure un composant owner-drawn du menu Glass.
bool MeasureDeckGlassMenuItem(MEASUREITEMSTRUCT* item) {
    return MeasureWidgetMenuSlider(item) || MeasureWidgetMenuCheckbox(item)
        || MeasureWidgetMenuRadio(item) || MeasureWidgetMenuToggleSubmenu(item)
        || MeasureOwnerMenuItem(item);
}

// Dessine un composant owner-drawn du menu Glass.
bool DrawDeckGlassMenuItem(DRAWITEMSTRUCT* item) {
    return DrawWidgetMenuSlider(item) || DrawWidgetMenuCheckbox(item)
        || DrawWidgetMenuRadio(item) || DrawWidgetMenuToggleSubmenu(item)
        || DrawOwnerMenuItem(item);
}

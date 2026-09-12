// ============================================================================
// Codex Glass - Orchestration du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier affiche le menu Win32 du widget et coordonne les modules de
// construction, de rendu owner-draw et de sliders.
// ============================================================================

#include "WidgetMenu.h"

#include "WidgetMenuBanner.h"
#include "WidgetMenuBuilder.h"
#include "WidgetMenuCheckbox.h"
#include "WidgetMenuOwnerDraw.h"
#include "WidgetMenuRadio.h"
#include "WidgetMenuSlider.h"
#include "WidgetMenuToggleSubmenu.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <cmath>
#include <vector>

namespace {

// Groupe exclusif des modes d'affichage.
constexpr UINT kCheckboxGroupDisplayMode = 1;

// Groupe exclusif des intervalles de rafraichissement.
constexpr UINT kCheckboxGroupRefreshInterval = 2;

// Groupe exclusif des presets GlassEffect.
constexpr UINT kCheckboxGroupGlassEffectPreset = 3;

// Groupe exclusif des animations GlassEffect.

// Groupe exclusif du nombre de pulsations.
constexpr UINT kCheckboxGroupMotionPulseRepetitions = 5;

// Groupe exclusif des directions de Wave.
constexpr UINT kCheckboxGroupMotionWaveDirection = 8;

// Groupe exclusif des langues d'interface.
constexpr UINT kCheckboxGroupLanguage = 10;

} // namespace

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les items owner-drawn du menu contextuel.
// ----------------------------------------------------------------------------
bool MeasureContextMenuItem(MEASUREITEMSTRUCT* measure_item) {
    if (MeasureMenuBanner(measure_item)) {
        return true;
    }
    if (MeasureWidgetMenuCheckbox(measure_item)) {
        return true;
    }
    if (MeasureWidgetMenuToggleSubmenu(measure_item)) {
        return true;
    }
    if (MeasureWidgetMenuRadio(measure_item)) {
        return true;
    }
    return MeasureOwnerMenuItem(measure_item);
}

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les items owner-drawn du menu contextuel.
// ----------------------------------------------------------------------------
bool DrawContextMenuItem(DRAWITEMSTRUCT* draw_item) {
    if (draw_item != nullptr
        && draw_item->CtlType == ODT_MENU
        && draw_item->itemID == kCommandMenuBanner) {
        DrawMenuBanner(draw_item);
        return true;
    }
    if (DrawWidgetMenuCheckbox(draw_item)) {
        return true;
    }
    if (DrawWidgetMenuToggleSubmenu(draw_item)) {
        return true;
    }
    if (DrawWidgetMenuRadio(draw_item)) {
        return true;
    }
    return DrawOwnerMenuItem(draw_item);
}

// ----------------------------------------------------------------------------
// Affiche le menu contextuel a une position ecran donnee.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire du menu.
// - point : position ecran ou afficher le menu.
// - state : etat courant necessaire a la construction du menu.
// - opened_from_tray : indique si le menu vient de l'icone de notification.
// ----------------------------------------------------------------------------
void ShowContextMenu(HWND hwnd, POINT point, const WidgetMenuState& state, bool opened_from_tray) {
    SetForegroundWindow(hwnd);
    ResetOwnerMenuDrawState();
    const WidgetMotionEffectsSettings motion = NormalizeWidgetMotionEffectsSettings(state.motion_effects);
    const GlassEffectSettings glass_effect = NormalizeGlassEffectSettings(state.glass_effect);
    const std::vector<WidgetMenuCheckboxSpec> checkbox_specs{
        WidgetMenuCheckboxSpec{
            kCommandToggleAlwaysOnTop,
            T(IDS_MENU_ALWAYS_ON_TOP),
            T(IDS_MENU_ALWAYS_ON_TOP),
            state.always_on_top
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleLockPosition,
            T(IDS_MENU_LOCK_POSITION),
            T(IDS_MENU_LOCK_POSITION),
            state.lock_position
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleDockToEdges,
            T(IDS_MENU_DOCK_TO_EDGES),
            T(IDS_MENU_DOCK_TO_EDGES),
            state.dock_to_screen_edges
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleClickThrough,
            T(IDS_MENU_CLICK_THROUGH),
            T(IDS_MENU_CLICK_THROUGH),
            state.click_through
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleHideWhenFullscreen,
            T(IDS_MENU_HIDE_FULLSCREEN),
            T(IDS_MENU_HIDE_FULLSCREEN),
            state.hide_when_fullscreen
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleStartWithWindows,
            T(IDS_MENU_START_WITH_WINDOWS),
            T(IDS_MENU_START_WITH_WINDOWS),
            state.start_with_windows
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleQuotaFiveHour,
            T(IDS_MENU_QUOTA_5H),
            T(IDS_MENU_QUOTA_5H),
            state.quota_visibility.five_hour
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleQuotaWeekly,
            T(IDS_MENU_QUOTA_WEEKLY),
            T(IDS_MENU_QUOTA_WEEKLY),
            state.quota_visibility.weekly
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleQuotaSparkFiveHour,
            T(IDS_MENU_QUOTA_SPARK_5H),
            T(IDS_MENU_QUOTA_SPARK_5H),
            state.quota_visibility.spark_five_hour
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleQuotaSparkWeekly,
            T(IDS_MENU_QUOTA_SPARK_WEEKLY),
            T(IDS_MENU_QUOTA_SPARK_WEEKLY),
            state.quota_visibility.spark_weekly
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleVibrationTestAuto,
            T(IDS_VIBRATION_TEST_AUTO),
            T(IDS_VIBRATION_TEST_AUTO),
            state.vibration_test_auto_enabled
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleRollingNumberTest,
            T(IDS_ROLLING_NUMBER_TEST_DECREMENT),
            T(IDS_ROLLING_NUMBER_TEST_DECREMENT),
            state.rolling_number_test_enabled
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionVibrationHorizontal,
            T(IDS_MOTION_HORIZONTAL),
            T(IDS_MOTION_HORIZONTAL),
            motion.vibration.horizontal_enabled
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionVibrationVertical,
            T(IDS_MOTION_VERTICAL),
            T(IDS_MOTION_VERTICAL),
            motion.vibration.vertical_enabled
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionTriggerUsageDrop,
            T(IDS_MOTION_TRIGGER_USAGE_DROP),
            T(IDS_MOTION_TRIGGER_USAGE_DROP),
            motion.trigger_on_usage_drop
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionTriggerQuotaReset,
            T(IDS_MOTION_TRIGGER_QUOTA_RESET),
            T(IDS_MOTION_TRIGGER_QUOTA_RESET),
            motion.trigger_on_quota_reset
        },
        WidgetMenuCheckboxSpec{
            kCommandRefreshManual,
            T(IDS_REFRESH_MANUAL),
            T(IDS_REFRESH_MANUAL),
            state.usage_refresh_interval == UsageRefreshInterval::Manual,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupRefreshInterval
        },
        WidgetMenuCheckboxSpec{
            kCommandRefresh10Seconds,
            T(IDS_REFRESH_10_SECONDS),
            T(IDS_REFRESH_10_SECONDS),
            state.usage_refresh_interval == UsageRefreshInterval::Seconds10,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupRefreshInterval
        },
        WidgetMenuCheckboxSpec{
            kCommandRefresh30Seconds,
            T(IDS_REFRESH_30_SECONDS),
            T(IDS_REFRESH_30_SECONDS),
            state.usage_refresh_interval == UsageRefreshInterval::Seconds30,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupRefreshInterval
        },
        WidgetMenuCheckboxSpec{
            kCommandRefresh1Minute,
            T(IDS_REFRESH_1_MINUTE),
            T(IDS_REFRESH_1_MINUTE),
            state.usage_refresh_interval == UsageRefreshInterval::Minute1,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupRefreshInterval
        },
        WidgetMenuCheckboxSpec{
            kCommandRefresh5Minutes,
            T(IDS_REFRESH_5_MINUTES),
            T(IDS_REFRESH_5_MINUTES),
            state.usage_refresh_interval == UsageRefreshInterval::Minutes5,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupRefreshInterval
        },
        WidgetMenuCheckboxSpec{
            kCommandRefresh15Minutes,
            T(IDS_REFRESH_15_MINUTES),
            T(IDS_REFRESH_15_MINUTES),
            state.usage_refresh_interval == UsageRefreshInterval::Minutes15,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupRefreshInterval
        },
        WidgetMenuCheckboxSpec{
            kCommandDisplayMinimal,
            T(IDS_DISPLAY_MINIMAL),
            T(IDS_DISPLAY_MINIMAL),
            state.display_mode == WidgetDisplayMode::Minimal,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupDisplayMode
        },
        WidgetMenuCheckboxSpec{
            kCommandDisplayCompact,
            T(IDS_DISPLAY_COMPACT),
            T(IDS_DISPLAY_COMPACT),
            state.display_mode == WidgetDisplayMode::Compact,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupDisplayMode
        },
        WidgetMenuCheckboxSpec{
            kCommandDisplayComplete,
            T(IDS_DISPLAY_COMPLETE),
            T(IDS_DISPLAY_COMPLETE),
            state.display_mode == WidgetDisplayMode::Complete,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupDisplayMode
        },
        WidgetMenuCheckboxSpec{
            kCommandDisplayHorizontal,
            T(IDS_DISPLAY_HORIZONTAL),
            T(IDS_DISPLAY_HORIZONTAL),
            state.display_mode == WidgetDisplayMode::Horizontal,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupDisplayMode
        },
        WidgetMenuCheckboxSpec{
            kCommandDisplayVertical,
            T(IDS_DISPLAY_VERTICAL),
            T(IDS_DISPLAY_VERTICAL),
            state.display_mode == WidgetDisplayMode::Vertical,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupDisplayMode
        },
        WidgetMenuCheckboxSpec{
            kCommandGlassEffectPresetClear,
            T(IDS_GLASS_EFFECT_PRESET_CLEAR),
            T(IDS_GLASS_EFFECT_PRESET_CLEAR),
            glass_effect.preset == GlassEffectPreset::Clear,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupGlassEffectPreset
        },
        WidgetMenuCheckboxSpec{
            kCommandGlassEffectPresetFrosted,
            T(IDS_GLASS_EFFECT_PRESET_FROSTED),
            T(IDS_GLASS_EFFECT_PRESET_FROSTED),
            glass_effect.preset == GlassEffectPreset::Frosted,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupGlassEffectPreset
        },
        WidgetMenuCheckboxSpec{
            kCommandGlassEffectPresetSubtle,
            T(IDS_GLASS_EFFECT_PRESET_SUBTLE),
            T(IDS_GLASS_EFFECT_PRESET_SUBTLE),
            glass_effect.preset == GlassEffectPreset::Subtle,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupGlassEffectPreset
        },
        WidgetMenuCheckboxSpec{
            kCommandGlassEffectPresetStrong,
            T(IDS_GLASS_EFFECT_PRESET_STRONG),
            T(IDS_GLASS_EFFECT_PRESET_STRONG),
            glass_effect.preset == GlassEffectPreset::Strong,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupGlassEffectPreset
        },
        WidgetMenuCheckboxSpec{
            kCommandGlassEffectPresetCustom,
            T(IDS_GLASS_EFFECT_PRESET_CUSTOM),
            T(IDS_GLASS_EFFECT_PRESET_CUSTOM),
            glass_effect.preset == GlassEffectPreset::Custom,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupGlassEffectPreset
        },
        WidgetMenuCheckboxSpec{
            kCommandToggleGlassElementLens,
            T(IDS_GLASS_APPEARANCE_ELEMENT_GLASS),
            T(IDS_GLASS_APPEARANCE_ELEMENT_GLASS),
            glass_effect.appearance.element_glass_enabled
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionPulseRepetitions1,
            T(IDS_MOTION_REPETITIONS_1),
            T(IDS_MOTION_REPETITIONS_1),
            motion.pulse.repetitions == WidgetMotionPulseRepetitions::One,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionPulseRepetitions
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionPulseRepetitions2,
            T(IDS_MOTION_REPETITIONS_2),
            T(IDS_MOTION_REPETITIONS_2),
            motion.pulse.repetitions == WidgetMotionPulseRepetitions::Two,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionPulseRepetitions
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionPulseRepetitions3,
            T(IDS_MOTION_REPETITIONS_3),
            T(IDS_MOTION_REPETITIONS_3),
            motion.pulse.repetitions == WidgetMotionPulseRepetitions::Three,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionPulseRepetitions
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionWaveDirectionLeft,
            T(IDS_MOTION_DIRECTION_LEFT),
            T(IDS_MOTION_DIRECTION_LEFT),
            motion.wave.direction == WidgetMotionWaveDirection::Left,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionWaveDirection
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionWaveDirectionRight,
            T(IDS_MOTION_DIRECTION_RIGHT),
            T(IDS_MOTION_DIRECTION_RIGHT),
            motion.wave.direction == WidgetMotionWaveDirection::Right,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionWaveDirection
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionWaveDirectionUp,
            T(IDS_MOTION_DIRECTION_UP),
            T(IDS_MOTION_DIRECTION_UP),
            motion.wave.direction == WidgetMotionWaveDirection::Up,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionWaveDirection
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionWaveDirectionDown,
            T(IDS_MOTION_DIRECTION_DOWN),
            T(IDS_MOTION_DIRECTION_DOWN),
            motion.wave.direction == WidgetMotionWaveDirection::Down,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionWaveDirection
        },
        WidgetMenuCheckboxSpec{
            kCommandMotionWaveDirectionRadial,
            T(IDS_MOTION_DIRECTION_RADIAL),
            T(IDS_MOTION_DIRECTION_RADIAL),
            motion.wave.direction == WidgetMotionWaveDirection::Radial,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupMotionWaveDirection
        },
        WidgetMenuCheckboxSpec{
            kCommandLanguageAuto,
            T(IDS_LANGUAGE_AUTO),
            T(IDS_LANGUAGE_AUTO),
            state.language == UiLanguage::Auto,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupLanguage
        },
        WidgetMenuCheckboxSpec{
            kCommandLanguageFrench,
            T(IDS_LANGUAGE_FRENCH),
            T(IDS_LANGUAGE_FRENCH),
            state.language == UiLanguage::French,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupLanguage
        },
        WidgetMenuCheckboxSpec{
            kCommandLanguageEnglish,
            T(IDS_LANGUAGE_ENGLISH),
            T(IDS_LANGUAGE_ENGLISH),
            state.language == UiLanguage::English,
            WidgetMenuCheckboxMode::Select,
            kCheckboxGroupLanguage
        }
    };
    const std::vector<WidgetMenuSliderSpec> slider_specs{
        WidgetMenuSliderSpec{
            kCommandOpacitySlider,
            IDS_MENU_OPACITY,
            kOpacityMenuSliderChangedMessage,
            20,
            100,
            static_cast<int>(std::lround(state.background_opacity * 100.0))
        },
        WidgetMenuSliderSpec{
            kCommandMotionThresholdSlider,
            IDS_VIBRATION_THRESHOLD,
            kMotionEffectMenuSliderChangedMessage,
            1,
            50,
            motion.usage_drop_threshold_percent,
            L" %"
        },
        WidgetMenuSliderSpec{
            kCommandMotionCooldownSlider,
            IDS_VIBRATION_COOLDOWN,
            kMotionEffectMenuSliderChangedMessage,
            5,
            3600,
            motion.minimum_interval_seconds,
            L" s"
        },
        WidgetMenuSliderSpec{
            kCommandMotionVibrationIntensitySlider,
            IDS_VIBRATION_INTENSITY,
            kMotionEffectMenuSliderChangedMessage,
            25,
            300,
            motion.vibration.intensity_percent,
            L" %"
        },
        WidgetMenuSliderSpec{
            kCommandMotionVibrationDurationSlider,
            IDS_VIBRATION_DURATION,
            kMotionEffectMenuSliderChangedMessage,
            100,
            2000,
            motion.vibration.duration_ms,
            L" ms"
        },
        WidgetMenuSliderSpec{kCommandMotionVibrationFrequencySlider, IDS_MOTION_FREQUENCY,
            kMotionEffectMenuSliderChangedMessage, 25, 300, motion.vibration.frequency_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionVibrationRefractionSlider, IDS_MOTION_REFRACTION,
            kMotionEffectMenuSliderChangedMessage, 0, 300, motion.vibration.refraction_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionVibrationDampingSlider, IDS_MOTION_DAMPING,
            kMotionEffectMenuSliderChangedMessage, 0, 100, motion.vibration.damping_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionPulseIntensitySlider, IDS_VIBRATION_INTENSITY,
            kMotionEffectMenuSliderChangedMessage, 25, 300, motion.pulse.intensity_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionPulseDurationSlider, IDS_VIBRATION_DURATION,
            kMotionEffectMenuSliderChangedMessage, 100, 3000, motion.pulse.duration_ms, L" ms"},
        WidgetMenuSliderSpec{kCommandMotionPulseSoftnessSlider, IDS_MOTION_SOFTNESS,
            kMotionEffectMenuSliderChangedMessage, 0, 100, motion.pulse.softness_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionPulseExtentSlider, IDS_MOTION_EXTENT,
            kMotionEffectMenuSliderChangedMessage, 0, 100, motion.pulse.extent_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionWaveIntensitySlider, IDS_VIBRATION_INTENSITY,
            kMotionEffectMenuSliderChangedMessage, 25, 300, motion.wave.intensity_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionWaveDurationSlider, IDS_VIBRATION_DURATION,
            kMotionEffectMenuSliderChangedMessage, 200, 5000, motion.wave.duration_ms, L" ms"},
        WidgetMenuSliderSpec{kCommandMotionWaveSpeedSlider, IDS_MOTION_SPEED,
            kMotionEffectMenuSliderChangedMessage, 25, 300, motion.wave.speed_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionWaveWavelengthSlider, IDS_MOTION_WAVELENGTH,
            kMotionEffectMenuSliderChangedMessage, 25, 300, motion.wave.wavelength_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionWaveCountSlider, IDS_MOTION_WAVE_COUNT,
            kMotionEffectMenuSliderChangedMessage, 1, 6, motion.wave.wave_count, L""},
        WidgetMenuSliderSpec{kCommandMotionWaveDampingSlider, IDS_MOTION_DAMPING,
            kMotionEffectMenuSliderChangedMessage, 0, 100, motion.wave.damping_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionWaveRefractionSlider, IDS_MOTION_REFRACTION,
            kMotionEffectMenuSliderChangedMessage, 0, 300, motion.wave.refraction_percent, L" %"},
        WidgetMenuSliderSpec{kCommandMotionWaveFrontUndulationSlider, IDS_MOTION_WAVE_FRONT_UNDULATION,
            kMotionEffectMenuSliderChangedMessage, 0, 100, motion.wave.front_undulation_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceDiffusionSlider, IDS_GLASS_APPEARANCE_DIFFUSION,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.appearance.diffusion_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceTintSlider, IDS_GLASS_APPEARANCE_TINT,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.appearance.tint_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceGrainSlider, IDS_GLASS_APPEARANCE_GRAIN,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.appearance.grain_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceEdgeRefractionSlider, IDS_GLASS_APPEARANCE_EDGE_REFRACTION,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.appearance.edge_refraction_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceEdgeWidthSlider, IDS_GLASS_APPEARANCE_EDGE_WIDTH,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.appearance.edge_width_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceChromaticSlider, IDS_GLASS_APPEARANCE_CHROMATIC,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.appearance.chromatic_aberration_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceIndicatorRefractionSlider, IDS_GLASS_APPEARANCE_INDICATOR_REFRACTION,
            kGlassEffectMenuSliderChangedMessage, -200, 200, glass_effect.appearance.indicator_refraction_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceIndicatorWidthSlider, IDS_GLASS_APPEARANCE_INDICATOR_WIDTH,
            kGlassEffectMenuSliderChangedMessage, 25, 250, glass_effect.appearance.indicator_width_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassAppearanceElementSoftnessSlider, IDS_GLASS_APPEARANCE_ELEMENT_SOFTNESS,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.appearance.element_softness_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassCalmIntensitySlider, IDS_GLASS_EFFECT_ANIMATION_INTENSITY,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.calm_water.intensity_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassCalmSpeedSlider, IDS_GLASS_EFFECT_ANIMATION_SPEED,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.calm_water.speed_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassCalmWavelengthSlider, IDS_MOTION_WAVELENGTH,
            kGlassEffectMenuSliderChangedMessage, 25, 300, glass_effect.calm_water.wavelength_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassCalmNoiseSlider, IDS_GLASS_ANIMATION_NOISE,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.calm_water.noise_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassLiquidIntensitySlider, IDS_GLASS_EFFECT_ANIMATION_INTENSITY,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.liquid.intensity_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassLiquidSpeedSlider, IDS_GLASS_EFFECT_ANIMATION_SPEED,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.liquid.speed_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassLiquidWavelengthSlider, IDS_MOTION_WAVELENGTH,
            kGlassEffectMenuSliderChangedMessage, 25, 300, glass_effect.liquid.wavelength_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassLiquidFluiditySlider, IDS_GLASS_LIQUID_FLUIDITY,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.liquid.fluidity_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassLiquidNoiseSlider, IDS_GLASS_ANIMATION_NOISE,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.liquid.noise_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassRainIntensitySlider, IDS_GLASS_EFFECT_ANIMATION_INTENSITY,
            kGlassEffectMenuSliderChangedMessage, 0, 200, glass_effect.rain.intensity_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassRainSpeedSlider, IDS_GLASS_EFFECT_ANIMATION_SPEED,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.rain.speed_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassRainDensitySlider, IDS_GLASS_EFFECT_RAIN_DENSITY,
            kGlassEffectMenuSliderChangedMessage, 25, 250, glass_effect.rain.density_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassRainRingSizeSlider, IDS_GLASS_EFFECT_RAIN_RING_SIZE,
            kGlassEffectMenuSliderChangedMessage, 50, 200, glass_effect.rain.ring_size_percent, L" %"},
        WidgetMenuSliderSpec{kCommandGlassRainFadeSlider, IDS_GLASS_RAIN_FADE,
            kGlassEffectMenuSliderChangedMessage, 25, 200, glass_effect.rain.fade_percent, L" %"}
    };

    std::vector<WidgetMenuRadioSpec> radio_specs;
    radio_specs.reserve(checkbox_specs.size());
    for (const WidgetMenuCheckboxSpec& spec : checkbox_specs) {
        if (spec.mode == WidgetMenuCheckboxMode::Select) {
            radio_specs.push_back(WidgetMenuRadioSpec{
                spec.command_id,
                spec.checked_text,
                spec.checked,
                spec.group_id,
            });
        }
    }

    const std::vector<WidgetMenuToggleSubmenuSpec> toggle_submenu_specs{
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleGlassEffect,
            T(IDS_MENU_GLASS_EFFECT),
            T(IDS_MENU_GLASS_EFFECT),
            state.glass_effect_mode == GlassEffectMode::Enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleGlassCalmWater,
            T(IDS_GLASS_EFFECT_ANIMATION_CALM_WATER),
            T(IDS_GLASS_EFFECT_ANIMATION_CALM_WATER),
            glass_effect.calm_water.enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleGlassLiquid,
            T(IDS_GLASS_EFFECT_ANIMATION_LIQUID),
            T(IDS_GLASS_EFFECT_ANIMATION_LIQUID),
            glass_effect.liquid.enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleGlassRain,
            T(IDS_GLASS_EFFECT_ANIMATION_RAIN),
            T(IDS_GLASS_EFFECT_ANIMATION_RAIN),
            glass_effect.rain.enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleVibration,
            T(IDS_MENU_VIBRATION),
            T(IDS_MENU_VIBRATION),
            motion.enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleMotionVibration,
            T(IDS_MOTION_VIBRATION),
            T(IDS_MOTION_VIBRATION),
            motion.vibration.enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleMotionPulse,
            T(IDS_MOTION_PULSE),
            T(IDS_MOTION_PULSE),
            motion.pulse.enabled
        },
        WidgetMenuToggleSubmenuSpec{
            kCommandToggleMotionWave,
            T(IDS_MOTION_WAVE),
            T(IDS_MOTION_WAVE),
            motion.wave.enabled
        },
    };
    BeginWidgetMenuCheckboxTracking(hwnd, checkbox_specs, state.active_control_color);
    BeginWidgetMenuToggleSubmenuTracking(hwnd, toggle_submenu_specs, state.active_control_color);
    BeginWidgetMenuRadioTracking(hwnd, radio_specs, state.active_control_color);
    BeginWidgetMenuSliderTracking(hwnd, slider_specs, state.active_control_color);
    BeginMenuBannerCursorTracking();
    BeginOwnerMenuWindowPolish();
    HMENU menu = CreateContextMenu(state);

    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hwnd, nullptr);
    if (opened_from_tray) {
        PostMessageW(hwnd, WM_NULL, 0, 0);
    }

    EndOwnerMenuWindowPolish();
    EndMenuBannerCursorTracking();
    EndWidgetMenuSliderTracking();
    EndWidgetMenuRadioTracking();
    EndWidgetMenuToggleSubmenuTracking();
    EndWidgetMenuCheckboxTracking();
    DestroyMenu(menu);
    ResetOwnerMenuDrawState();
}

// ----------------------------------------------------------------------------
// Actualise les libelles du menu contextuel actuellement affiche.
//
// Parametres :
// - previous_language : langue effective des libelles encore en memoire.
//
// Effet de bord : remplace et redessine les textes sans fermer TrackPopupMenu.
// ----------------------------------------------------------------------------
void RefreshContextMenuLocalization(UiLanguage previous_language) {
    RelocalizeWidgetMenuCheckboxes(previous_language);
    RelocalizeWidgetMenuRadios(previous_language);
    RelocalizeWidgetMenuToggleSubmenus(previous_language);
    RelocalizeOwnerMenuItems(previous_language);
}

// ----------------------------------------------------------------------------
// Resynchronise l'apparence Glass dans un menu contextuel encore ouvert.
// ----------------------------------------------------------------------------
void RefreshGlassEffectAppearanceMenuState(const GlassEffectSettings& source) {
    const GlassEffectSettings settings = NormalizeGlassEffectSettings(source);
    UINT preset_command = kCommandGlassEffectPresetCustom;
    switch (settings.preset) {
    case GlassEffectPreset::Clear: preset_command = kCommandGlassEffectPresetClear; break;
    case GlassEffectPreset::Frosted: preset_command = kCommandGlassEffectPresetFrosted; break;
    case GlassEffectPreset::Subtle: preset_command = kCommandGlassEffectPresetSubtle; break;
    case GlassEffectPreset::Strong: preset_command = kCommandGlassEffectPresetStrong; break;
    case GlassEffectPreset::Custom: break;
    }

    SelectWidgetMenuRadioItem(preset_command);
    UpdateWidgetMenuCheckboxChecked(
        kCommandToggleGlassElementLens,
        settings.appearance.element_glass_enabled
    );
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceDiffusionSlider,
        settings.appearance.diffusion_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceTintSlider,
        settings.appearance.tint_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceGrainSlider,
        settings.appearance.grain_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceEdgeRefractionSlider,
        settings.appearance.edge_refraction_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceEdgeWidthSlider,
        settings.appearance.edge_width_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceChromaticSlider,
        settings.appearance.chromatic_aberration_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceIndicatorRefractionSlider,
        settings.appearance.indicator_refraction_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceIndicatorWidthSlider,
        settings.appearance.indicator_width_percent);
    UpdateWidgetMenuSliderValue(kCommandGlassAppearanceElementSoftnessSlider,
        settings.appearance.element_softness_percent);
}

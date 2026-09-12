// ============================================================================
// Codex Glass - Construction du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier assemble les entrees metier du menu. Il s'appuie sur les helpers
// owner-draw sans connaitre les details de peinture ni de suivi souris.
// ============================================================================

#include "WidgetMenuBuilder.h"

#include "WidgetMenuCheckbox.h"
#include "WidgetMenuOwnerDraw.h"
#include "WidgetMenuRadio.h"
#include "WidgetMenuSlider.h"
#include "WidgetMenuToggleSubmenu.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

namespace {

// Identifiant du preset interne alimente par le panneau de personnalisation.
constexpr wchar_t kCustomColorPresetId[] = L"custom";

HMENU CreateGlassEffectPresetMenu(const WidgetMenuState& state);
// ----------------------------------------------------------------------------
// Cree le sous-menu des intervalles de rafraichissement.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateRefreshIntervalMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuRadioItem(menu, kCommandRefreshManual);
    AppendWidgetMenuRadioItem(menu, kCommandRefresh10Seconds);
    AppendWidgetMenuRadioItem(menu, kCommandRefresh30Seconds);
    AppendWidgetMenuRadioItem(menu, kCommandRefresh1Minute);
    AppendWidgetMenuRadioItem(menu, kCommandRefresh5Minutes);
    AppendWidgetMenuRadioItem(menu, kCommandRefresh15Minutes);
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu des modes d'affichage.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateDisplayModeMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuRadioItem(menu, kCommandDisplayMinimal);
    AppendWidgetMenuRadioItem(menu, kCommandDisplayCompact);
    AppendWidgetMenuRadioItem(menu, kCommandDisplayComplete);
    AppendWidgetMenuRadioItem(menu, kCommandDisplayHorizontal);
    AppendWidgetMenuRadioItem(menu, kCommandDisplayVertical);
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu de visibilite des quatre lignes de quota.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateQuotaVisibilityMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleQuotaFiveHour);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleQuotaWeekly);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleQuotaSparkFiveHour);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleQuotaSparkWeekly);
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu des modes d'effet glass.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateGlassEffectMenu(const WidgetMenuState& state) {
    HMENU menu = CreateOwnerPopupMenu();
    HMENU appearance_menu = CreateOwnerPopupMenu();
    AppendSubMenu(
        appearance_menu,
        CreateGlassEffectPresetMenu(state),
        T(IDS_GLASS_APPEARANCE_PRESET).c_str()
    );
    AppendMenuSeparator(appearance_menu);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandOpacitySlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceDiffusionSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceTintSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceGrainSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceEdgeRefractionSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceEdgeWidthSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceChromaticSlider);
    AppendMenuSeparator(appearance_menu);
    AppendWidgetMenuCheckboxItem(appearance_menu, kCommandToggleGlassElementLens);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceIndicatorRefractionSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceIndicatorWidthSlider);
    AppendWidgetMenuSliderItem(appearance_menu, kCommandGlassAppearanceElementSoftnessSlider);
    AppendMenuSeparator(appearance_menu);
    AppendCommandMenuItem(appearance_menu, kCommandRandomizeGlassAppearance, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(
        appearance_menu,
        kCommandResetGlassAppearance,
        T(IDS_GLASS_RESET_APPEARANCE).c_str()
    );
    AppendSubMenu(menu, appearance_menu, T(IDS_GLASS_APPEARANCE).c_str());

    HMENU calm_menu = CreateOwnerPopupMenu();
    AppendWidgetMenuSliderItem(calm_menu, kCommandGlassCalmIntensitySlider);
    AppendWidgetMenuSliderItem(calm_menu, kCommandGlassCalmSpeedSlider);
    AppendWidgetMenuSliderItem(calm_menu, kCommandGlassCalmWavelengthSlider);
    AppendWidgetMenuSliderItem(calm_menu, kCommandGlassCalmNoiseSlider);
    AppendMenuSeparator(calm_menu);
    AppendCommandMenuItem(calm_menu, kCommandRandomizeGlassCalmWater, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(calm_menu, kCommandResetGlassCalmWater, T(IDS_GLASS_RESET_CALM_WATER).c_str());
    AppendWidgetMenuToggleSubmenuItem(menu, calm_menu, kCommandToggleGlassCalmWater);

    HMENU liquid_menu = CreateOwnerPopupMenu();
    AppendWidgetMenuSliderItem(liquid_menu, kCommandGlassLiquidIntensitySlider);
    AppendWidgetMenuSliderItem(liquid_menu, kCommandGlassLiquidSpeedSlider);
    AppendWidgetMenuSliderItem(liquid_menu, kCommandGlassLiquidWavelengthSlider);
    AppendWidgetMenuSliderItem(liquid_menu, kCommandGlassLiquidFluiditySlider);
    AppendWidgetMenuSliderItem(liquid_menu, kCommandGlassLiquidNoiseSlider);
    AppendMenuSeparator(liquid_menu);
    AppendCommandMenuItem(liquid_menu, kCommandRandomizeGlassLiquid, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(liquid_menu, kCommandResetGlassLiquid, T(IDS_GLASS_RESET_LIQUID).c_str());
    AppendWidgetMenuToggleSubmenuItem(menu, liquid_menu, kCommandToggleGlassLiquid);

    HMENU rain_menu = CreateOwnerPopupMenu();
    AppendWidgetMenuSliderItem(rain_menu, kCommandGlassRainIntensitySlider);
    AppendWidgetMenuSliderItem(rain_menu, kCommandGlassRainSpeedSlider);
    AppendWidgetMenuSliderItem(rain_menu, kCommandGlassRainDensitySlider);
    AppendWidgetMenuSliderItem(rain_menu, kCommandGlassRainRingSizeSlider);
    AppendWidgetMenuSliderItem(rain_menu, kCommandGlassRainFadeSlider);
    AppendMenuSeparator(rain_menu);
    AppendCommandMenuItem(rain_menu, kCommandRandomizeGlassRain, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(rain_menu, kCommandResetGlassRain, T(IDS_GLASS_RESET_RAIN).c_str());
    AppendWidgetMenuToggleSubmenuItem(menu, rain_menu, kCommandToggleGlassRain);

    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu des presets GlassEffect.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateGlassEffectPresetMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuRadioItem(menu, kCommandGlassEffectPresetClear);
    AppendWidgetMenuRadioItem(menu, kCommandGlassEffectPresetFrosted);
    AppendWidgetMenuRadioItem(menu, kCommandGlassEffectPresetSubtle);
    AppendWidgetMenuRadioItem(menu, kCommandGlassEffectPresetStrong);
    AppendWidgetMenuRadioItem(menu, kCommandGlassEffectPresetCustom);
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu de vibration du widget.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateMotionVibrationMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuSliderItem(menu, kCommandMotionVibrationIntensitySlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionVibrationDurationSlider);
    AppendMenuSeparator(menu);
    AppendWidgetMenuCheckboxItem(menu, kCommandMotionVibrationHorizontal);
    AppendWidgetMenuCheckboxItem(menu, kCommandMotionVibrationVertical);
    AppendMenuSeparator(menu);
    AppendWidgetMenuSliderItem(menu, kCommandMotionVibrationFrequencySlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionVibrationRefractionSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionVibrationDampingSlider);
    AppendMenuSeparator(menu);
    AppendCommandMenuItem(menu, kCommandRandomizeMotionVibration, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(menu, kCommandResetMotionVibration, T(IDS_MOTION_RESET).c_str());
    return menu;
}

// ----------------------------------------------------------------------------
// Cree les reglages propres a Pulse.
// ----------------------------------------------------------------------------
HMENU CreateMotionPulseMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuSliderItem(menu, kCommandMotionPulseIntensitySlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionPulseDurationSlider);
    AppendMenuSeparator(menu);
    HMENU repetitions_menu = CreateOwnerPopupMenu();
    AppendWidgetMenuRadioItem(repetitions_menu, kCommandMotionPulseRepetitions1);
    AppendWidgetMenuRadioItem(repetitions_menu, kCommandMotionPulseRepetitions2);
    AppendWidgetMenuRadioItem(repetitions_menu, kCommandMotionPulseRepetitions3);
    AppendSubMenu(menu, repetitions_menu, T(IDS_MOTION_REPETITIONS).c_str());
    AppendWidgetMenuSliderItem(menu, kCommandMotionPulseSoftnessSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionPulseExtentSlider);
    AppendMenuSeparator(menu);
    AppendCommandMenuItem(menu, kCommandRandomizeMotionPulse, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(menu, kCommandResetMotionPulse, T(IDS_MOTION_RESET).c_str());
    return menu;
}

// ----------------------------------------------------------------------------
// Cree les reglages propres a Wave.
// ----------------------------------------------------------------------------
HMENU CreateMotionWaveMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveIntensitySlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveDurationSlider);
    AppendMenuSeparator(menu);
    HMENU direction_menu = CreateOwnerPopupMenu();
    AppendWidgetMenuRadioItem(direction_menu, kCommandMotionWaveDirectionLeft);
    AppendWidgetMenuRadioItem(direction_menu, kCommandMotionWaveDirectionRight);
    AppendWidgetMenuRadioItem(direction_menu, kCommandMotionWaveDirectionUp);
    AppendWidgetMenuRadioItem(direction_menu, kCommandMotionWaveDirectionDown);
    AppendWidgetMenuRadioItem(direction_menu, kCommandMotionWaveDirectionRadial);
    AppendSubMenu(menu, direction_menu, T(IDS_MOTION_DIRECTION).c_str());
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveSpeedSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveWavelengthSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveCountSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveDampingSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveRefractionSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionWaveFrontUndulationSlider);
    AppendMenuSeparator(menu);
    AppendCommandMenuItem(menu, kCommandRandomizeMotionWave, T(IDS_SETTINGS_RANDOMIZE).c_str());
    AppendCommandMenuItem(menu, kCommandResetMotionWave, T(IDS_MOTION_RESET).c_str());
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu global des Effets Motion.
// ----------------------------------------------------------------------------
HMENU CreateVibrationMenu(const WidgetMenuState& state) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuToggleSubmenuItem(
        menu,
        CreateMotionVibrationMenu(state),
        kCommandToggleMotionVibration
    );
    AppendWidgetMenuToggleSubmenuItem(
        menu,
        CreateMotionPulseMenu(state),
        kCommandToggleMotionPulse
    );
    AppendWidgetMenuToggleSubmenuItem(
        menu,
        CreateMotionWaveMenu(state),
        kCommandToggleMotionWave
    );
    AppendMenuSeparator(menu);
    AppendWidgetMenuCheckboxItem(menu, kCommandMotionTriggerUsageDrop);
    AppendWidgetMenuCheckboxItem(menu, kCommandMotionTriggerQuotaReset);
    AppendMenuSeparator(menu);
    if (state.motion_effects.debug_menu_enabled) {
        AppendWidgetMenuCheckboxItem(menu, kCommandToggleVibrationTestAuto);
        AppendWidgetMenuCheckboxItem(menu, kCommandToggleRollingNumberTest);
        AppendMenuSeparator(menu);
    }
    AppendWidgetMenuSliderItem(menu, kCommandMotionThresholdSlider);
    AppendWidgetMenuSliderItem(menu, kCommandMotionCooldownSlider);
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu des presets de couleurs.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateColorPresetMenu(const WidgetMenuState& state) {
    HMENU menu = CreateOwnerPopupMenu();
    if (state.color_presets == nullptr) {
        return menu;
    }

    for (size_t index = 0; index < state.color_presets->size(); ++index) {
        const WidgetColorPreset& preset = (*state.color_presets)[index];
        if (preset.id == kCustomColorPresetId) {
            continue;
        }
        AppendCommandMenuItem(
            menu,
            kCommandColorPresetBase + static_cast<UINT>(index),
            preset.name.c_str()
        );
    }
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu global de couleurs.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateColorsMenu(const WidgetMenuState& state) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendSubMenu(menu, CreateColorPresetMenu(state), T(IDS_COLOR_PRESETS).c_str());
    AppendCommandMenuItem(
        menu,
        kCommandColorCustomize,
        state.color_panel_open ? T(IDS_COLOR_PANEL_HIDE).c_str() : T(IDS_COLOR_PANEL_SHOW).c_str()
    );
    AppendMenuSeparator(menu);
    AppendCommandMenuItem(menu, kCommandColorReset, T(IDS_COLOR_RESET).c_str());
    return menu;
}

// ----------------------------------------------------------------------------
// Cree le sous-menu des langues d'interface.
//
// Parametres :
// - state : etat courant du menu.
//
// Retour :
// - handle du sous-menu cree.
// ----------------------------------------------------------------------------
HMENU CreateLanguageMenu(const WidgetMenuState&) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendWidgetMenuRadioItem(menu, kCommandLanguageAuto);
    AppendWidgetMenuRadioItem(menu, kCommandLanguageFrench);
    AppendWidgetMenuRadioItem(menu, kCommandLanguageEnglish);
    return menu;
}


} // namespace

// ----------------------------------------------------------------------------
// Cree le menu contextuel principal du widget et du tray.
//
// Parametres :
// - state : etat courant necessaire aux coches et libelles.
//
// Retour :
// - handle du menu cree.
// ----------------------------------------------------------------------------
HMENU CreateContextMenu(const WidgetMenuState& state) {
    HMENU menu = CreateOwnerPopupMenu();
    AppendCommandMenuItem(menu, kCommandMenuBanner, T(IDS_APP_TITLE).c_str());
    AppendCommandMenuItem(
        menu,
        kCommandToggleVisible,
        state.visible ? T(IDS_MENU_HIDE).c_str() : T(IDS_MENU_SHOW).c_str()
    );
    AppendCommandMenuItem(menu, kCommandRefreshNow, T(IDS_MENU_REFRESH_NOW).c_str());
    AppendMenuSeparator(menu);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleAlwaysOnTop);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleLockPosition);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleDockToEdges);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleClickThrough);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleHideWhenFullscreen);
    AppendWidgetMenuCheckboxItem(menu, kCommandToggleStartWithWindows);
    AppendMenuSeparator(menu);
    AppendSubMenu(menu, CreateDisplayModeMenu(state), T(IDS_MENU_DISPLAY_MODE).c_str());
    AppendSubMenu(
        menu,
        CreateQuotaVisibilityMenu(state),
        T(IDS_MENU_QUOTA_VISIBILITY).c_str()
    );
    AppendWidgetMenuToggleSubmenuItem(
        menu,
        CreateGlassEffectMenu(state),
        kCommandToggleGlassEffect
    );
    AppendWidgetMenuToggleSubmenuItem(
        menu,
        CreateVibrationMenu(state),
        kCommandToggleVibration
    );
    AppendSubMenu(menu, CreateRefreshIntervalMenu(state), T(IDS_MENU_REFRESH_INTERVAL).c_str());
    AppendSubMenu(menu, CreateColorsMenu(state), T(IDS_MENU_COLORS).c_str());
    AppendSubMenu(menu, CreateLanguageMenu(state), T(IDS_MENU_LANGUAGE).c_str());
    AppendMenuSeparator(menu);
    if (state.motion_effects.debug_menu_enabled && state.visible) {
        AppendCommandMenuItem(
            menu,
            kCommandCopyInterfaceScreenshot,
            T(IDS_MENU_COPY_INTERFACE_SCREENSHOT).c_str()
        );
        AppendMenuSeparator(menu);
    }
    AppendCommandMenuItem(menu, kCommandResetDefaults, T(IDS_MENU_RESET_DEFAULTS).c_str());
    AppendCommandMenuItem(menu, kCommandExit, T(IDS_MENU_EXIT).c_str());
    return menu;
}

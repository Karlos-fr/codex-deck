// ============================================================================
// Codex Glass - Implementation du routage des commandes de menu
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp les commandes du menu contextuel en les
// regroupant par domaine fonctionnel : affichage, GlassEffect, vibration, usage,
// couleurs, langue, graphe et actions globales.
// ============================================================================

#include "WidgetCommandHandler.h"

#include "../color/WidgetColorTools.h"
#include "../shell/WidgetShellActions.h"
#include "../shell/WidgetScreenshotClipboard.h"
#include "../startup/WindowsStartup.h"
#include "../window/WidgetWindow.h"

// ----------------------------------------------------------------------------
// Traite les commandes generales et les actions shell.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleGlobalCommand(HWND hwnd, UINT command_id) {
    switch (command_id) {
    case kCommandMenuBanner:
        OpenCodexGlassRepository(hwnd);
        return true;

    case kCommandCopyInterfaceScreenshot:
        if (app_settings_.motion_effects.debug_menu_enabled && IsWindowVisible(hwnd)) {
            PostMessageW(hwnd, kCopyWidgetScreenshotMessage, 0, 0);
        }
        return true;

    case kCommandOpacitySlider:
    case kCommandGlassAppearanceDiffusionSlider:
    case kCommandGlassAppearanceTintSlider:
    case kCommandGlassAppearanceGrainSlider:
    case kCommandGlassAppearanceEdgeRefractionSlider:
    case kCommandGlassAppearanceEdgeWidthSlider:
    case kCommandGlassAppearanceChromaticSlider:
    case kCommandGlassAppearanceIndicatorRefractionSlider:
    case kCommandGlassAppearanceIndicatorWidthSlider:
    case kCommandGlassAppearanceElementSoftnessSlider:
    case kCommandGlassCalmIntensitySlider:
    case kCommandGlassCalmSpeedSlider:
    case kCommandGlassCalmWavelengthSlider:
    case kCommandGlassCalmNoiseSlider:
    case kCommandGlassLiquidIntensitySlider:
    case kCommandGlassLiquidSpeedSlider:
    case kCommandGlassLiquidWavelengthSlider:
    case kCommandGlassLiquidFluiditySlider:
    case kCommandGlassLiquidNoiseSlider:
    case kCommandGlassRainIntensitySlider:
    case kCommandGlassRainSpeedSlider:
    case kCommandGlassRainDensitySlider:
    case kCommandGlassRainRingSizeSlider:
    case kCommandGlassRainFadeSlider:
    case kCommandMotionVibrationIntensitySlider:
    case kCommandMotionVibrationDurationSlider:
    case kCommandMotionVibrationFrequencySlider:
    case kCommandMotionVibrationRefractionSlider:
    case kCommandMotionVibrationDampingSlider:
    case kCommandMotionPulseIntensitySlider:
    case kCommandMotionPulseDurationSlider:
    case kCommandMotionPulseSoftnessSlider:
    case kCommandMotionPulseExtentSlider:
    case kCommandMotionWaveIntensitySlider:
    case kCommandMotionWaveDurationSlider:
    case kCommandMotionWaveSpeedSlider:
    case kCommandMotionWaveWavelengthSlider:
    case kCommandMotionWaveCountSlider:
    case kCommandMotionWaveDampingSlider:
    case kCommandMotionWaveRefractionSlider:
    case kCommandMotionWaveFrontUndulationSlider:
    case kCommandMotionThresholdSlider:
    case kCommandMotionCooldownSlider:
        return true;

    case kCommandRefreshNow:
        RefreshUsageSnapshot(hwnd);
        return true;

    case kCommandExit:
        DestroyWindow(hwnd);
        return true;

    case kCommandResetDefaults:
        ResetSettingsToDefaults(hwnd);
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite les commandes de visibilite et de comportement de fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleVisibilityCommand(HWND hwnd, UINT command_id) {
    switch (command_id) {
    case kCommandToggleVisible:
        ClearGraphHover(hwnd);
        ToggleWidgetVisibility(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleAlwaysOnTop:
        app_settings_.always_on_top = !app_settings_.always_on_top;
        ApplyAlwaysOnTopSetting(hwnd, app_settings_.always_on_top);
        PositionColorPanelWindow();
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleLockPosition:
        app_settings_.lock_position = !app_settings_.lock_position;
        ApplyClickThroughTransparency(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleDockToEdges:
        app_settings_.dock_to_screen_edges = !app_settings_.dock_to_screen_edges;
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleClickThrough:
        app_settings_.click_through = !app_settings_.click_through;
        if (app_settings_.click_through) {
            ClearGraphHover(hwnd);
        }
        ApplyClickThroughSetting(hwnd, app_settings_);
        ApplyClickThroughHitTestTimer(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleHideWhenFullscreen:
        app_settings_.hide_when_fullscreen = !app_settings_.hide_when_fullscreen;
        UpdateFullscreenVisibility(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleStartWithWindows:
        if (SetStartWithWindowsEnabled(!app_settings_.start_with_windows)) {
            app_settings_.start_with_windows = !app_settings_.start_with_windows;
            SaveCurrentSettings(hwnd);
        }
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite les commandes d'affichage du contenu.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleDisplayCommand(HWND hwnd, UINT command_id) {
    switch (command_id) {
    case kCommandToggleQuotaFiveHour:
        app_settings_.quota_visibility.five_hour = !app_settings_.quota_visibility.five_hour;
        ApplyDisplayModeWindowSize(hwnd);
        RefreshTrayQuotaTooltip(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleQuotaWeekly:
        app_settings_.quota_visibility.weekly = !app_settings_.quota_visibility.weekly;
        ApplyDisplayModeWindowSize(hwnd);
        RefreshTrayQuotaTooltip(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleQuotaSparkFiveHour:
        app_settings_.quota_visibility.spark_five_hour =
            !app_settings_.quota_visibility.spark_five_hour;
        ApplyDisplayModeWindowSize(hwnd);
        RefreshTrayQuotaTooltip(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleQuotaSparkWeekly:
        app_settings_.quota_visibility.spark_weekly =
            !app_settings_.quota_visibility.spark_weekly;
        ApplyDisplayModeWindowSize(hwnd);
        RefreshTrayQuotaTooltip(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandToggleGraph:
        app_settings_.show_graph = !app_settings_.show_graph;
        if (app_settings_.show_graph) {
            app_settings_.display_mode = WidgetDisplayMode::Complete;
        } else if (app_settings_.display_mode == WidgetDisplayMode::Complete) {
            app_settings_.display_mode = WidgetDisplayMode::Compact;
        }
        if (!app_settings_.show_graph) {
            ClearGraphHover(hwnd);
        }
        ApplyDisplayModeWindowSize(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandDisplayMinimal:
        app_settings_.display_mode = WidgetDisplayMode::Minimal;
        app_settings_.show_graph = false;
        ClearGraphHover(hwnd);
        ApplyDisplayModeWindowSize(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandDisplayCompact:
        app_settings_.display_mode = WidgetDisplayMode::Compact;
        app_settings_.show_graph = false;
        ClearGraphHover(hwnd);
        ApplyDisplayModeWindowSize(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandDisplayComplete:
        app_settings_.display_mode = WidgetDisplayMode::Complete;
        app_settings_.show_graph = true;
        ApplyDisplayModeWindowSize(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandDisplayHorizontal:
        app_settings_.display_mode = WidgetDisplayMode::Horizontal;
        app_settings_.show_graph = false;
        ClearGraphHover(hwnd);
        ApplyDisplayModeWindowSize(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    case kCommandDisplayVertical:
        app_settings_.display_mode = WidgetDisplayMode::Vertical;
        app_settings_.show_graph = false;
        ClearGraphHover(hwnd);
        ApplyDisplayModeWindowSize(hwnd);
        SaveCurrentSettings(hwnd);
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite les commandes GlassEffect.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleGlassEffectCommand(HWND hwnd, UINT command_id) {
    switch (command_id) {
    case kCommandToggleGlassEffect:
        SetGlassEffectMode(
            hwnd,
            app_settings_.glass_effect_mode == GlassEffectMode::Enabled
                ? GlassEffectMode::Off
                : GlassEffectMode::Enabled
        );
        return true;

    case kCommandGlassEffectPresetClear:
        SetGlassEffectPreset(hwnd, GlassEffectPreset::Clear);
        return true;

    case kCommandGlassEffectPresetFrosted:
        SetGlassEffectPreset(hwnd, GlassEffectPreset::Frosted);
        return true;

    case kCommandGlassEffectPresetSubtle:
        SetGlassEffectPreset(hwnd, GlassEffectPreset::Subtle);
        return true;

    case kCommandGlassEffectPresetStrong:
        SetGlassEffectPreset(hwnd, GlassEffectPreset::Strong);
        return true;

    case kCommandGlassEffectPresetCustom:
        return true;

    case kCommandToggleGlassElementLens: {
        GlassEffectAppearanceSettings settings = app_settings_.glass_effect.appearance;
        settings.element_glass_enabled = !settings.element_glass_enabled;
        SetGlassEffectAppearanceSettings(hwnd, settings, true);
        return true;
    }

    case kCommandToggleGlassCalmWater: {
        GlassEffectCalmWaterSettings settings = app_settings_.glass_effect.calm_water;
        settings.enabled = !settings.enabled;
        SetGlassEffectCalmWaterSettings(hwnd, settings, true);
        return true;
    }

    case kCommandToggleGlassLiquid: {
        GlassEffectLiquidSettings settings = app_settings_.glass_effect.liquid;
        settings.enabled = !settings.enabled;
        SetGlassEffectLiquidSettings(hwnd, settings, true);
        return true;
    }

    case kCommandToggleGlassRain: {
        GlassEffectRainSettings settings = app_settings_.glass_effect.rain;
        settings.enabled = !settings.enabled;
        SetGlassEffectRainSettings(hwnd, settings, true);
        return true;
    }

    case kCommandResetGlassAppearance:
        ResetGlassEffectAppearance(hwnd);
        return true;

    case kCommandRandomizeGlassAppearance:
        RandomizeGlassEffectAppearance(hwnd);
        return true;

    case kCommandResetGlassCalmWater:
        ResetGlassEffectCalmWater(hwnd);
        return true;

    case kCommandRandomizeGlassCalmWater:
        RandomizeGlassEffectCalmWater(hwnd);
        return true;

    case kCommandResetGlassLiquid:
        ResetGlassEffectLiquid(hwnd);
        return true;

    case kCommandRandomizeGlassLiquid:
        RandomizeGlassEffectLiquid(hwnd);
        return true;

    case kCommandResetGlassRain:
        ResetGlassEffectRain(hwnd);
        return true;

    case kCommandRandomizeGlassRain:
        RandomizeGlassEffectRain(hwnd);
        return true;

    case kCommandResetGlassEffect:
        ResetGlassEffectSettings(hwnd);
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite les commandes de vibration.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleVibrationCommand(HWND hwnd, UINT command_id) {
    WidgetMotionEffectsSettings motion = app_settings_.motion_effects;
    bool preview_wave = false;

    switch (command_id) {
    case kCommandToggleVibration:
        motion.enabled = !motion.enabled;
        break;

    case kCommandToggleVibrationTestAuto:
        ToggleWidgetVibrationTestMode(hwnd);
        return true;

    case kCommandToggleRollingNumberTest:
        ToggleRollingNumberTestMode(hwnd);
        return true;

    case kCommandToggleMotionVibration:
        motion.vibration.enabled = !motion.vibration.enabled;
        break;

    case kCommandToggleMotionPulse:
        motion.pulse.enabled = !motion.pulse.enabled;
        break;

    case kCommandToggleMotionWave:
        motion.wave.enabled = !motion.wave.enabled;
        preview_wave = motion.wave.enabled;
        break;

    case kCommandMotionTriggerUsageDrop:
        motion.trigger_on_usage_drop = !motion.trigger_on_usage_drop;
        break;

    case kCommandMotionTriggerQuotaReset:
        motion.trigger_on_quota_reset = !motion.trigger_on_quota_reset;
        break;

    case kCommandResetMotionVibration:
        motion.vibration = ResetWidgetMotionVibrationSettings(motion.vibration);
        break;

    case kCommandRandomizeMotionVibration:
        motion.vibration = RandomizeWidgetMotionVibrationSettings(motion.vibration);
        break;

    case kCommandResetMotionPulse:
        motion.pulse = ResetWidgetMotionPulseSettings(motion.pulse);
        break;

    case kCommandRandomizeMotionPulse:
        motion.pulse = RandomizeWidgetMotionPulseSettings(motion.pulse);
        break;

    case kCommandResetMotionWave:
        motion.wave = ResetWidgetMotionWaveSettings(motion.wave);
        break;

    case kCommandRandomizeMotionWave:
        motion.wave = RandomizeWidgetMotionWaveSettings(motion.wave);
        break;

    case kCommandMotionVibrationHorizontal:
        motion.vibration.horizontal_enabled = !motion.vibration.horizontal_enabled;
        break;

    case kCommandMotionVibrationVertical:
        motion.vibration.vertical_enabled = !motion.vibration.vertical_enabled;
        break;

    case kCommandMotionPulseRepetitions1:
        motion.pulse.repetitions = WidgetMotionPulseRepetitions::One;
        break;

    case kCommandMotionPulseRepetitions2:
        motion.pulse.repetitions = WidgetMotionPulseRepetitions::Two;
        break;

    case kCommandMotionPulseRepetitions3:
        motion.pulse.repetitions = WidgetMotionPulseRepetitions::Three;
        break;

    case kCommandMotionWaveDirectionLeft:
        motion.wave.direction = WidgetMotionWaveDirection::Left;
        break;

    case kCommandMotionWaveDirectionRight:
        motion.wave.direction = WidgetMotionWaveDirection::Right;
        break;

    case kCommandMotionWaveDirectionUp:
        motion.wave.direction = WidgetMotionWaveDirection::Up;
        break;

    case kCommandMotionWaveDirectionDown:
        motion.wave.direction = WidgetMotionWaveDirection::Down;
        break;

    case kCommandMotionWaveDirectionRadial:
        motion.wave.direction = WidgetMotionWaveDirection::Radial;
        break;

    default:
        return false;
    }

    SetWidgetMotionEffectsSettings(hwnd, motion);
    if (preview_wave) {
        WidgetMotionEffectsSettings preview = motion;
        preview.enabled = true;
        preview.vibration.enabled = false;
        preview.pulse.enabled = false;
        StartWidgetVibration(hwnd, preview);
    }
    return true;
}

// ----------------------------------------------------------------------------
// Traite les commandes d'usage et de plage du graphe.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleUsageCommand(HWND hwnd, UINT command_id) {
    switch (command_id) {
    case kCommandRefreshManual:
        SetUsageRefreshInterval(hwnd, UsageRefreshInterval::Manual);
        return true;

    case kCommandRefresh10Seconds:
        SetUsageRefreshInterval(hwnd, UsageRefreshInterval::Seconds10);
        return true;

    case kCommandRefresh30Seconds:
        SetUsageRefreshInterval(hwnd, UsageRefreshInterval::Seconds30);
        return true;

    case kCommandRefresh1Minute:
        SetUsageRefreshInterval(hwnd, UsageRefreshInterval::Minute1);
        return true;

    case kCommandRefresh5Minutes:
        SetUsageRefreshInterval(hwnd, UsageRefreshInterval::Minutes5);
        return true;

    case kCommandRefresh15Minutes:
        SetUsageRefreshInterval(hwnd, UsageRefreshInterval::Minutes15);
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite les commandes de couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleColorCommand(HWND hwnd, UINT command_id) {
    if (command_id >= kCommandColorPresetBase) {
        const size_t preset_index = static_cast<size_t>(command_id - kCommandColorPresetBase);
        if (preset_index < app_settings_.color_presets.size()) {
            SetWidgetColors(hwnd, app_settings_.color_presets[preset_index].colors);
        }
        return true;
    }

    switch (command_id) {
    case kCommandColorCustomize:
        ToggleColorPanel(hwnd);
        return true;

    case kCommandColorReset:
        SetWidgetColors(hwnd, DefaultWidgetColorPresets().front().colors);
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite les commandes de langue.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
//
// Retour :
// - true si la commande a ete traitee.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleLanguageCommand(HWND hwnd, UINT command_id) {
    switch (command_id) {
    case kCommandLanguageAuto:
        SetWidgetLanguage(hwnd, UiLanguage::Auto);
        return true;

    case kCommandLanguageFrench:
        SetWidgetLanguage(hwnd, UiLanguage::French);
        return true;

    case kCommandLanguageEnglish:
        SetWidgetLanguage(hwnd, UiLanguage::English);
        return true;

    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite une commande issue du menu contextuel ou du menu tray.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - command_id : identifiant de commande selectionne.
// ----------------------------------------------------------------------------
void WidgetApp::HandleMenuCommand(HWND hwnd, UINT command_id) {
    if (HandleColorCommand(hwnd, command_id)
        || HandleGlobalCommand(hwnd, command_id)
        || HandleVisibilityCommand(hwnd, command_id)
        || HandleDisplayCommand(hwnd, command_id)
        || HandleGlassEffectCommand(hwnd, command_id)
        || HandleVibrationCommand(hwnd, command_id)
        || HandleUsageCommand(hwnd, command_id)
        || HandleLanguageCommand(hwnd, command_id)) {
        return;
    }
}

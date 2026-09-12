// ============================================================================
// Codex Glass - Implementation du controle applicatif GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp les decisions applicatives du mode
// GlassEffect : activation, presets, runtime, capture et warmup.
// ============================================================================

#include "WidgetGlassEffectController.h"

#include "WidgetTimers.h"

#include "../window/WidgetWindow.h"

// ----------------------------------------------------------------------------
// Applique et persiste le mode glass selectionne sans effet visuel.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - mode : mode glass demande par l'utilisateur.
// ----------------------------------------------------------------------------
void WidgetApp::SetGlassEffectMode(HWND hwnd, GlassEffectMode mode) {
    if (app_settings_.glass_effect_mode == mode) {
        return;
    }

    app_settings_.glass_effect_mode = mode;
    ApplyGlassEffectRuntimeMode(hwnd);
    ApplyClickThroughSetting(hwnd, app_settings_);
    ApplyClickThroughTransparency(hwnd);
    RefreshVisualResources(hwnd);
    ApplyGlassEffectAnimationTimer(hwnd);
    if (mode == GlassEffectMode::Enabled) {
        StartGlassEffectWarmupCapture(hwnd);
    }
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Applique et persiste le preset GlassEffect selectionne.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - preset : preset GlassEffect demande par l'utilisateur.
// ----------------------------------------------------------------------------
void WidgetApp::SetGlassEffectPreset(HWND hwnd, GlassEffectPreset preset) {
    if (preset == GlassEffectPreset::Custom) {
        return;
    }
    const bool preset_unchanged = app_settings_.glass_effect.preset == preset;
    const bool glass_effect_already_active = app_settings_.glass_effect_mode == GlassEffectMode::Enabled;
    if (preset_unchanged && glass_effect_already_active) {
        return;
    }

    app_settings_.glass_effect.preset = preset;
    app_settings_.glass_effect.appearance = GlassEffectAppearanceForPreset(preset);
    app_settings_.glass_effect = NormalizeGlassEffectSettings(app_settings_.glass_effect);
    RefreshGlassEffectAppearanceMenuState(app_settings_.glass_effect);
    app_settings_.glass_effect_mode = GlassEffectMode::Enabled;
    ApplyGlassEffectRuntimeMode(hwnd);
    ApplyClickThroughSetting(hwnd, app_settings_);
    ApplyClickThroughTransparency(hwnd);
    RefreshVisualResources(hwnd);
    ApplyGlassEffectAnimationTimer(hwnd);
    RefreshGlassEffectFrame(hwnd);
    StartGlassEffectWarmupCapture(hwnd);
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Applique les reglages d'apparence sans modifier les animations.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir.
// - settings : apparence demandee.
// - save_immediately : true pour ecrire l'INI sans delai.
// ----------------------------------------------------------------------------
void WidgetApp::SetGlassEffectAppearanceSettings(
    HWND hwnd,
    GlassEffectAppearanceSettings settings,
    bool save_immediately
) {
    app_settings_.glass_effect.appearance = NormalizeGlassEffectAppearanceSettings(settings);
    app_settings_.glass_effect.preset = GlassEffectPreset::Custom;
    app_settings_.glass_effect = NormalizeGlassEffectSettings(app_settings_.glass_effect);
    RefreshGlassEffectAppearanceMenuState(app_settings_.glass_effect);
    RefreshVisualResources(hwnd);
    if (save_immediately) {
        SaveCurrentSettings(hwnd);
    } else {
        ScheduleSettingsSave(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Applique les reglages Calm Water sans modifier les autres animations.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir.
// - settings : reglages Calm Water demandes.
// - save_immediately : true pour ecrire l'INI sans delai.
// ----------------------------------------------------------------------------
void WidgetApp::SetGlassEffectCalmWaterSettings(
    HWND hwnd,
    GlassEffectCalmWaterSettings settings,
    bool save_immediately
) {
    app_settings_.glass_effect.calm_water = NormalizeGlassEffectCalmWaterSettings(settings);
    ApplyGlassEffectAnimationTimer(hwnd);
    RefreshVisualResources(hwnd);
    if (save_immediately) SaveCurrentSettings(hwnd); else ScheduleSettingsSave(hwnd);
}

// ----------------------------------------------------------------------------
// Applique les reglages Liquid sans modifier les autres animations.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir.
// - settings : reglages Liquid demandes.
// - save_immediately : true pour ecrire l'INI sans delai.
// ----------------------------------------------------------------------------
void WidgetApp::SetGlassEffectLiquidSettings(
    HWND hwnd,
    GlassEffectLiquidSettings settings,
    bool save_immediately
) {
    app_settings_.glass_effect.liquid = NormalizeGlassEffectLiquidSettings(settings);
    ApplyGlassEffectAnimationTimer(hwnd);
    RefreshVisualResources(hwnd);
    if (save_immediately) SaveCurrentSettings(hwnd); else ScheduleSettingsSave(hwnd);
}

// ----------------------------------------------------------------------------
// Applique les reglages Rain sans modifier les autres animations.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir.
// - settings : reglages Rain demandes.
// - save_immediately : true pour ecrire l'INI sans delai.
// ----------------------------------------------------------------------------
void WidgetApp::SetGlassEffectRainSettings(
    HWND hwnd,
    GlassEffectRainSettings settings,
    bool save_immediately
) {
    app_settings_.glass_effect.rain = NormalizeGlassEffectRainSettings(settings);
    ApplyGlassEffectAnimationTimer(hwnd);
    RefreshVisualResources(hwnd);
    if (save_immediately) SaveCurrentSettings(hwnd); else ScheduleSettingsSave(hwnd);
}

// ----------------------------------------------------------------------------
// Restaure l'apparence du preset courant ou le preset Frosted par defaut.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::ResetGlassEffectAppearance(HWND hwnd) {
    const GlassEffectPreset preset = app_settings_.glass_effect.preset == GlassEffectPreset::Custom
        ? DefaultGlassEffectPreset()
        : app_settings_.glass_effect.preset;
    SetGlassEffectPreset(hwnd, preset);
}

// ----------------------------------------------------------------------------
// Restaure Calm Water en conservant son etat d'activation.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::ResetGlassEffectCalmWater(HWND hwnd) {
    const bool enabled = app_settings_.glass_effect.calm_water.enabled;
    GlassEffectCalmWaterSettings settings{};
    settings.enabled = enabled;
    SetGlassEffectCalmWaterSettings(hwnd, settings, true);
}

// ----------------------------------------------------------------------------
// Restaure Liquid en conservant son etat d'activation.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::ResetGlassEffectLiquid(HWND hwnd) {
    const bool enabled = app_settings_.glass_effect.liquid.enabled;
    GlassEffectLiquidSettings settings{};
    settings.enabled = enabled;
    SetGlassEffectLiquidSettings(hwnd, settings, true);
}

// ----------------------------------------------------------------------------
// Restaure Rain en conservant son etat d'activation.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::ResetGlassEffectRain(HWND hwnd) {
    const bool enabled = app_settings_.glass_effect.rain.enabled;
    GlassEffectRainSettings settings{};
    settings.enabled = enabled;
    SetGlassEffectRainSettings(hwnd, settings, true);
}

// ----------------------------------------------------------------------------
// Genere et applique une apparence Glass et une opacite aleatoires.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::RandomizeGlassEffectAppearance(HWND hwnd) {
    ApplyBackgroundOpacity(
        hwnd,
        static_cast<double>(RandomGlassEffectOpacityPercent()) / 100.0,
        false
    );
    SetGlassEffectAppearanceSettings(hwnd, RandomizeGlassEffectAppearanceSettings(), true);
}

// ----------------------------------------------------------------------------
// Genere et applique des reglages Calm Water aleatoires.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::RandomizeGlassEffectCalmWater(HWND hwnd) {
    SetGlassEffectCalmWaterSettings(
        hwnd,
        RandomizeGlassEffectCalmWaterSettings(app_settings_.glass_effect.calm_water),
        true
    );
}

// ----------------------------------------------------------------------------
// Genere et applique des reglages Liquid aleatoires.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::RandomizeGlassEffectLiquid(HWND hwnd) {
    SetGlassEffectLiquidSettings(
        hwnd,
        RandomizeGlassEffectLiquidSettings(app_settings_.glass_effect.liquid),
        true
    );
}

// ----------------------------------------------------------------------------
// Genere et applique des reglages Rain aleatoires.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::RandomizeGlassEffectRain(HWND hwnd) {
    SetGlassEffectRainSettings(
        hwnd,
        RandomizeGlassEffectRainSettings(app_settings_.glass_effect.rain),
        true
    );
}

// ----------------------------------------------------------------------------
// Restaure le bloc GlassEffect sans modifier son activation globale.
//
// Parametres :
// - hwnd : fenetre principale a rafraichir et proprietaire des reglages.
// ----------------------------------------------------------------------------
void WidgetApp::ResetGlassEffectSettings(HWND hwnd) {
    app_settings_.glass_effect = DefaultGlassEffectSettings();
    ApplyGlassEffectAnimationTimer(hwnd);
    RefreshVisualResources(hwnd);
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Applique le cycle de vie du runtime GlassEffect selon le mode selectionne.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyGlassEffectRuntimeMode(HWND hwnd) {
    if (app_settings_.glass_effect_mode != GlassEffectMode::Enabled) {
        KillTimer(hwnd, kGlassEffectCaptureTimerId);
        KillTimer(hwnd, kGlassEffectAnimationTimerId);
        glass_effect_capture_interactive_ = false;
        glass_effect_capture_idle_probe_ = false;
        glass_effect_warmup_capture_attempts_ = 0;
        if (glass_effect_) {
            glass_effect_->Shutdown();
            glass_effect_.reset();
        }
        ApplyColorPanelGlassEffectRuntimeMode();
        return;
    }

    if (!glass_effect_) {
        glass_effect_ = std::make_unique<WidgetGlassEffect>();
    }

    if (glass_effect_->EnsureInitialized(hwnd)) {
        KillTimer(hwnd, kGlassEffectCaptureTimerId);
        glass_effect_capture_interactive_ = false;
        glass_effect_capture_idle_probe_ = false;
        glass_effect_warmup_capture_attempts_ = 0;
        RefreshGlassEffectFrame(hwnd);
        ApplyColorPanelGlassEffectRuntimeMode();
        RefreshColorPanelGlassEffectFrame();
        StartGlassEffectIdleProbeCapture(hwnd);
        ApplyGlassEffectAnimationTimer(hwnd);
        return;
    }

    KillTimer(hwnd, kGlassEffectCaptureTimerId);
    KillTimer(hwnd, kGlassEffectAnimationTimerId);
    glass_effect_.reset();
    app_settings_.glass_effect_mode = GlassEffectMode::Off;
    ApplyColorPanelGlassEffectRuntimeMode();
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Synchronise le runtime GlassEffect propre a la palette flottante.
//
// La palette utilise un second crop issu de la meme acquisition DXGI afin de
// partager exactement le rythme de capture de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyColorPanelGlassEffectRuntimeMode() {
    const bool should_run = color_panel_open_
        && color_panel_hwnd_ != nullptr
        && app_settings_.glass_effect_mode == GlassEffectMode::Enabled;
    if (color_panel_open_ && color_panel_hwnd_ != nullptr) {
        ApplyClickThroughSetting(color_panel_hwnd_, app_settings_);
    }
    if (glass_effect_ == nullptr) {
        color_panel_glass_effect_warmup_pending_ = false;
        return;
    }

    glass_effect_->SetCompanionWindow(should_run ? color_panel_hwnd_ : nullptr);
    color_panel_glass_effect_warmup_pending_ = should_run
        && glass_effect_->LatestCompanionFrame() == nullptr;
}

// ----------------------------------------------------------------------------
// Capture une frame GlassEffect hors paint et redemande un rendu.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
//
// Retour :
// - true si une nouvelle frame a ete capturee.
// ----------------------------------------------------------------------------
bool WidgetApp::RefreshGlassEffectFrame(HWND hwnd) {
    if (glass_effect_ == nullptr) {
        return false;
    }

    if (!glass_effect_->IsInitialized()) {
        if (!glass_effect_->EnsureInitialized(hwnd)) {
            return false;
        }
        ApplyColorPanelGlassEffectRuntimeMode();
    }

    if (glass_effect_->TickCapture()) {
        InvalidateRect(hwnd, nullptr, FALSE);
        if (color_panel_open_ && color_panel_hwnd_ != nullptr) {
            color_panel_glass_effect_warmup_pending_ = glass_effect_->LatestCompanionFrame() == nullptr;
            InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
        }
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Reinitialise Glass apres une transition systeme qui invalide Desktop Duplication.
//
// Parametres :
// - hwnd : fenetre principale dont la capture doit etre recreee.
// - resume_capture : indique si les tentatives de reprise doivent demarrer.
//
// Effet de bord :
// - libere le device et les anciennes frames, puis arme une reprise asynchrone.
// ----------------------------------------------------------------------------
void WidgetApp::ResetGlassEffectAfterSystemTransition(HWND hwnd, bool resume_capture) {
    if (app_settings_.glass_effect_mode != GlassEffectMode::Enabled) {
        return;
    }

    KillTimer(hwnd, kGlassEffectCaptureTimerId);
    glass_effect_capture_interactive_ = false;
    glass_effect_capture_idle_probe_ = false;
    glass_effect_warmup_capture_attempts_ = 0;
    color_panel_glass_effect_warmup_pending_ = false;
    if (glass_effect_ != nullptr) {
        glass_effect_->Shutdown();
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    if (color_panel_hwnd_ != nullptr) {
        InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
    }
    if (resume_capture && IsWindowVisible(hwnd)) {
        if (glass_effect_ == nullptr) {
            glass_effect_ = std::make_unique<WidgetGlassEffect>();
        }
        StartGlassEffectWarmupCapture(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Redemande le rendu si le crop compagnon de la palette est disponible.
//
// Retour :
// - true si une frame compagnon est disponible.
// ----------------------------------------------------------------------------
bool WidgetApp::RefreshColorPanelGlassEffectFrame() {
    if (glass_effect_ == nullptr || color_panel_hwnd_ == nullptr) {
        return false;
    }

    if (glass_effect_->LatestCompanionFrame() != nullptr) {
        color_panel_glass_effect_warmup_pending_ = false;
        InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Lance des tentatives courtes de capture GlassEffect apres une creation ou taille.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::StartGlassEffectWarmupCapture(HWND hwnd) {
    if (glass_effect_ == nullptr || glass_effect_capture_interactive_) {
        return;
    }

    glass_effect_capture_idle_probe_ = false;
    glass_effect_warmup_capture_attempts_ = kGlassEffectWarmupCaptureAttempts;
    SetTimer(hwnd, kGlassEffectCaptureTimerId, kGlassEffectWarmupCaptureIntervalMs, nullptr);
}

// ----------------------------------------------------------------------------
// Lance le probe GlassEffect idle qui reveille la capture seulement si le bureau change.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::StartGlassEffectIdleProbeCapture(HWND hwnd) {
    if (glass_effect_ == nullptr || glass_effect_capture_interactive_) {
        return;
    }

    glass_effect_warmup_capture_attempts_ = 0;
    glass_effect_capture_idle_probe_ = true;
    SetTimer(hwnd, kGlassEffectCaptureTimerId, kGlassEffectIdleProbeCaptureIntervalMs, nullptr);
}

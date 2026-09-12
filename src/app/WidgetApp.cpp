// ============================================================================
// Codex Glass - Implementation de l'orchestration applicative
// ----------------------------------------------------------------------------
// Ce fichier coordonne le cycle de vie Win32 du widget, les reglages, les
// timers, le fournisseur d'usage, le rendu, les menus et l'historique local.
// ============================================================================

#include "WidgetApp.h"

#include "WidgetTimers.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../usage/UsageRefreshWorker.h"
#include "../color/WidgetColorTools.h"
#include "../window/WidgetColorPanelWindow.h"
#include "../window/WidgetDocking.h"
#include "../menu/WidgetMenuSlider.h"
#include "../motion/WidgetMotionCurves.h"
#include "../shell/WidgetShellMessages.h"
#include "../shell/WidgetScreenshotClipboard.h"
#include "../shell/WidgetTrayIcon.h"
#include "../shell/WidgetTrayTooltip.h"
#include "../startup/WindowsStartup.h"
#include "../window/WidgetWindow.h"

#include <shellapi.h>
#include <windowsx.h>
#include <wtsapi32.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

namespace {

// Opacite minimale acceptee par les reglages utilisateur.
constexpr double kMinimumBackgroundOpacity = 0.20;

// Opacite maximale acceptee par les reglages utilisateur.
constexpr double kMaximumBackgroundOpacity = 1.00;

}  // namespace

// ----------------------------------------------------------------------------
// Initialise une application avec ses valeurs par defaut.
// ----------------------------------------------------------------------------
WidgetApp::WidgetApp() = default;

// ----------------------------------------------------------------------------
// Detruit les ressources possedees par l'application.
// ----------------------------------------------------------------------------
WidgetApp::~WidgetApp() = default;

// ----------------------------------------------------------------------------
// Transfere le dernier snapshot d'activite publie par le watcher.
//
// Parametres :
// - hwnd : fenetre principale, reservee a la future integration visuelle.
// ----------------------------------------------------------------------------
void WidgetApp::CompleteCodexActivityRefresh(HWND hwnd) {
    const std::optional<CodexActivitySnapshot> snapshot = codex_activity_monitor_.TakeSnapshot();
    if (snapshot.has_value()) {
        codex_activity_snapshot_ = *snapshot;
        const auto now = WidgetActivityController::Clock::now();
        widget_activity_controller_.Update(codex_activity_snapshot_, now);
        widget_activity_frame_ = widget_activity_controller_.Frame(now);
        ApplyActivityVeinAnimationTimer(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ----------------------------------------------------------------------------
// Lance l'application et execute la boucle de messages Win32.
//
// Parametres :
// - instance : handle de l'instance courante de l'application.
// - command_show : mode d'affichage initial demande par Windows.
//
// Retour :
// - code de sortie du processus.
// ----------------------------------------------------------------------------
int WidgetApp::Run(HINSTANCE instance, int command_show) {
    app_settings_ = LoadAppSettings(CalculateInitialWindowRect());
    SetUiLanguage(app_settings_.language);
    ApplyStartupSettings();
    usage_history_store_.Open(GetUsageHistoryDatabasePath());

    if (!widget_renderer_.Initialize() || !color_panel_renderer_.Initialize()) {
        MessageBoxW(nullptr, T(IDS_ERROR_GRAPHICS_INIT).c_str(), T(IDS_APP_TITLE).c_str(), MB_ICONERROR);
        return 1;
    }

    if (!RegisterMainWindowClass(instance, WidgetApp::WindowProc)) {
        MessageBoxW(nullptr, T(IDS_ERROR_WINDOW_CLASS).c_str(), T(IDS_APP_TITLE).c_str(), MB_ICONERROR);
        return 1;
    }

    if (!RegisterWidgetColorPanelWindowClass(instance, WidgetApp::ColorPanelWindowProc)) {
        MessageBoxW(nullptr, T(IDS_ERROR_WINDOW_CLASS).c_str(), T(IDS_APP_TITLE).c_str(), MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateMainWindow(instance, app_settings_, this);
    if (!hwnd) {
        MessageBoxW(nullptr, T(IDS_ERROR_WINDOW_CREATE).c_str(), T(IDS_APP_TITLE).c_str(), MB_ICONERROR);
        return 1;
    }

    main_hwnd_ = hwnd;
    ApplyPreferredWindowSize(hwnd, app_settings_, usage_snapshot_);
    ShowWindow(hwnd, command_show == SW_HIDE ? SW_SHOWNOACTIVATE : command_show);
    UpdateWindow(hwnd);
    ApplyGlassEffectRuntimeMode(hwnd);
    StartGlassEffectWarmupCapture(hwnd);
    UpdateWindow(hwnd);

    return RunMessageLoop();
}

// ----------------------------------------------------------------------------
// Procedure de fenetre statique branchee sur la classe Win32.
//
// Parametres :
// - hwnd : handle de la fenetre concernee.
// - message : identifiant du message Win32 recu.
// - wparam : premier parametre du message.
// - lparam : second parametre du message.
//
// Retour :
// - valeur de traitement attendue par Win32 pour le message recu.
// ----------------------------------------------------------------------------
LRESULT CALLBACK WidgetApp::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    WidgetApp* app = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        app = static_cast<WidgetApp*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<WidgetApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app == nullptr) {
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    return app->HandleWindowMessage(hwnd, message, wparam, lparam);
}

// ----------------------------------------------------------------------------
// Procedure de fenetre statique de la palette flottante de couleurs.
//
// Parametres :
// - hwnd : handle de la palette concernee.
// - message : message Win32 recu.
// - wparam : premier parametre du message.
// - lparam : second parametre du message.
//
// Retour :
// - valeur de traitement attendue par Win32.
// ----------------------------------------------------------------------------
LRESULT CALLBACK WidgetApp::ColorPanelWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    WidgetApp* app = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        app = static_cast<WidgetApp*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<WidgetApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app == nullptr) {
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    return app->HandleColorPanelWindowMessage(hwnd, message, wparam, lparam);
}

// ----------------------------------------------------------------------------
// Force la recreation des ressources graphiques apres un changement visuel.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// ----------------------------------------------------------------------------
void WidgetApp::RefreshVisualResources(HWND hwnd) {
    widget_renderer_.RefreshVisualResources(hwnd);
    if (color_panel_hwnd_ != nullptr) {
        color_panel_renderer_.RefreshVisualResources(color_panel_hwnd_);
    }
}

// ----------------------------------------------------------------------------
// Persiste les reglages courants apres synchronisation avec la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre dont l'etat doit etre lu.
// ----------------------------------------------------------------------------
void WidgetApp::SaveCurrentSettings(HWND hwnd) {
    UpdateSettingsFromWindow(hwnd);
    settings_save_worker_.Schedule(app_settings_);
}

// ----------------------------------------------------------------------------
// Applique l'opacite du fond et force la recreation des brosses.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// - opacity : opacite demandee entre 0 et 1.
// - save_immediately : indique si les reglages doivent etre sauvegardes sans delai.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyBackgroundOpacity(HWND hwnd, double opacity, bool save_immediately) {
    app_settings_.background_opacity = std::clamp(
        opacity,
        kMinimumBackgroundOpacity,
        kMaximumBackgroundOpacity
    );
    ApplyClickThroughSetting(hwnd, app_settings_);
    if (color_panel_hwnd_ != nullptr) {
        ApplyClickThroughSetting(color_panel_hwnd_, app_settings_);
    }
    ApplyClickThroughTransparency(hwnd);
    RefreshVisualResources(hwnd);
    if (save_immediately) {
        KillTimer(hwnd, kSettingsSaveTimerId);
        SaveCurrentSettings(hwnd);
    } else {
        ScheduleSettingsSave(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Applique un jeu de couleurs et force la recreation des brosses.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// - colors : couleurs a appliquer.
// - save_immediately : indique si les reglages doivent etre sauvegardes sans delai.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyWidgetColors(HWND hwnd, const WidgetColorSettings& colors, bool save_immediately) {
    app_settings_.colors = colors;
    UpsertCustomColorPreset(app_settings_);
    RefreshVisualResources(hwnd);
    if (save_immediately) {
        KillTimer(hwnd, kSettingsSaveTimerId);
        SaveCurrentSettings(hwnd);
    } else {
        ScheduleSettingsSave(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Applique un jeu de couleurs et sauvegarde les reglages immediatement.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// - colors : couleurs a appliquer.
// ----------------------------------------------------------------------------
void WidgetApp::SetWidgetColors(HWND hwnd, const WidgetColorSettings& colors) {
    ApplyWidgetColors(hwnd, colors, true);
}

// ----------------------------------------------------------------------------
// Applique une seule couleur et force la recreation des brosses.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// - field : champ couleur a modifier.
// - color : nouvelle couleur.
// ----------------------------------------------------------------------------
void WidgetApp::SetSingleWidgetColor(HWND hwnd, WidgetColorField field, COLORREF color) {
    SetWidgetColorField(app_settings_.colors, field, color);
    UpsertCustomColorPreset(app_settings_);
    RefreshVisualResources(hwnd);
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Ouvre ou ferme l'extension laterale de couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre a redimensionner.
// ----------------------------------------------------------------------------
void WidgetApp::ToggleColorPanel(HWND hwnd) {
    SetColorPanelOpen(hwnd, !color_panel_open_);
}

// ----------------------------------------------------------------------------
// Definit l'etat d'ouverture de l'extension laterale de couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre a redimensionner.
// - open : nouvel etat d'ouverture.
// ----------------------------------------------------------------------------
void WidgetApp::SetColorPanelOpen(HWND hwnd, bool open) {
    const bool state_already_applied = open
        ? color_panel_open_ && color_panel_hwnd_ != nullptr
        : !color_panel_open_ && color_panel_hwnd_ == nullptr;
    if (state_already_applied) {
        return;
    }

    if (open && !EnsureColorPanelWindow(hwnd)) {
        return;
    }

    color_panel_open_ = open;
    color_panel_interaction_ = WidgetColorPanelInteraction{};
    if (color_panel_hwnd_ == nullptr) {
        return;
    }

    if (open) {
        PositionColorPanelWindow();
        ApplyNativeDarkMode(color_panel_hwnd_);
        ApplyClickThroughSetting(color_panel_hwnd_, app_settings_);
        ShowWindow(color_panel_hwnd_, SW_SHOWNOACTIVATE);
        ApplyColorPanelGlassEffectRuntimeMode();
        RefreshColorPanelGlassEffectFrame();
        StartGlassEffectWarmupCapture(hwnd);
        InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
        return;
    }

    if (GetCapture() == color_panel_hwnd_) {
        ReleaseCapture();
    }
    ApplyColorPanelGlassEffectRuntimeMode();
    DestroyWindow(color_panel_hwnd_);
}

// ----------------------------------------------------------------------------
// Cree la palette flottante lors de sa premiere ouverture.
//
// Parametres :
// - owner_hwnd : fenetre principale proprietaire.
//
// Retour :
// - true si la palette existe apres l'appel.
// ----------------------------------------------------------------------------
bool WidgetApp::EnsureColorPanelWindow(HWND owner_hwnd) {
    if (color_panel_hwnd_ != nullptr) {
        return true;
    }

    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_hwnd, GWLP_HINSTANCE));
    color_panel_hwnd_ = CreateWidgetColorPanelWindow(instance, owner_hwnd, this);
    return color_panel_hwnd_ != nullptr;
}

// ----------------------------------------------------------------------------
// Replace la palette flottante contre le widget principal.
// ----------------------------------------------------------------------------
void WidgetApp::PositionColorPanelWindow() {
    if (color_panel_open_ && color_panel_hwnd_ != nullptr && main_hwnd_ != nullptr) {
        PositionWidgetColorPanelWindow(color_panel_hwnd_, main_hwnd_);
    }
}

// ----------------------------------------------------------------------------
// Reinitialise les reglages principaux en conservant la position courante.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ResetSettingsToDefaults(HWND hwnd) {
    RECT current_rect{};
    GetWindowRect(hwnd, &current_rect);

    const std::vector<WidgetColorPreset> existing_color_presets = app_settings_.color_presets;
    AppSettings defaults{};
    defaults.window_rect = current_rect;
    defaults.color_presets = existing_color_presets.empty() ? DefaultWidgetColorPresets() : existing_color_presets;

    SetStartWithWindowsEnabled(false);
    app_settings_ = defaults;
    usage_refresh_interval_ = RefreshIntervalFromSeconds(app_settings_.refresh_interval_seconds);
    SetColorPanelOpen(hwnd, false);
    color_panel_interaction_ = WidgetColorPanelInteraction{};
    click_through_left_button_down_ = false;
    click_through_right_button_down_ = false;
    click_through_middle_button_down_ = false;

    SetUiLanguage(app_settings_.language);
    LocalizeWidgetColorPresetNames(app_settings_);
    ApplyAlwaysOnTopSetting(hwnd, app_settings_.always_on_top);
    ApplyDisplayModeWindowSize(hwnd);
    ApplyGlassEffectRuntimeMode(hwnd);
    ApplyClickThroughSetting(hwnd, app_settings_);
    ApplyClickThroughHitTestTimer(hwnd);
    ApplyUsageFetchTimer(hwnd);
    RefreshTrayQuotaTooltip(hwnd);
    RefreshVisualResources(hwnd);
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Applique la taille du mode d'affichage et recapture le fond GlassEffect si besoin.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyDisplayModeWindowSize(HWND hwnd) {
    ApplyPreferredWindowSize(hwnd, app_settings_, usage_snapshot_);
    RefreshGlassEffectFrame(hwnd);
    StartGlassEffectWarmupCapture(hwnd);
    PositionColorPanelWindow();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Applique et persiste les reglages cumulables des Effets Motion.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - settings : reglages Motion demandes.
// ----------------------------------------------------------------------------
void WidgetApp::SetWidgetMotionEffectsSettings(HWND hwnd, WidgetMotionEffectsSettings settings) {
    app_settings_.motion_effects = NormalizeWidgetMotionEffectsSettings(settings);
    if (!app_settings_.motion_effects.enabled) {
        StopWidgetVibration(hwnd);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Construit l'etat necessaire a l'affichage du menu contextuel.
//
// Parametres :
// - hwnd : handle de la fenetre utilisee pour connaitre la visibilite.
//
// Retour :
// - etat courant du menu.
// ----------------------------------------------------------------------------
WidgetMenuState WidgetApp::BuildWidgetMenuState(HWND hwnd) const {
    WidgetMenuState state{};
    state.visible = IsWindowVisible(hwnd) != FALSE;
    state.always_on_top = app_settings_.always_on_top;
    state.lock_position = app_settings_.lock_position;
    state.dock_to_screen_edges = app_settings_.dock_to_screen_edges;
    state.click_through = app_settings_.click_through;
    state.hide_when_fullscreen = app_settings_.hide_when_fullscreen;
    state.start_with_windows = app_settings_.start_with_windows;
    state.display_mode = app_settings_.display_mode;
    state.quota_visibility = app_settings_.quota_visibility;
    state.glass_effect_mode = app_settings_.glass_effect_mode;
    state.glass_effect = app_settings_.glass_effect;
    state.motion_effects = app_settings_.motion_effects;
    state.vibration_test_auto_enabled = vibration_test_auto_enabled_;
    state.rolling_number_test_enabled = rolling_number_test_enabled_;
    state.usage_refresh_interval = usage_refresh_interval_;
    state.background_opacity = app_settings_.background_opacity;
    state.active_control_color = app_settings_.colors.active_control;
    state.language = app_settings_.language;
    state.color_presets = &app_settings_.color_presets;
    state.color_panel_open = color_panel_open_;
    return state;
}

// ----------------------------------------------------------------------------
// Affiche le menu contextuel du widget avec le theme natif courant.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire du menu.
// - point : position ecran ou afficher le menu.
// - opened_from_tray : indique si le menu vient de l'icone de notification.
// ----------------------------------------------------------------------------
void WidgetApp::ShowWidgetContextMenu(HWND hwnd, POINT point, bool opened_from_tray) {
    ApplyNativeDarkMode(hwnd);
    ShowContextMenu(hwnd, point, BuildWidgetMenuState(hwnd), opened_from_tray);
}

// ----------------------------------------------------------------------------
// Applique et persiste une nouvelle langue d'interface.
//
// Parametres :
// - hwnd : handle de la fenetre a mettre a jour.
// - language : langue demandee par l'utilisateur.
// ----------------------------------------------------------------------------
void WidgetApp::SetWidgetLanguage(HWND hwnd, UiLanguage language) {
    const UiLanguage previous_language = ActiveUiLanguage();
    app_settings_.language = language;
    SetUiLanguage(language);
    RefreshContextMenuLocalization(previous_language);
    LocalizeWidgetColorPresetNames(app_settings_);
    SetWindowTextW(hwnd, T(IDS_APP_TITLE).c_str());
    RefreshTrayQuotaTooltip(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
    if (color_panel_hwnd_ != nullptr) {
        InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
    }
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Demarre une vibration courte du widget.
//
// Parametres :
// - hwnd : handle de la fenetre a animer.
// ----------------------------------------------------------------------------
void WidgetApp::StartWidgetVibration(HWND hwnd) {
    StartWidgetVibration(hwnd, app_settings_.motion_effects);
}

// ----------------------------------------------------------------------------
// Demarre une vibration courte avec des reglages explicites.
//
// Parametres :
// - hwnd : handle de la fenetre a animer.
// - settings : reglages utilises uniquement pour ce declenchement Motion.
// ----------------------------------------------------------------------------
void WidgetApp::StartWidgetVibration(HWND hwnd, const WidgetMotionEffectsSettings& settings) {
    const WidgetMotionEffectsSettings normalized = NormalizeWidgetMotionEffectsSettings(settings);
    if (!normalized.enabled
        || (!normalized.vibration.enabled && !normalized.pulse.enabled && !normalized.wave.enabled)) {
        return;
    }

    StopWidgetVibration(hwnd);

    RECT window_rect{};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    active_motion_effects_settings_ = normalized;
    const WidgetVibrationAnimation::TimePoint now = WidgetVibrationAnimation::Clock::now();
    if (normalized.vibration.enabled) {
        vibration_animation_.Start(now, std::chrono::milliseconds{normalized.vibration.duration_ms});
    }
    if (normalized.pulse.enabled) {
        pulse_animation_.Start(now, std::chrono::milliseconds{normalized.pulse.duration_ms});
    }
    if (normalized.wave.enabled) {
        wave_animation_.Start(now, std::chrono::milliseconds{normalized.wave.duration_ms});
        if (app_settings_.glass_effect_mode == GlassEffectMode::Enabled) {
            RefreshGlassEffectFrame(hwnd);
        }
    }
    vibration_motion_.Start(window_rect, normalized.vibration);
    vibration_glass_effect_state_ = vibration_glass_effect_.Evaluate(
        normalized,
        app_settings_.glass_effect_mode,
        0.0,
        0.0,
        0.0,
        0.0
    );
    SetTimer(
        hwnd,
        kVibrationAnimationTimerId,
        kVibrationAnimationIntervalMs,
        ForwardWidgetVibrationTimer
    );
    KillTimer(hwnd, kGlassEffectAnimationTimerId);
    UpdateWidgetVibration(hwnd);
}

// ----------------------------------------------------------------------------
// Avance la timeline de vibration et applique les moteurs actifs.
//
// Parametres :
// - hwnd : handle de la fenetre animee.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateWidgetVibration(HWND hwnd) {
    const WidgetVibrationAnimation::TimePoint now = WidgetVibrationAnimation::Clock::now();
    const bool vibration_active = active_motion_effects_settings_.vibration.enabled
        && vibration_animation_.IsActive(now);
    const bool pulse_active = active_motion_effects_settings_.pulse.enabled
        && pulse_animation_.IsActive(now);
    const bool wave_active = active_motion_effects_settings_.wave.enabled
        && wave_animation_.IsActive(now);
    if (!vibration_active && !pulse_active && !wave_active) {
        StopWidgetVibration(hwnd);
        return;
    }

    const WidgetMotionEffectsSettings normalized = NormalizeWidgetMotionEffectsSettings(active_motion_effects_settings_);
    const double vibration_progress = vibration_active ? vibration_animation_.Progress(now) : 1.0;
    const double vibration_intensity = vibration_active
        ? WidgetMotionVibrationEnvelope(vibration_progress, normalized.vibration.damping_percent)
        : 0.0;
    const double oscillation_progress = vibration_animation_.OscillationPhase(now);
    const double pulse_progress = pulse_active ? pulse_animation_.Progress(now) : 1.0;
    const double wave_progress = wave_active ? wave_animation_.Progress(now) : 1.0;

    if (vibration_active) {
        vibration_window_move_active_ = true;
        vibration_motion_.Apply(hwnd, oscillation_progress, vibration_intensity);
        vibration_window_move_active_ = false;
    } else if (vibration_motion_.HasTemporaryOffset()) {
        vibration_window_move_active_ = true;
        vibration_motion_.Finish(hwnd);
        vibration_window_move_active_ = false;
    }

    vibration_glass_effect_state_ = vibration_glass_effect_.Evaluate(
        normalized,
        app_settings_.glass_effect_mode,
        vibration_progress,
        vibration_intensity,
        pulse_progress,
        wave_progress
    );
    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE
    );
    if (color_panel_open_ && color_panel_hwnd_ != nullptr) {
        RedrawWindow(
            color_panel_hwnd_,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE
        );
    }
}

// ----------------------------------------------------------------------------
// Termine la vibration et replace le widget dans son etat stable.
//
// Parametres :
// - hwnd : handle de la fenetre animee.
// ----------------------------------------------------------------------------
void WidgetApp::StopWidgetVibration(HWND hwnd) {
    KillTimer(hwnd, kVibrationAnimationTimerId);
    vibration_glass_effect_state_ = WidgetVibrationGlassEffectState{};
    active_motion_effects_settings_ = WidgetMotionEffectsSettings{};
    vibration_window_move_active_ = true;
    vibration_motion_.Finish(hwnd);
    vibration_window_move_active_ = false;
    InvalidateRect(hwnd, nullptr, FALSE);
    if (color_panel_open_ && color_panel_hwnd_ != nullptr) {
        InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
    }
    ApplyGlassEffectAnimationTimer(hwnd);
}

// ----------------------------------------------------------------------------
// Active ou desactive le mode de test automatique de vibration.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ToggleWidgetVibrationTestMode(HWND hwnd) {
    vibration_test_auto_enabled_ = !vibration_test_auto_enabled_;
    ApplyWidgetVibrationTestTimer(hwnd);
    if (vibration_test_auto_enabled_) {
        TriggerWidgetVibrationTest(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Declenche une vibration de test sans attendre un changement de quota.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::TriggerWidgetVibrationTest(HWND hwnd) {
    if (!vibration_test_auto_enabled_) {
        return;
    }

    WidgetMotionEffectsSettings motion = NormalizeWidgetMotionEffectsSettings(app_settings_.motion_effects);
    motion.enabled = true;
    StartWidgetVibration(hwnd, motion);
    vibration_test_next_tick_ = WidgetVibrationAnimation::Clock::now()
        + std::chrono::milliseconds{kVibrationTestIntervalMs};
}

// ----------------------------------------------------------------------------
// Maintient le test et le rendu de vibration pendant un deplacement Windows.
//
// Parametres :
// - hwnd : handle de la fenetre principale en cours de deplacement.
//
// Effets de bord :
// - declenche un test arrive a echeance sans dependre de WM_TIMER ;
// - avance immediatement une vibration deja active.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateVibrationDuringMove(HWND hwnd) {
    const WidgetVibrationAnimation::TimePoint now = WidgetVibrationAnimation::Clock::now();
    if (vibration_test_auto_enabled_
        && vibration_test_next_tick_ != WidgetVibrationAnimation::TimePoint{}
        && now >= vibration_test_next_tick_) {
        TriggerWidgetVibrationTest(hwnd);
        return;
    }

    if (vibration_animation_.IsActive(now)
        || pulse_animation_.IsActive(now)
        || wave_animation_.IsActive(now)) {
        UpdateWidgetVibration(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Synchronise les reglages en memoire avec l'etat courant de la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a lire.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateSettingsFromWindow(HWND hwnd) {
    if (vibration_motion_.HasTemporaryOffset()) {
        app_settings_.window_rect = vibration_motion_.StableRect(hwnd);
        last_window_rect_ = app_settings_.window_rect;
        app_settings_.refresh_interval_seconds = RefreshIntervalToSeconds(usage_refresh_interval_);
        return;
    }

    RECT window_rect{};
    if (GetWindowRect(hwnd, &window_rect)) {
        app_settings_.window_rect = window_rect;
        last_window_rect_ = window_rect;
    }

    app_settings_.refresh_interval_seconds = RefreshIntervalToSeconds(usage_refresh_interval_);
}

// ----------------------------------------------------------------------------
// Memorise la position et la taille courantes du widget.
//
// Parametres :
// - hwnd : handle de la fenetre dont le rectangle doit etre memorise.
// ----------------------------------------------------------------------------
void WidgetApp::RememberWindowRect(HWND hwnd) {
    if (vibration_motion_.HasTemporaryOffset()) {
        app_settings_.window_rect = vibration_motion_.StableRect(hwnd);
        last_window_rect_ = app_settings_.window_rect;
        return;
    }

    RECT window_rect{};
    if (GetWindowRect(hwnd, &window_rect)) {
        last_window_rect_ = window_rect;
        app_settings_.window_rect = window_rect;
    }
}

// ----------------------------------------------------------------------------
// Applique les reglages locaux globaux au demarrage.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyStartupSettings() {
    app_settings_.window_rect = EnsureWindowRectVisible(app_settings_.window_rect);
    last_window_rect_ = app_settings_.window_rect;
    usage_refresh_interval_ = RefreshIntervalFromSeconds(app_settings_.refresh_interval_seconds);
    if (!SetStartWithWindowsEnabled(app_settings_.start_with_windows)) {
        app_settings_.start_with_windows = IsStartWithWindowsEnabled();
        SaveAppSettings(app_settings_);
    }
}

// ----------------------------------------------------------------------------
// Traite les messages envoyes a la fenetre principale.
//
// Parametres :
// - hwnd : handle de la fenetre concernee.
// - message : identifiant du message Win32 recu.
// - wparam : premier parametre du message.
// - lparam : second parametre du message.
//
// Retour :
// - valeur de traitement attendue par Win32 pour le message recu.
// ----------------------------------------------------------------------------
LRESULT WidgetApp::HandleWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (IsTaskbarCreatedMessage(message)) {
        AddTrayIcon(
            hwnd,
            BuildWidgetTrayTooltip(usage_snapshot_, app_settings_.quota_visibility)
        );
        return 0;
    }

    if (message == TrayIconMessage()) {
        // Message souris ou contexte transmis par l'icone de notification.
        const UINT tray_message = LOWORD(lparam);
        if (tray_message == WM_CONTEXTMENU) {
            POINT point{};
            GetCursorPos(&point);
            ShowWidgetContextMenu(hwnd, point, true);
            return 0;
        }

        if (tray_message == NIN_SELECT
            || tray_message == WM_LBUTTONUP
            || tray_message == WM_LBUTTONDBLCLK) {
            if (!IsWindowVisible(hwnd)) {
                ToggleWidgetVisibility(hwnd);
            }
            return 0;
        }

        return 0;
    }

    switch (message) {
    case WM_CREATE:
        WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
        AddTrayIcon(
            hwnd,
            BuildWidgetTrayTooltip(usage_snapshot_, app_settings_.quota_visibility)
        );
        StartWidgetTimers(hwnd);
        codex_activity_monitor_.Start(hwnd);
        RefreshUsageSnapshot(hwnd);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_POWERBROADCAST:
        if (wparam == PBT_APMSUSPEND) {
            ResetGlassEffectAfterSystemTransition(hwnd, false);
            return TRUE;
        }
        if (wparam == PBT_APMRESUMEAUTOMATIC
            || wparam == PBT_APMRESUMESUSPEND
            || wparam == PBT_APMRESUMECRITICAL) {
            ResetGlassEffectAfterSystemTransition(hwnd, true);
            return TRUE;
        }
        return TRUE;

    case WM_WTSSESSION_CHANGE:
        if (wparam == WTS_SESSION_LOCK) {
            ResetGlassEffectAfterSystemTransition(hwnd, false);
        } else if (wparam == WTS_SESSION_UNLOCK || wparam == WTS_SESSION_LOGON) {
            ResetGlassEffectAfterSystemTransition(hwnd, true);
        }
        return 0;

    case WM_MEASUREITEM:
        if (MeasureContextMenuItem(reinterpret_cast<MEASUREITEMSTRUCT*>(lparam))) {
            return TRUE;
        }
        if (MeasureWidgetMenuSlider(reinterpret_cast<MEASUREITEMSTRUCT*>(lparam))) {
            return TRUE;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_DRAWITEM:
        if (DrawContextMenuItem(reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) {
            return TRUE;
        }
        if (DrawWidgetMenuSlider(reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) {
            return TRUE;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case kOpacityMenuSliderChangedMessage:
        ApplyBackgroundOpacity(hwnd, static_cast<double>(wparam) / 100.0, false);
        return 0;

    case kGlassEffectMenuSliderChangedMessage: {
        const int value = static_cast<int>(wparam);
        switch (static_cast<UINT>(lparam)) {
        case kCommandGlassAppearanceDiffusionSlider: app_settings_.glass_effect.appearance.diffusion_percent = value; break;
        case kCommandGlassAppearanceTintSlider: app_settings_.glass_effect.appearance.tint_percent = value; break;
        case kCommandGlassAppearanceGrainSlider: app_settings_.glass_effect.appearance.grain_percent = value; break;
        case kCommandGlassAppearanceEdgeRefractionSlider: app_settings_.glass_effect.appearance.edge_refraction_percent = value; break;
        case kCommandGlassAppearanceEdgeWidthSlider: app_settings_.glass_effect.appearance.edge_width_percent = value; break;
        case kCommandGlassAppearanceChromaticSlider: app_settings_.glass_effect.appearance.chromatic_aberration_percent = value; break;
        case kCommandGlassAppearanceIndicatorRefractionSlider: app_settings_.glass_effect.appearance.indicator_refraction_percent = value; break;
        case kCommandGlassAppearanceIndicatorWidthSlider: app_settings_.glass_effect.appearance.indicator_width_percent = value; break;
        case kCommandGlassAppearanceElementSoftnessSlider: app_settings_.glass_effect.appearance.element_softness_percent = value; break;
        case kCommandGlassCalmIntensitySlider: app_settings_.glass_effect.calm_water.intensity_percent = value; break;
        case kCommandGlassCalmSpeedSlider: app_settings_.glass_effect.calm_water.speed_percent = value; break;
        case kCommandGlassCalmWavelengthSlider: app_settings_.glass_effect.calm_water.wavelength_percent = value; break;
        case kCommandGlassCalmNoiseSlider: app_settings_.glass_effect.calm_water.noise_percent = value; break;
        case kCommandGlassLiquidIntensitySlider: app_settings_.glass_effect.liquid.intensity_percent = value; break;
        case kCommandGlassLiquidSpeedSlider: app_settings_.glass_effect.liquid.speed_percent = value; break;
        case kCommandGlassLiquidWavelengthSlider: app_settings_.glass_effect.liquid.wavelength_percent = value; break;
        case kCommandGlassLiquidFluiditySlider: app_settings_.glass_effect.liquid.fluidity_percent = value; break;
        case kCommandGlassLiquidNoiseSlider: app_settings_.glass_effect.liquid.noise_percent = value; break;
        case kCommandGlassRainIntensitySlider: app_settings_.glass_effect.rain.intensity_percent = value; break;
        case kCommandGlassRainSpeedSlider: app_settings_.glass_effect.rain.speed_percent = value; break;
        case kCommandGlassRainDensitySlider: app_settings_.glass_effect.rain.density_percent = value; break;
        case kCommandGlassRainRingSizeSlider: app_settings_.glass_effect.rain.ring_size_percent = value; break;
        case kCommandGlassRainFadeSlider: app_settings_.glass_effect.rain.fade_percent = value; break;
        default: return 0;
        }
        const UINT command_id = static_cast<UINT>(lparam);
        if ((command_id >= kCommandGlassAppearanceDiffusionSlider
                && command_id <= kCommandGlassAppearanceIndicatorWidthSlider)
            || command_id == kCommandGlassAppearanceElementSoftnessSlider) {
            SetGlassEffectAppearanceSettings(hwnd, app_settings_.glass_effect.appearance, false);
        } else if (command_id >= kCommandGlassCalmIntensitySlider
            && command_id <= kCommandGlassCalmNoiseSlider) {
            SetGlassEffectCalmWaterSettings(hwnd, app_settings_.glass_effect.calm_water, false);
        } else if (command_id >= kCommandGlassLiquidIntensitySlider
            && command_id <= kCommandGlassLiquidNoiseSlider) {
            SetGlassEffectLiquidSettings(hwnd, app_settings_.glass_effect.liquid, false);
        } else {
            SetGlassEffectRainSettings(hwnd, app_settings_.glass_effect.rain, false);
        }
        return 0;
    }

    case kMotionEffectMenuSliderChangedMessage: {
        WidgetMotionEffectsSettings settings = app_settings_.motion_effects;
        const int value = static_cast<int>(wparam);
        switch (static_cast<UINT>(lparam)) {
        case kCommandMotionThresholdSlider: settings.usage_drop_threshold_percent = value; break;
        case kCommandMotionCooldownSlider: settings.minimum_interval_seconds = value; break;
        case kCommandMotionVibrationIntensitySlider: settings.vibration.intensity_percent = value; break;
        case kCommandMotionVibrationDurationSlider: settings.vibration.duration_ms = value; break;
        case kCommandMotionVibrationFrequencySlider: settings.vibration.frequency_percent = value; break;
        case kCommandMotionVibrationRefractionSlider: settings.vibration.refraction_percent = value; break;
        case kCommandMotionVibrationDampingSlider: settings.vibration.damping_percent = value; break;
        case kCommandMotionPulseIntensitySlider: settings.pulse.intensity_percent = value; break;
        case kCommandMotionPulseDurationSlider: settings.pulse.duration_ms = value; break;
        case kCommandMotionPulseSoftnessSlider: settings.pulse.softness_percent = value; break;
        case kCommandMotionPulseExtentSlider: settings.pulse.extent_percent = value; break;
        case kCommandMotionWaveIntensitySlider: settings.wave.intensity_percent = value; break;
        case kCommandMotionWaveDurationSlider: settings.wave.duration_ms = value; break;
        case kCommandMotionWaveSpeedSlider: settings.wave.speed_percent = value; break;
        case kCommandMotionWaveWavelengthSlider: settings.wave.wavelength_percent = value; break;
        case kCommandMotionWaveCountSlider: settings.wave.wave_count = value; break;
        case kCommandMotionWaveDampingSlider: settings.wave.damping_percent = value; break;
        case kCommandMotionWaveRefractionSlider: settings.wave.refraction_percent = value; break;
        case kCommandMotionWaveFrontUndulationSlider: settings.wave.front_undulation_percent = value; break;
        default: return 0;
        }
        app_settings_.motion_effects = NormalizeWidgetMotionEffectsSettings(settings);
        InvalidateRect(hwnd, nullptr, FALSE);
        ScheduleSettingsSave(hwnd);
        return 0;
    }

    case kUsageRefreshCompletedMessage:
        CompleteUsageRefresh(hwnd);
        return 0;

    case kTokenUsageRefreshCompletedMessage:
        CompleteTokenUsageRefresh(hwnd);
        return 0;

    case kCodexActivityChangedMessage:
        CompleteCodexActivityRefresh(hwnd);
        return 0;

    case WM_DESTROY:
        codex_activity_monitor_.Stop();
        widget_activity_controller_.Reset();
        widget_activity_frame_ = WidgetActivityFrame{};
        WTSUnRegisterSessionNotification(hwnd);
        color_panel_open_ = false;
        if (color_panel_hwnd_ != nullptr) {
            DestroyWindow(color_panel_hwnd_);
            color_panel_hwnd_ = nullptr;
        }
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        color_panel_interaction_ = WidgetColorPanelInteraction{};
        StopWidgetVibration(hwnd);
        SaveCurrentSettings(hwnd);
        settings_save_worker_.Flush();
        RemoveTrayIcon(hwnd);
        StopWidgetTimers(hwnd);
        usage_refresh_worker_.Stop();
        token_usage_refresh_worker_.Stop();
        usage_history_store_.Close();
        if (glass_effect_) {
            glass_effect_->Shutdown();
            glass_effect_.reset();
        }
        widget_renderer_.DiscardDeviceResources();
        color_panel_renderer_.DiscardDeviceResources();
        main_hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_MOVE:
        PositionColorPanelWindow();
        if (vibration_window_move_active_) {
            return 0;
        }
        RememberWindowRect(hwnd);
        RefreshGlassEffectFrame(hwnd);
        RefreshColorPanelGlassEffectFrame();
        return 0;

    case WM_MOVING:
        if (auto* moving_rect = reinterpret_cast<RECT*>(lparam); moving_rect != nullptr) {
            if (app_settings_.dock_to_screen_edges && !app_settings_.lock_position) {
                DockWindowRectToScreenEdges(*moving_rect);
            }
            if (color_panel_open_ && color_panel_hwnd_ != nullptr) {
                PositionWidgetColorPanelWindowForRect(color_panel_hwnd_, hwnd, *moving_rect);
            }
        }
        UpdateVibrationDuringMove(hwnd);
        UpdateRollingNumbersDuringMove(hwnd);
        return TRUE;

    case WM_ENTERSIZEMOVE:
        ClearGraphHover(hwnd);
        ClearTrayHideButtonInteraction(hwnd);
        if (glass_effect_ != nullptr) {
            glass_effect_capture_interactive_ = true;
            glass_effect_capture_idle_probe_ = false;
            glass_effect_warmup_capture_attempts_ = 0;
            RefreshGlassEffectFrame(hwnd);
            RefreshColorPanelGlassEffectFrame();
            SetTimer(hwnd, kGlassEffectCaptureTimerId, kGlassEffectActiveCaptureIntervalMs, nullptr);
        }
        return 0;

    case WM_EXITSIZEMOVE:
        RememberWindowRect(hwnd);
        RefreshGlassEffectFrame(hwnd);
        RefreshColorPanelGlassEffectFrame();
        if (glass_effect_ != nullptr) {
            KillTimer(hwnd, kGlassEffectCaptureTimerId);
            glass_effect_capture_interactive_ = false;
            glass_effect_capture_idle_probe_ = false;
            glass_effect_warmup_capture_attempts_ = 0;
            StartGlassEffectWarmupCapture(hwnd);
            ApplyGlassEffectAnimationTimer(hwnd);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        ScheduleSettingsSave(hwnd);
        return 0;

    case WM_SIZE:
        RememberWindowRect(hwnd);
        widget_renderer_.Resize(hwnd);
        RefreshGlassEffectFrame(hwnd);
        PositionColorPanelWindow();
        RefreshColorPanelGlassEffectFrame();
        return 0;

    case WM_GETMINMAXINFO:
        ApplyModeMinimumSize(hwnd, reinterpret_cast<MINMAXINFO*>(lparam), app_settings_, usage_snapshot_);
        return 0;

    case WM_NCHITTEST: {
        if (app_settings_.click_through) {
            POINT client_point{
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam),
            };
            ScreenToClient(hwnd, &client_point);

            if (!app_settings_.lock_position && IsClickThroughTitleDragHandle(hwnd, client_point)) {
                return HTCAPTION;
            }

            return HTTRANSPARENT;
        }

        if (app_settings_.lock_position) {
            return HTCLIENT;
        }

        POINT graph_point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &graph_point);
        if (IsTrayHideButtonPoint(hwnd, graph_point)
            || IsActivityVeinPoint(hwnd, graph_point)
            || IsGraphInteractivePoint(hwnd, graph_point)) {
            return HTCLIENT;
        }

        const LRESULT hit = DefWindowProcW(hwnd, message, wparam, lparam);
        if (hit == HTCLIENT) {
            return HTCAPTION;
        }

        return hit;
    }

    case WM_MOUSEMOVE: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (HandleGraphDrag(hwnd, point)) {
            return 0;
        }
        UpdateGraphHover(hwnd, point);
        UpdateTrayHideButtonHover(hwnd, point);
        UpdateActivityVeinHover(hwnd, point);
        return 0;
    }

    case WM_NCMOUSEMOVE: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &point);
        UpdateGraphHover(hwnd, point, true);
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_MOUSELEAVE:
        ClearGraphHover(hwnd);
        ClearTrayHideButtonInteraction(hwnd);
        ClearActivityVeinHover(hwnd);
        return 0;

    case WM_NCMOUSELEAVE:
        ClearGraphHover(hwnd);
        ClearTrayHideButtonInteraction(hwnd);
        ClearActivityVeinHover(hwnd);
        return 0;

    case WM_LBUTTONDOWN: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (HandleTrayHideButtonClick(hwnd, point)) {
            return 0;
        }
        if (HandleGraphClick(hwnd, point)) {
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (HandleTrayHideButtonRelease(hwnd, point)) {
            return 0;
        }
        if (HandleGraphButtonRelease(hwnd, point)) {
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_CAPTURECHANGED:
        ClearTrayHideButtonInteraction(hwnd);
        ClearActivityVeinHover(hwnd);
        if (graph_interaction_.pressed_graph_capture_button) {
            graph_interaction_.pressed_graph_capture_button = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (graph_interaction_.pressed_graph_tab.has_value()) {
            graph_interaction_.pressed_graph_tab.reset();
            graph_press_client_point_.reset();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && color_panel_open_) {
            SetColorPanelOpen(hwnd, false);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        widget_activity_frame_ = widget_activity_controller_.Frame(
            WidgetActivityController::Clock::now()
        );
        const WidgetGlassEffectFrame* glass_effect_frame = glass_effect_ != nullptr ? glass_effect_->LatestFrame() : nullptr;
        widget_renderer_.Render(
            hwnd,
            app_settings_,
            usage_snapshot_,
            token_usage_snapshot_,
            graph_interaction_,
            tray_hide_button_interaction_,
            usage_history_store_,
            app_settings_.graph_range,
            widget_activity_frame_,
            activity_vein_interaction_,
            quota_graph_animation_.Frame(WidgetQuotaGraphAnimation::Clock::now()),
            glass_effect_frame,
            vibration_glass_effect_state_,
            five_hour_rolling_animation_.Frame(WidgetRollingNumberAnimation::Clock::now()),
            weekly_rolling_animation_.Frame(WidgetRollingNumberAnimation::Clock::now()),
            refreshing_label_dot_count_
        );
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_TIMER:
        if (wparam == kDisplayRefreshTimerId) {
            UpdateFullscreenVisibility(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (wparam == kGlassEffectCaptureTimerId) {
            const bool main_captured = RefreshGlassEffectFrame(hwnd);
            const bool captured = main_captured;
            if (glass_effect_capture_interactive_) {
                return 0;
            }

            if (glass_effect_warmup_capture_attempts_ > 0) {
                --glass_effect_warmup_capture_attempts_;
                if ((captured && !color_panel_glass_effect_warmup_pending_)
                    || glass_effect_warmup_capture_attempts_ <= 0) {
                    glass_effect_warmup_capture_attempts_ = 0;
                    StartGlassEffectIdleProbeCapture(hwnd);
                }
                return 0;
            }

            if (!glass_effect_capture_idle_probe_) {
                KillTimer(hwnd, kGlassEffectCaptureTimerId);
            }
            return 0;
        }

        if (wparam == kGlassEffectAnimationTimerId) {
            InvalidateRect(hwnd, nullptr, FALSE);
            if (color_panel_open_ && color_panel_hwnd_ != nullptr) {
                InvalidateRect(color_panel_hwnd_, nullptr, FALSE);
            }
            return 0;
        }

        if (wparam == kActivityVeinAnimationTimerId) {
            widget_activity_frame_ = widget_activity_controller_.Frame(
                WidgetActivityController::Clock::now()
            );
            if (!widget_activity_frame_.animation_active || !IsWindowVisible(hwnd)) {
                KillTimer(hwnd, kActivityVeinAnimationTimerId);
                return 0;
            }
            RedrawWindow(
                hwnd,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE
            );
            return 0;
        }

        if (wparam == kRefreshingLabelAnimationTimerId) {
            if (usage_snapshot_.freshness != UsageFreshness::Refreshing) {
                StopRefreshingLabelAnimation(hwnd);
                return 0;
            }

            refreshing_label_dot_count_ = (refreshing_label_dot_count_ % 3U) + 1U;
            if (usage_refresh_completion_pending_) {
                CompleteUsageRefresh(hwnd);
                if (usage_snapshot_.freshness != UsageFreshness::Refreshing) {
                    return 0;
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (wparam == kQuotaGraphAnimationTimerId) {
            if (!quota_graph_animation_.IsActive(WidgetQuotaGraphAnimation::Clock::now())) {
                KillTimer(hwnd, kQuotaGraphAnimationTimerId);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (wparam == kClickThroughHitTestTimerId) {
            const bool pass_through_under_cursor = ApplyClickThroughTransparency(hwnd);
            RefreshGlassEffectAfterClickThroughMouseEvent(hwnd, pass_through_under_cursor);
            return 0;
        }

        if (wparam == kSettingsSaveTimerId) {
            KillTimer(hwnd, kSettingsSaveTimerId);
            SaveCurrentSettings(hwnd);
            return 0;
        }

        if (wparam == kVibrationAnimationTimerId) {
            UpdateWidgetVibration(hwnd);
            return 0;
        }

        if (wparam == kVibrationTestTimerId) {
            TriggerWidgetVibrationTest(hwnd);
            return 0;
        }

        if (wparam == kRollingNumberAnimationTimerId) {
            ApplyRollingNumberAnimationTimer(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (wparam == kRollingNumberTestTimerId) {
            TriggerRollingNumberTest(hwnd);
            return 0;
        }

        if (wparam == kUsageFetchTimerId) {
            RefreshUsageSnapshot(hwnd);
            return 0;
        }

        return 0;

    case WM_RBUTTONUP: {
        POINT point{
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam),
        };
        ClientToScreen(hwnd, &point);
        ShowWidgetContextMenu(hwnd, point);
        return 0;
    }

    case WM_NCRBUTTONUP: {
        POINT point{
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam),
        };
        ShowWidgetContextMenu(hwnd, point);
        return 0;
    }

    case WM_CONTEXTMENU: {
        POINT point{
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam),
        };
        if (point.x == -1 && point.y == -1) {
            GetCursorPos(&point);
        }

        ShowWidgetContextMenu(hwnd, point);
        return 0;
    }

    case WM_COMMAND:
        HandleMenuCommand(hwnd, LOWORD(wparam));
        return 0;

    case kCopyWidgetScreenshotMessage:
        if (app_settings_.motion_effects.debug_menu_enabled && IsWindowVisible(hwnd)) {
            CopyWidgetScreenshotToClipboard(hwnd);
        }
        return 0;

    case WM_SETTINGCHANGE:
        ApplyNativeDarkMode(hwnd);
        if (color_panel_hwnd_ != nullptr) {
            ApplyNativeDarkMode(color_panel_hwnd_);
            color_panel_renderer_.RefreshVisualResources(color_panel_hwnd_);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_DISPLAYCHANGE:
        {
            RECT applied_rect{};
            if (EnsureCurrentWindowVisible(hwnd, applied_rect)) {
                last_window_rect_ = applied_rect;
                app_settings_.window_rect = applied_rect;
            }
        }
        PositionColorPanelWindow();
        ResetGlassEffectAfterSystemTransition(hwnd, true);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED:
        if (ApplyDpiSuggestedRect(hwnd, reinterpret_cast<const RECT*>(lparam))) {
            ApplyPreferredWindowSize(hwnd, app_settings_, usage_snapshot_);
            RememberWindowRect(hwnd);
        }
        PositionColorPanelWindow();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

// ----------------------------------------------------------------------------
// Execute la boucle de messages principale de l'application.
//
// Retour :
// - code de sortie transmis par le message WM_QUIT.
// ----------------------------------------------------------------------------
int WidgetApp::RunMessageLoop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

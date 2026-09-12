// ============================================================================
// Codex Glass - Implementation des timers applicatifs
// ----------------------------------------------------------------------------
// Ce fichier regroupe les helpers qui arment ou desarment les timers Win32 du
// widget. Le traitement des messages WM_TIMER reste orchestre par WidgetApp.
// ============================================================================

#include "WidgetTimers.h"

#include "WidgetApp.h"

#include <chrono>

// ----------------------------------------------------------------------------
// Route un timer de vibration depuis les boucles modales Win32 vers WidgetApp.
//
// Parametres :
// - hwnd : fenetre proprietaire du timer.
// - message : message systeme reserve, non utilise.
// - timer_id : identifiant du timer a transmettre.
// - tick_count : compteur systeme reserve, non utilise.
//
// Effets de bord :
// - appelle synchroniquement le traitement WM_TIMER de la fenetre principale.
// ----------------------------------------------------------------------------
void CALLBACK ForwardWidgetVibrationTimer(
    HWND hwnd,
    UINT,
    UINT_PTR timer_id,
    DWORD
) {
    SendMessageW(hwnd, WM_TIMER, timer_id, 0);
}

// ----------------------------------------------------------------------------
// Route le timer Glass pendant les boucles modales de deplacement Win32.
//
// Parametres :
// - hwnd : fenetre proprietaire du timer.
// - message : message systeme reserve, non utilise.
// - timer_id : identifiant du timer a transmettre.
// - tick_count : compteur systeme reserve, non utilise.
//
// Effet de bord :
// - appelle synchroniquement le traitement WM_TIMER de la fenetre principale.
// ----------------------------------------------------------------------------
void CALLBACK ForwardWidgetGlassEffectTimer(
    HWND hwnd,
    UINT message,
    UINT_PTR timer_id,
    DWORD tick_count
) {
    static_cast<void>(message);
    static_cast<void>(tick_count);
    SendMessageW(hwnd, WM_TIMER, timer_id, 0);
}

// ----------------------------------------------------------------------------
// Route le timer de la veine pendant les boucles modales de Windows.
//
// Parametres :
// - hwnd : fenetre proprietaire du timer.
// - message : message systeme reserve, non utilise.
// - timer_id : identifiant du timer a transmettre.
// - tick_count : compteur systeme reserve, non utilise.
// ----------------------------------------------------------------------------
void CALLBACK ForwardWidgetActivityVeinTimer(
    HWND hwnd,
    UINT message,
    UINT_PTR timer_id,
    DWORD tick_count
) {
    static_cast<void>(message);
    static_cast<void>(tick_count);
    SendMessageW(hwnd, WM_TIMER, timer_id, 0);
}

// ----------------------------------------------------------------------------
// Applique le timer de recuperation d'usage selon l'intervalle courant.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyUsageFetchTimer(HWND hwnd) {
    KillTimer(hwnd, kUsageFetchTimerId);
    const UINT interval_ms = RefreshIntervalMilliseconds(usage_refresh_interval_);
    if (interval_ms > 0) {
        SetTimer(hwnd, kUsageFetchTimerId, interval_ms, nullptr);
    }
}

// ----------------------------------------------------------------------------
// Programme une sauvegarde differee des reglages.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::ScheduleSettingsSave(HWND hwnd) {
    KillTimer(hwnd, kSettingsSaveTimerId);
    SetTimer(hwnd, kSettingsSaveTimerId, kSettingsSaveDelayMs, nullptr);
}

// ----------------------------------------------------------------------------
// Active ou desactive le suivi souris requis par le click-through.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyClickThroughHitTestTimer(HWND hwnd) {
    KillTimer(hwnd, kClickThroughHitTestTimerId);
    ApplyClickThroughTransparency(hwnd);

    if (!app_settings_.click_through) {
        click_through_left_button_down_ = false;
        click_through_right_button_down_ = false;
        click_through_middle_button_down_ = false;
    }

    if (app_settings_.click_through) {
        SetTimer(hwnd, kClickThroughHitTestTimerId, kClickThroughHitTestIntervalMs, nullptr);
    }
}

// ----------------------------------------------------------------------------
// Active ou desactive le timer de test automatique de vibration.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyWidgetVibrationTestTimer(HWND hwnd) {
    KillTimer(hwnd, kVibrationTestTimerId);
    vibration_test_next_tick_ = WidgetVibrationAnimation::TimePoint{};
    if (vibration_test_auto_enabled_) {
        vibration_test_next_tick_ = WidgetVibrationAnimation::Clock::now()
            + std::chrono::milliseconds{kVibrationTestIntervalMs};
        SetTimer(
            hwnd,
            kVibrationTestTimerId,
            kVibrationTestIntervalMs,
            ForwardWidgetVibrationTimer
        );
    }
}

// ----------------------------------------------------------------------------
// Active le timer temporaire de test des chiffres quand l'option est cochee.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyRollingNumberTestTimer(HWND hwnd) {
    KillTimer(hwnd, kRollingNumberTestTimerId);
    rolling_number_test_next_tick_ = WidgetRollingNumberAnimation::TimePoint{};
    if (rolling_number_test_enabled_) {
        rolling_number_test_next_tick_ = WidgetRollingNumberAnimation::Clock::now()
            + std::chrono::milliseconds{kRollingNumberTestIntervalMs};
        SetTimer(hwnd, kRollingNumberTestTimerId, kRollingNumberTestIntervalMs, nullptr);
    }
}

// ----------------------------------------------------------------------------
// Demarre l'animation des points du libelle de rafraichissement.
//
// Parametres :
// - hwnd : handle de la fenetre recevant le timer.
// ----------------------------------------------------------------------------
void WidgetApp::StartRefreshingLabelAnimation(HWND hwnd) {
    KillTimer(hwnd, kRefreshingLabelAnimationTimerId);
    refreshing_label_dot_count_ = 1;
    SetTimer(
        hwnd,
        kRefreshingLabelAnimationTimerId,
        kRefreshingLabelAnimationIntervalMs,
        nullptr
    );
}

// ----------------------------------------------------------------------------
// Arrete l'animation des points du libelle de rafraichissement.
//
// Parametres :
// - hwnd : handle de la fenetre qui possede le timer.
// ----------------------------------------------------------------------------
void WidgetApp::StopRefreshingLabelAnimation(HWND hwnd) {
    KillTimer(hwnd, kRefreshingLabelAnimationTimerId);
    refreshing_label_dot_count_ = 1;
}

// ----------------------------------------------------------------------------
// Active le timer uniquement pendant la transition du graphique Quotas.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les images intermediaires.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyQuotaGraphAnimationTimer(HWND hwnd) {
    KillTimer(hwnd, kQuotaGraphAnimationTimerId);
    if (quota_graph_animation_.IsActive(WidgetQuotaGraphAnimation::Clock::now())) {
        SetTimer(hwnd, kQuotaGraphAnimationTimerId, kQuotaGraphAnimationIntervalMs, nullptr);
    }
}

// ----------------------------------------------------------------------------
// Active ou desactive le timer de rendu anime GlassEffect.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyGlassEffectAnimationTimer(HWND hwnd) {
    KillTimer(hwnd, kGlassEffectAnimationTimerId);
    if (
        app_settings_.glass_effect_mode == GlassEffectMode::Enabled
        && HasActiveGlassEffectAnimation(app_settings_.glass_effect)
        && IsWindowVisible(hwnd)
    ) {
        SetTimer(
            hwnd,
            kGlassEffectAnimationTimerId,
            kGlassEffectAnimationIntervalMs,
            ForwardWidgetGlassEffectTimer
        );
    }
}

// ----------------------------------------------------------------------------
// Active le rendu continu uniquement tant qu'une session reste visible.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les images de la veine.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyActivityVeinAnimationTimer(HWND hwnd) {
    KillTimer(hwnd, kActivityVeinAnimationTimerId);
    widget_activity_frame_ = widget_activity_controller_.Frame(
        WidgetActivityController::Clock::now()
    );
    if (widget_activity_frame_.animation_active && IsWindowVisible(hwnd)) {
        SetTimer(
            hwnd,
            kActivityVeinAnimationTimerId,
            kActivityVeinAnimationIntervalMs,
            ForwardWidgetActivityVeinTimer
        );
    }
}

// ----------------------------------------------------------------------------
// Demarre les timers du widget.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les messages WM_TIMER.
// ----------------------------------------------------------------------------
void WidgetApp::StartWidgetTimers(HWND hwnd) {
    SetTimer(hwnd, kDisplayRefreshTimerId, kDisplayRefreshIntervalMs, nullptr);
    ApplyUsageFetchTimer(hwnd);
    ApplyClickThroughHitTestTimer(hwnd);
    ApplyWidgetVibrationTestTimer(hwnd);
    ApplyRollingNumberTestTimer(hwnd);
    ApplyGlassEffectAnimationTimer(hwnd);
    ApplyActivityVeinAnimationTimer(hwnd);
}

// ----------------------------------------------------------------------------
// Arrete les timers du widget.
//
// Parametres :
// - hwnd : handle de la fenetre dont les timers doivent etre arretes.
// ----------------------------------------------------------------------------
void WidgetApp::StopWidgetTimers(HWND hwnd) {
    KillTimer(hwnd, kUsageFetchTimerId);
    KillTimer(hwnd, kDisplayRefreshTimerId);
    KillTimer(hwnd, kGlassEffectCaptureTimerId);
    KillTimer(hwnd, kClickThroughHitTestTimerId);
    KillTimer(hwnd, kSettingsSaveTimerId);
    KillTimer(hwnd, kVibrationAnimationTimerId);
    KillTimer(hwnd, kVibrationTestTimerId);
    KillTimer(hwnd, kRollingNumberAnimationTimerId);
    KillTimer(hwnd, kRollingNumberTestTimerId);
    KillTimer(hwnd, kGlassEffectAnimationTimerId);
    KillTimer(hwnd, kRefreshingLabelAnimationTimerId);
    KillTimer(hwnd, kQuotaGraphAnimationTimerId);
    KillTimer(hwnd, kActivityVeinAnimationTimerId);
    glass_effect_capture_interactive_ = false;
    glass_effect_capture_idle_probe_ = false;
    glass_effect_warmup_capture_attempts_ = 0;
}

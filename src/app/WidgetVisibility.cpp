// ============================================================================
// Codex Glass - Implementation de la visibilite applicative
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp les chemins cacher/afficher et plein
// ecran. WidgetApp reste proprietaire de l'etat, ce module ne fait que porter
// les definitions membres associees.
// ============================================================================

#include "WidgetVisibility.h"

#include "WidgetTimers.h"
#include "../window/WidgetWindow.h"

// ----------------------------------------------------------------------------
// Affiche le widget sans voler le focus et relance les effets visuels dependants
// de la visibilite de la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a afficher.
// ----------------------------------------------------------------------------
void WidgetApp::ShowWidget(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    ApplyAlwaysOnTopSetting(hwnd, app_settings_.always_on_top);
    if (app_settings_.glass_effect_mode == GlassEffectMode::Enabled) {
        ApplyGlassEffectRuntimeMode(hwnd);
        RefreshGlassEffectFrame(hwnd);
        StartGlassEffectWarmupCapture(hwnd);
        ApplyGlassEffectAnimationTimer(hwnd);
    }
    ApplyActivityVeinAnimationTimer(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Affiche ou masque le widget selon son etat courant.
//
// Parametres :
// - hwnd : handle de la fenetre a basculer.
// ----------------------------------------------------------------------------
void WidgetApp::ToggleWidgetVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        user_hidden_widget_ = true;
        SetColorPanelOpen(hwnd, false);
        if (glass_effect_ != nullptr) {
            KillTimer(hwnd, kGlassEffectCaptureTimerId);
            KillTimer(hwnd, kGlassEffectAnimationTimerId);
            glass_effect_capture_interactive_ = false;
            glass_effect_capture_idle_probe_ = false;
            glass_effect_warmup_capture_attempts_ = 0;
            glass_effect_->Shutdown();
            glass_effect_.reset();
        }
        KillTimer(hwnd, kActivityVeinAnimationTimerId);
        ClearGraphHover(hwnd);
        ClearTrayHideButtonInteraction(hwnd);
        ClearActivityVeinHover(hwnd);
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    user_hidden_widget_ = false;
    hidden_by_fullscreen_ = false;
    ShowWidget(hwnd);
}

// ----------------------------------------------------------------------------
// Indique si une fenetre couvre entierement son moniteur.
//
// Parametres :
// - window : handle de la fenetre a tester.
// - widget_hwnd : handle du widget a ignorer.
//
// Retour :
// - true si la fenetre au premier plan ressemble a un plein ecran.
// - false sinon.
// ----------------------------------------------------------------------------
bool WidgetApp::IsFullscreenWindow(HWND window, HWND widget_hwnd) const {
    if (window == nullptr || window == widget_hwnd || window == GetShellWindow()) {
        return false;
    }

    if (!IsWindowVisible(window) || IsIconic(window)) {
        return false;
    }

    RECT window_rect{};
    if (!GetWindowRect(window, &window_rect)) {
        return false;
    }

    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
        return false;
    }

    const RECT monitor_rect = monitor_info.rcMonitor;
    return window_rect.left <= monitor_rect.left
        && window_rect.top <= monitor_rect.top
        && window_rect.right >= monitor_rect.right
        && window_rect.bottom >= monitor_rect.bottom;
}

// ----------------------------------------------------------------------------
// Met a jour la visibilite automatique liee aux applications plein ecran.
//
// Parametres :
// - hwnd : handle de la fenetre du widget.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateFullscreenVisibility(HWND hwnd) {
    if (!app_settings_.hide_when_fullscreen) {
        if (hidden_by_fullscreen_) {
            hidden_by_fullscreen_ = false;
            if (!user_hidden_widget_) {
                ShowWidget(hwnd);
            }
        }

        return;
    }

    const bool fullscreen_active = IsFullscreenWindow(GetForegroundWindow(), hwnd);
    if (fullscreen_active && !hidden_by_fullscreen_ && IsWindowVisible(hwnd)) {
        hidden_by_fullscreen_ = true;
        SetColorPanelOpen(hwnd, false);
        KillTimer(hwnd, kGlassEffectCaptureTimerId);
        KillTimer(hwnd, kGlassEffectAnimationTimerId);
        KillTimer(hwnd, kActivityVeinAnimationTimerId);
        glass_effect_capture_interactive_ = false;
        glass_effect_capture_idle_probe_ = false;
        glass_effect_warmup_capture_attempts_ = 0;
        ClearGraphHover(hwnd);
        ClearTrayHideButtonInteraction(hwnd);
        ClearActivityVeinHover(hwnd);
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    if (!fullscreen_active && hidden_by_fullscreen_) {
        hidden_by_fullscreen_ = false;
        if (!user_hidden_widget_) {
            ShowWidget(hwnd);
            ApplyGlassEffectAnimationTimer(hwnd);
        }
    }
}

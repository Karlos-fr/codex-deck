// ============================================================================
// Codex Glass - Implementation des interactions du panneau couleurs
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp la conversion des hit-tests couleur en
// actions applicatives : fermeture, aleatoire, selection via color picker.
// ============================================================================

#include "WidgetColorInteraction.h"

#include "../color/WidgetColorPanel.h"
#include "../color/WidgetColorTools.h"
#include "../window/WidgetColorPanelWindow.h"
#include "../window/WidgetWindow.h"

#include <windowsx.h>

namespace {

// DPI de reference Win32 utilise pour convertir les pixels physiques en DIP.
constexpr float kColorInteractionReferenceDpi = 96.0F;

}

// ----------------------------------------------------------------------------
// Teste un point client dans l'extension laterale de couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - client_point : position souris en coordonnees client.
//
// Retour :
// - action detectee ou None.
// ----------------------------------------------------------------------------
WidgetColorPanelHitTestResult WidgetApp::HitTestColorPanel(HWND hwnd, POINT client_point) const {
    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);
    const float dpi_scale = static_cast<float>(GetDpiForWindow(hwnd)) / kColorInteractionReferenceDpi;
    const float safe_dpi_scale = dpi_scale > 0.0F ? dpi_scale : 1.0F;
    const POINT dip_point{
        static_cast<LONG>(static_cast<float>(client_point.x) / safe_dpi_scale),
        static_cast<LONG>(static_cast<float>(client_point.y) / safe_dpi_scale),
    };
    const WidgetColorPanelLayout layout = BuildWidgetColorPanelLayout(D2D1::SizeF(
        static_cast<float>(client_rect.right - client_rect.left) / safe_dpi_scale,
        static_cast<float>(client_rect.bottom - client_rect.top) / safe_dpi_scale
    ));

    return HitTestWidgetColorPanel(layout, dip_point);
}

// ----------------------------------------------------------------------------
// Execute une action de l'extension laterale de couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - action : action a executer.
// ----------------------------------------------------------------------------
void WidgetApp::ExecuteColorPanelAction(HWND hwnd, const WidgetColorPanelHitTestResult& action) {
    switch (action.action) {
    case WidgetColorPanelActionType::Close:
        SetColorPanelOpen(hwnd, false);
        return;

    case WidgetColorPanelActionType::Randomize:
        ApplyWidgetColors(hwnd, GenerateRandomWidgetColors(), false);
        return;

    case WidgetColorPanelActionType::PickColor: {
        COLORREF color = GetWidgetColorField(app_settings_.colors, action.field);
        const bool color_chosen = ChooseWidgetColor(hwnd, color);
        if (color_chosen) {
            SetSingleWidgetColor(hwnd, action.field, color);
        }
        if (color_panel_open_) {
            PositionColorPanelWindow();
            ShowWindow(color_panel_hwnd_, SW_SHOWNOACTIVATE);
        }
        return;
    }

    case WidgetColorPanelActionType::None:
    default:
        return;
    }
}

// ----------------------------------------------------------------------------
// Met a jour l'element survole du panneau couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - client_point : position souris en coordonnees client.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateColorPanelHover(HWND hwnd, POINT client_point) {
    const WidgetColorPanelHitTestResult hovered = HitTestColorPanel(hwnd, client_point);
    if (!IsSameWidgetColorPanelHit(color_panel_interaction_.hovered, hovered)) {
        color_panel_interaction_.hovered = hovered;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ----------------------------------------------------------------------------
// Reinitialise les etats souris du panneau couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ClearColorPanelInteraction(HWND hwnd) {
    if (color_panel_interaction_.hovered.action == WidgetColorPanelActionType::None
        && color_panel_interaction_.pressed.action == WidgetColorPanelActionType::None) {
        return;
    }

    color_panel_interaction_ = WidgetColorPanelInteraction{};
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Traite les messages Win32 propres a la palette flottante de couleurs.
//
// Parametres :
// - hwnd : handle de la palette.
// - message : message Win32 recu.
// - wparam : premier parametre du message.
// - lparam : second parametre du message.
//
// Retour :
// - valeur attendue par Win32 pour le message traite.
// ----------------------------------------------------------------------------
LRESULT WidgetApp::HandleColorPanelWindowMessage(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        const WidgetGlassEffectFrame* glass_effect_frame = glass_effect_ != nullptr
            ? glass_effect_->LatestCompanionFrame()
            : nullptr;
        color_panel_renderer_.RenderColorPanelWindow(
            hwnd,
            app_settings_,
            color_panel_interaction_,
            glass_effect_frame,
            vibration_glass_effect_state_
        );
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_SIZE:
        color_panel_renderer_.Resize(hwnd);
        RefreshColorPanelGlassEffectFrame();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED:
        ApplyDpiSuggestedRect(hwnd, reinterpret_cast<const RECT*>(lparam));
        PositionColorPanelWindow();
        RefreshColorPanelGlassEffectFrame();
        return 0;

    case WM_SETTINGCHANGE:
        ApplyNativeDarkMode(hwnd);
        color_panel_renderer_.RefreshVisualResources(hwnd);
        return 0;

    case WM_LBUTTONDOWN: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const WidgetColorPanelHitTestResult hit = HitTestColorPanel(hwnd, point);
        if (hit.action != WidgetColorPanelActionType::None) {
            color_panel_interaction_.pressed = hit;
            color_panel_interaction_.hovered = hit;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (main_hwnd_ != nullptr && !app_settings_.lock_position) {
            POINT screen_point = point;
            ClientToScreen(hwnd, &screen_point);
            ReleaseCapture();
            SendMessageW(
                main_hwnd_,
                WM_NCLBUTTONDOWN,
                HTCAPTION,
                MAKELPARAM(screen_point.x, screen_point.y)
            );
            PositionColorPanelWindow();
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const WidgetColorPanelHitTestResult released = HitTestColorPanel(hwnd, point);
        const WidgetColorPanelHitTestResult pressed = color_panel_interaction_.pressed;
        color_panel_interaction_.pressed = WidgetColorPanelHitTestResult{};
        color_panel_interaction_.hovered = released;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        if (IsSameWidgetColorPanelHit(pressed, released)) {
            ExecuteColorPanelAction(main_hwnd_, released);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        UpdateColorPanelHover(hwnd, point);
        TRACKMOUSEEVENT track_mouse{};
        track_mouse.cbSize = sizeof(track_mouse);
        track_mouse.dwFlags = TME_LEAVE;
        track_mouse.hwndTrack = hwnd;
        TrackMouseEvent(&track_mouse);
        return 0;
    }

    case WM_MOUSELEAVE:
        ClearColorPanelInteraction(hwnd);
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            SetColorPanelOpen(main_hwnd_, false);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_CLOSE:
        SetColorPanelOpen(main_hwnd_, false);
        return 0;

    case WM_NCDESTROY:
        if (glass_effect_) {
            glass_effect_->SetCompanionWindow(nullptr);
        }
        color_panel_renderer_.DiscardDeviceResources();
        color_panel_hwnd_ = nullptr;
        color_panel_open_ = false;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hwnd, message, wparam, lparam);

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

// ============================================================================
// Codex Glass - Implementation du controleur du bouton tray
// ----------------------------------------------------------------------------
// Ce fichier convertit les coordonnees Win32, gere le survol et relie le petit
// bouton de bandeau a la logique existante de visibilite de WidgetApp.
// ============================================================================

#include "WidgetApp.h"

#include "../rendering/WidgetRenderConstants.h"
#include "../rendering/WidgetTrayHideButton.h"

namespace {

// Convertit une coordonnee physique Win32 en DIPs.
//
// Parametres :
// - hwnd : fenetre qui fournit son DPI courant.
// - value : coordonnee exprimee en pixels physiques.
//
// Retour :
// - coordonnee exprimee en DIPs.
float TrayButtonPixelsToDips(HWND hwnd, int value) {
    const UINT dpi = GetDpiForWindow(hwnd);
    const float active_dpi = dpi == 0 ? kReferenceDpi : static_cast<float>(dpi);
    return static_cast<float>(value) * kReferenceDpi / active_dpi;
}

// Construit la geometrie courante du bouton depuis la fenetre principale.
//
// Parametres :
// - hwnd : fenetre dont la zone cliente est mesuree.
// - display_mode : mode d'affichage qui determine les marges.
//
// Retour :
// - geometrie Direct2D du bouton et de son hint.
WidgetTrayHideButtonLayout CurrentTrayHideButtonLayout(
    HWND hwnd,
    WidgetDisplayMode display_mode
) {
    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);
    return BuildWidgetTrayHideButtonLayout(
        D2D1::SizeF(
            TrayButtonPixelsToDips(hwnd, client_rect.right - client_rect.left),
            TrayButtonPixelsToDips(hwnd, client_rect.bottom - client_rect.top)
        ),
        display_mode,
        0.0F
    );
}

// Convertit un point client physique vers le repere Direct2D.
//
// Parametres :
// - hwnd : fenetre qui fournit son DPI courant.
// - point : position client exprimee en pixels physiques.
//
// Retour :
// - position cliente exprimee en DIPs.
D2D1_POINT_2F TrayButtonPointToDips(HWND hwnd, POINT point) {
    return D2D1::Point2F(
        TrayButtonPixelsToDips(hwnd, point.x),
        TrayButtonPixelsToDips(hwnd, point.y)
    );
}

} // namespace

// Indique si un point client cible le bouton de masquage dans le tray.
bool WidgetApp::IsTrayHideButtonPoint(HWND hwnd, POINT client_point) const {
    if (app_settings_.click_through) {
        return false;
    }
    return HitTestWidgetTrayHideButton(
        CurrentTrayHideButtonLayout(hwnd, app_settings_.display_mode),
        TrayButtonPointToDips(hwnd, client_point)
    );
}

// Met a jour le survol du bouton et arme WM_MOUSELEAVE.
void WidgetApp::UpdateTrayHideButtonHover(HWND hwnd, POINT client_point) {
    const bool hovered = IsTrayHideButtonPoint(hwnd, client_point);
    if (tray_hide_button_interaction_.hovered != hovered) {
        tray_hide_button_interaction_.hovered = hovered;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    if (hovered) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
    }
    if (!tray_hide_button_interaction_.tracking_mouse_leave) {
        TRACKMOUSEEVENT tracking{
            sizeof(tracking),
            TME_LEAVE,
            hwnd,
            0,
        };
        if (TrackMouseEvent(&tracking) != FALSE) {
            tray_hide_button_interaction_.tracking_mouse_leave = true;
        }
    }
}

// Efface les etats transitoires du bouton de masquage.
void WidgetApp::ClearTrayHideButtonInteraction(HWND hwnd) {
    const bool changed = tray_hide_button_interaction_.hovered
        || tray_hide_button_interaction_.pressed;
    if (tray_hide_button_interaction_.tracking_mouse_leave) {
        TRACKMOUSEEVENT tracking{
            sizeof(tracking),
            TME_CANCEL | TME_LEAVE,
            hwnd,
            0,
        };
        TrackMouseEvent(&tracking);
    }
    tray_hide_button_interaction_ = WidgetTrayHideButtonInteraction{};
    if (changed) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// Traite l'enfoncement du bouton de masquage.
bool WidgetApp::HandleTrayHideButtonClick(HWND hwnd, POINT client_point) {
    if (!IsTrayHideButtonPoint(hwnd, client_point)) {
        return false;
    }
    tray_hide_button_interaction_.hovered = true;
    tray_hide_button_interaction_.pressed = true;
    SetCapture(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

// Termine le clic et masque le widget si le pointeur cible encore le bouton.
bool WidgetApp::HandleTrayHideButtonRelease(HWND hwnd, POINT client_point) {
    if (!tray_hide_button_interaction_.pressed) {
        return false;
    }

    const bool activate = IsTrayHideButtonPoint(hwnd, client_point);
    tray_hide_button_interaction_.pressed = false;
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    if (activate) {
        ClearTrayHideButtonInteraction(hwnd);
        ToggleWidgetVisibility(hwnd);
        SaveCurrentSettings(hwnd);
    } else {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return true;
}

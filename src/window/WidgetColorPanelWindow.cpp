// ============================================================================
// Codex Glass - Implementation de la fenetre outil du panneau de couleurs
// ----------------------------------------------------------------------------
// Ce fichier gere la classe Win32, la taille DPI et l'ancrage multi-ecran de la
// palette. Il ne connait ni les reglages couleur, ni Direct2D.
// ============================================================================

#include "WidgetColorPanelWindow.h"

#include "../color/WidgetColorPanel.h"

#include <dwmapi.h>

#include <algorithm>

namespace {

// Nom interne de la classe Win32 de la palette.
constexpr wchar_t kWidgetColorPanelWindowClassName[] = L"CodexGlassColorPanelWindowClass";

// DPI logique de reference des dimensions de la palette.
constexpr int kWidgetColorPanelReferenceDpi = 96;

// Espace logique nul afin de coller la palette au widget.
constexpr int kWidgetColorPanelGap = 0;

// Preference DWM identique a celle de la fenetre principale.
constexpr DWM_WINDOW_CORNER_PREFERENCE kWidgetColorPanelCornerPreference = DWMWCP_ROUND;

// ----------------------------------------------------------------------------
// Convertit une dimension logique en pixels physiques.
//
// Parametres :
// - logical_size : dimension exprimee pour le DPI de reference.
// - dpi : DPI physique cible.
//
// Retour :
// - dimension physique arrondie en pixels.
// ----------------------------------------------------------------------------
int ScaleColorPanelSize(int logical_size, UINT dpi) {
    return MulDiv(logical_size, static_cast<int>(dpi), kWidgetColorPanelReferenceDpi);
}

} // namespace

// ----------------------------------------------------------------------------
// Enregistre la classe Win32 de la palette de couleurs.
//
// Parametres :
// - instance : instance qui possede la classe.
// - window_proc : procedure de fenetre de la palette.
//
// Retour :
// - true si la classe est disponible.
// ----------------------------------------------------------------------------
bool RegisterWidgetColorPanelWindowClass(HINSTANCE instance, WNDPROC window_proc) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWidgetColorPanelWindowClassName;

    if (RegisterClassExW(&window_class) != 0) {
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

// ----------------------------------------------------------------------------
// Cree la palette de couleurs comme fenetre outil possedee par le widget.
//
// Parametres :
// - instance : instance de l'application.
// - owner : widget proprietaire.
// - create_parameter : pointeur transmis a WM_NCCREATE.
//
// Retour :
// - handle cree, ou nullptr en cas d'echec.
// ----------------------------------------------------------------------------
HWND CreateWidgetColorPanelWindow(HINSTANCE instance, HWND owner, void* create_parameter) {
    const UINT dpi = owner != nullptr ? GetDpiForWindow(owner) : GetDpiForSystem();
    HWND panel = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kWidgetColorPanelWindowClassName,
        L"Codex Glass - Couleurs",
        WS_POPUP,
        0,
        0,
        ScaleColorPanelSize(kWidgetColorPanelWidth, dpi),
        ScaleColorPanelSize(kWidgetColorPanelWindowHeight, dpi),
        owner,
        nullptr,
        instance,
        create_parameter
    );
    if (panel != nullptr) {
        DwmSetWindowAttribute(
            panel,
            DWMWA_WINDOW_CORNER_PREFERENCE,
            &kWidgetColorPanelCornerPreference,
            sizeof(kWidgetColorPanelCornerPreference)
        );
    }
    return panel;
}

// ----------------------------------------------------------------------------
// Ancre la palette a droite du widget, ou a gauche si la place manque.
//
// Parametres :
// - panel_hwnd : palette a positionner.
// - owner_hwnd : widget servant d'ancre et de reference DPI.
// ----------------------------------------------------------------------------
void PositionWidgetColorPanelWindow(HWND panel_hwnd, HWND owner_hwnd) {
    if (panel_hwnd == nullptr || owner_hwnd == nullptr) {
        return;
    }

    RECT owner_rect{};
    if (!GetWindowRect(owner_hwnd, &owner_rect)) {
        return;
    }

    PositionWidgetColorPanelWindowForRect(panel_hwnd, owner_hwnd, owner_rect);
}

// ----------------------------------------------------------------------------
// Ancre la palette sur un rectangle futur du widget pendant son deplacement.
//
// Parametres :
// - panel_hwnd : palette a positionner.
// - owner_hwnd : widget servant de reference DPI et de z-order.
// - owner_rect : rectangle ecran propose pour le widget.
// ----------------------------------------------------------------------------
void PositionWidgetColorPanelWindowForRect(HWND panel_hwnd, HWND owner_hwnd, const RECT& owner_rect) {
    if (panel_hwnd == nullptr || owner_hwnd == nullptr) {
        return;
    }

    const HMONITOR monitor = MonitorFromRect(&owner_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) {
        return;
    }

    const UINT dpi = GetDpiForWindow(owner_hwnd);
    const int width = ScaleColorPanelSize(kWidgetColorPanelWidth, dpi);
    const int height = ScaleColorPanelSize(kWidgetColorPanelWindowHeight, dpi);
    const int gap = ScaleColorPanelSize(kWidgetColorPanelGap, dpi);
    const RECT work_area = monitor_info.rcWork;

    int left = owner_rect.right + gap;
    if (left + width > work_area.right) {
        left = owner_rect.left - gap - width;
    }
    const int work_left = static_cast<int>(work_area.left);
    const int work_top = static_cast<int>(work_area.top);
    const int work_right = static_cast<int>(work_area.right);
    const int work_bottom = static_cast<int>(work_area.bottom);
    left = std::clamp(left, work_left, std::max(work_left, work_right - width));
    const int top = std::clamp(
        static_cast<int>(owner_rect.top),
        work_top,
        std::max(work_top, work_bottom - height)
    );
    const HWND z_order = (GetWindowLongPtrW(owner_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0
        ? HWND_TOPMOST
        : HWND_TOP;

    SetWindowPos(
        panel_hwnd,
        z_order,
        left,
        top,
        width,
        height,
        SWP_NOACTIVATE
    );
}

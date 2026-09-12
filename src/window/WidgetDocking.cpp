// ============================================================================
// Codex Glass - Implementation de l'aimantation aux bords d'ecran
// ----------------------------------------------------------------------------
// Ce fichier isole le comportement de docking magnetique de la fenetre.
// ============================================================================

#include "WidgetDocking.h"

#include <cstdlib>

namespace {

// Distance maximale en pixels pour declencher l'aimantation a un bord.
constexpr int kDockEdgeThreshold = 8;

// Distance maximale en pixels pour declencher le centrage sur un bord.
constexpr int kDockCenterThreshold = 24;

// ----------------------------------------------------------------------------
// Replace un rectangle en conservant sa taille.
// ----------------------------------------------------------------------------
void MoveRectTo(RECT& rect, int left, int top) {
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    rect.left = left;
    rect.top = top;
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
}

// ----------------------------------------------------------------------------
// Deplace horizontalement un rectangle en conservant sa taille.
// ----------------------------------------------------------------------------
void MoveRectLeftTo(RECT& rect, int left) {
    MoveRectTo(rect, left, rect.top);
}

// ----------------------------------------------------------------------------
// Deplace verticalement un rectangle en conservant sa taille.
// ----------------------------------------------------------------------------
void MoveRectTopTo(RECT& rect, int top) {
    MoveRectTo(rect, rect.left, top);
}

// ----------------------------------------------------------------------------
// Retourne true si deux valeurs sont suffisamment proches.
// ----------------------------------------------------------------------------
bool IsNear(int value, int target, int threshold) {
    return std::abs(value - target) <= threshold;
}

// ----------------------------------------------------------------------------
// Regroupe les zones utiles de l'ecran cible.
struct MonitorDockAreas {
    RECT work_area{};
    RECT monitor_area{};
};

// ----------------------------------------------------------------------------
// Recupere les zones de l'ecran le plus proche d'un rectangle.
// ----------------------------------------------------------------------------
bool TryGetNearestMonitorDockAreas(const RECT& window_rect, MonitorDockAreas& dock_areas) {
    const HMONITOR monitor = MonitorFromRect(&window_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitor_info)) {
        return false;
    }

    dock_areas.work_area = monitor_info.rcWork;
    dock_areas.monitor_area = monitor_info.rcMonitor;
    return true;
}

}  // namespace

// ----------------------------------------------------------------------------
// Ajuste un rectangle de deplacement pour l'aimanter aux bords de l'ecran.
//
// Parametres :
// - window_rect : rectangle de fenetre en coordonnees ecran, modifie sur place.
// ----------------------------------------------------------------------------
void DockWindowRectToScreenEdges(RECT& window_rect) {
    const int width = window_rect.right - window_rect.left;
    const int height = window_rect.bottom - window_rect.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    MonitorDockAreas dock_areas{};
    if (!TryGetNearestMonitorDockAreas(window_rect, dock_areas)) {
        return;
    }

    const RECT work_area = dock_areas.work_area;
    const RECT monitor_area = dock_areas.monitor_area;
    const int work_center_x = work_area.left + ((work_area.right - work_area.left) / 2);
    const int work_center_y = work_area.top + ((work_area.bottom - work_area.top) / 2);

    bool docked_left = false;
    bool docked_right = false;
    bool docked_top = false;
    bool docked_bottom = false;

    if (IsNear(window_rect.left, work_area.left, kDockEdgeThreshold)) {
        MoveRectLeftTo(window_rect, work_area.left);
        docked_left = true;
    } else if (IsNear(window_rect.right, work_area.right, kDockEdgeThreshold)) {
        MoveRectLeftTo(window_rect, work_area.right - width);
        docked_right = true;
    }

    if (IsNear(window_rect.top, work_area.top, kDockEdgeThreshold)) {
        MoveRectTopTo(window_rect, work_area.top);
        docked_top = true;
    } else if (IsNear(window_rect.bottom, work_area.bottom, kDockEdgeThreshold)) {
        MoveRectTopTo(window_rect, work_area.bottom - height);
        docked_bottom = true;
    } else if (IsNear(window_rect.bottom, monitor_area.bottom, kDockEdgeThreshold)) {
        MoveRectTopTo(window_rect, monitor_area.bottom - height);
        docked_bottom = true;
    }

    const int rect_center_x = window_rect.left + (width / 2);
    const int rect_center_y = window_rect.top + (height / 2);

    if ((docked_top || docked_bottom) && IsNear(rect_center_x, work_center_x, kDockCenterThreshold)) {
        MoveRectLeftTo(window_rect, work_center_x - (width / 2));
    }

    if ((docked_left || docked_right) && IsNear(rect_center_y, work_center_y, kDockCenterThreshold)) {
        MoveRectTopTo(window_rect, work_center_y - (height / 2));
    }
}

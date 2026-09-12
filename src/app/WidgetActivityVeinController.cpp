// ============================================================================
// Codex Glass - Controleur de survol de la veine d'activite
// ----------------------------------------------------------------------------
// Ce fichier convertit les coordonnees Win32 et maintient uniquement le survol
// global du filament. Il ne cible ni n'expose aucune session individuelle.
// ============================================================================

#include "WidgetApp.h"

#include "../rendering/WidgetRenderActivityVein.h"
#include "../rendering/WidgetRenderConstants.h"

namespace {

// ----------------------------------------------------------------------------
// Convertit une coordonnee physique Win32 en DIPs.
//
// Parametres :
// - hwnd : fenetre qui fournit son DPI courant.
// - value : coordonnee exprimee en pixels physiques.
//
// Retour :
// - coordonnee exprimee en DIPs.
// ----------------------------------------------------------------------------
float ActivityVeinPixelsToDips(HWND hwnd, int value) {
    const UINT dpi = GetDpiForWindow(hwnd);
    const float active_dpi = dpi == 0 ? kReferenceDpi : static_cast<float>(dpi);
    return static_cast<float>(value) * kReferenceDpi / active_dpi;
}

// ----------------------------------------------------------------------------
// Construit la geometrie courante de la veine depuis la fenetre principale.
//
// Parametres :
// - hwnd : fenetre dont la zone cliente est mesuree.
// - display_mode : mode qui selectionne le placement disponible.
//
// Retour :
// - geometrie Direct2D utilisee uniquement pour le hit-test.
// ----------------------------------------------------------------------------
WidgetActivityVeinLayout CurrentActivityVeinLayout(
    HWND hwnd,
    WidgetDisplayMode display_mode
) {
    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);
    return BuildWidgetActivityVeinLayout(
        D2D1::SizeF(
            ActivityVeinPixelsToDips(hwnd, client_rect.right - client_rect.left),
            ActivityVeinPixelsToDips(hwnd, client_rect.bottom - client_rect.top)
        ),
        display_mode,
        0.0F
    );
}

// ----------------------------------------------------------------------------
// Convertit un point client physique vers le repere Direct2D.
//
// Parametres :
// - hwnd : fenetre qui fournit son DPI courant.
// - point : position client exprimee en pixels physiques.
//
// Retour :
// - position cliente exprimee en DIPs.
// ----------------------------------------------------------------------------
D2D1_POINT_2F ActivityVeinPointToDips(HWND hwnd, POINT point) {
    return D2D1::Point2F(
        ActivityVeinPixelsToDips(hwnd, point.x),
        ActivityVeinPixelsToDips(hwnd, point.y)
    );
}

} // namespace

// ----------------------------------------------------------------------------
// Indique si le point client appartient a la veine d'activite.
// ----------------------------------------------------------------------------
bool WidgetApp::IsActivityVeinPoint(HWND hwnd, POINT client_point) const {
    if (app_settings_.click_through) {
        return false;
    }
    return HitTestWidgetActivityVein(
        CurrentActivityVeinLayout(hwnd, app_settings_.display_mode),
        ActivityVeinPointToDips(hwnd, client_point)
    );
}

// ----------------------------------------------------------------------------
// Met a jour le survol global de la veine d'activite.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateActivityVeinHover(HWND hwnd, POINT client_point) {
    const bool hovered = IsActivityVeinPoint(hwnd, client_point);
    if (activity_vein_interaction_.hovered == hovered) {
        return;
    }
    activity_vein_interaction_.hovered = hovered;
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Efface le survol global de la veine d'activite.
// ----------------------------------------------------------------------------
void WidgetApp::ClearActivityVeinHover(HWND hwnd) {
    if (!activity_vein_interaction_.hovered) {
        return;
    }
    activity_vein_interaction_.hovered = false;
    InvalidateRect(hwnd, nullptr, FALSE);
}

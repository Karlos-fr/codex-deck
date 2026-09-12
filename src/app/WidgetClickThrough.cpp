// ============================================================================
// Codex Glass - Implementation du click-through
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp la logique qui decide si la souris traverse
// le widget ou si le titre reste utilisable pour deplacer la fenetre.
// ============================================================================

#include "WidgetClickThrough.h"

#include <algorithm>

namespace {

// DPI de reference utilise par les dimensions du renderer.
constexpr float kReferenceDpi = 96.0F;

// Marge interne du widget hors mode minimal en DIPs.
constexpr float kHitTestPanelPadding = 18.0F;

// Marge interne du widget en mode minimal en DIPs.
constexpr float kHitTestMinimalPanelPadding = 14.0F;

// Position verticale du titre hors mode minimal en DIPs.
constexpr float kHitTestTitleTop = 12.0F;

// Position verticale du titre en mode minimal en DIPs.
constexpr float kHitTestMinimalTitleTop = 14.0F;

// Hauteur de la zone de titre hors mode minimal en DIPs.
constexpr float kHitTestTitleHeight = 28.0F;

// Hauteur de la zone de titre en mode minimal en DIPs.
constexpr float kHitTestMinimalTitleHeight = 24.0F;

} // namespace

// ----------------------------------------------------------------------------
// Indique si un point client touche le titre utilisable en click-through.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - client_point : point a tester en coordonnees client physiques.
//
// Retour :
// - true si le point doit permettre de deplacer le widget.
// ----------------------------------------------------------------------------
bool WidgetApp::IsClickThroughTitleDragHandle(HWND hwnd, POINT client_point) const {
    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);

    const float dpi_scale = static_cast<float>(GetDpiForWindow(hwnd)) / kReferenceDpi;
    const bool condensed_mode = app_settings_.display_mode == WidgetDisplayMode::Minimal
        || app_settings_.display_mode == WidgetDisplayMode::Horizontal
        || app_settings_.display_mode == WidgetDisplayMode::Vertical;
    const float padding = (condensed_mode ? kHitTestMinimalPanelPadding : kHitTestPanelPadding) * dpi_scale;
    const float title_top = (condensed_mode ? kHitTestMinimalTitleTop : kHitTestTitleTop) * dpi_scale;
    const float title_height = (condensed_mode ? kHitTestMinimalTitleHeight : kHitTestTitleHeight) * dpi_scale;
    const RECT title_rect{
        static_cast<LONG>(padding),
        static_cast<LONG>(title_top),
        static_cast<LONG>(std::max(padding, static_cast<float>(client_rect.right) - padding)),
        static_cast<LONG>(title_top + title_height),
    };

    return PtInRect(&title_rect, client_point) != FALSE;
}

// ----------------------------------------------------------------------------
// Applique le vrai passage souris selon la position du curseur.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
//
// Retour :
// - true si le curseur est dans une zone traversante.
// ----------------------------------------------------------------------------
bool WidgetApp::ApplyClickThroughTransparency(HWND hwnd) {
    LONG_PTR extended_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR previous_style = extended_style;

    bool transparent_to_mouse = app_settings_.click_through;
    bool pass_through_under_cursor = false;
    if (transparent_to_mouse) {
        POINT cursor{};
        if (GetCursorPos(&cursor)) {
            POINT client_point = cursor;
            ScreenToClient(hwnd, &client_point);

            RECT client_rect{};
            GetClientRect(hwnd, &client_rect);
            const bool cursor_in_client = PtInRect(&client_rect, client_point) != FALSE;
            const bool title_drag_handle = cursor_in_client
                && !app_settings_.lock_position
                && IsClickThroughTitleDragHandle(hwnd, client_point);
            if (title_drag_handle) {
                transparent_to_mouse = false;
            }
            pass_through_under_cursor = cursor_in_client && !title_drag_handle;
        }
    }

    if (transparent_to_mouse) {
        extended_style |= WS_EX_TRANSPARENT;
    } else {
        extended_style &= ~WS_EX_TRANSPARENT;
    }

    if (extended_style != previous_style) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, extended_style);
        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
        );
    }

    return pass_through_under_cursor;
}

// ----------------------------------------------------------------------------
// Recapture GlassEffect apres un clic transmis derriere le widget.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// - pass_through_under_cursor : true si le curseur est dans une zone traversante.
// ----------------------------------------------------------------------------
void WidgetApp::RefreshGlassEffectAfterClickThroughMouseEvent(HWND hwnd, bool pass_through_under_cursor) {
    const bool left_button_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool right_button_down = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool middle_button_down = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

    const bool new_pass_through_press = pass_through_under_cursor
        && ((left_button_down && !click_through_left_button_down_)
            || (right_button_down && !click_through_right_button_down_)
            || (middle_button_down && !click_through_middle_button_down_));

    click_through_left_button_down_ = left_button_down;
    click_through_right_button_down_ = right_button_down;
    click_through_middle_button_down_ = middle_button_down;

    if (!new_pass_through_press || glass_effect_ == nullptr) {
        return;
    }

    RefreshGlassEffectFrame(hwnd);
    StartGlassEffectWarmupCapture(hwnd);
}

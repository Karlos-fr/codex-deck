// ============================================================================
// Codex Glass - Implementation des sliders de menu
// ----------------------------------------------------------------------------
// Ce fichier dessine des items owner-drawn reutilisables dans les menus Win32
// et suit la souris pendant TrackPopupMenu avec un hook local au thread UI.
// ============================================================================

#include "WidgetMenuSlider.h"

#include "../theme/Theme.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

// Largeur de l'item owner-drawn en pixels.
constexpr UINT kSliderItemWidth = 232;

// Hauteur de l'item owner-drawn en pixels.
constexpr UINT kSliderItemHeight = 38;

// Marge horizontale du contenu du slider.
constexpr int kSliderHorizontalPadding = 12;

// Position verticale du rail dans l'item.
constexpr int kSliderTrackTop = 25;

// Hauteur du rail du slider.
constexpr int kSliderTrackHeight = 4;

// Rayon visuel du curseur.
constexpr int kSliderThumbRadius = 6;

// Couleur du fond sombre natif approximatif des menus Windows 11.
constexpr COLORREF kDarkMenuBackgroundColor = RGB(0x2B, 0x2B, 0x2B);

// Couleur du fond sombre natif approximatif d'un item de menu survole.
constexpr COLORREF kDarkMenuHotBackgroundColor = RGB(0x3A, 0x3A, 0x3A);

// Couleur du texte principal en theme sombre.
constexpr COLORREF kDarkMenuTextColor = RGB(0xF3, 0xF3, 0xF3);

// Couleur du texte secondaire en theme sombre.
constexpr COLORREF kDarkMenuMutedTextColor = RGB(0xC8, 0xC8, 0xC8);

// Couleur neutre du rail d'un slider au repos.
constexpr COLORREF kSliderTrackColor = RGB(0x68, 0x68, 0x72);

// Couleur legerement eclaircie du rail d'un slider survole.
constexpr COLORREF kSliderHotTrackColor = RGB(0x82, 0x82, 0x8C);

// Message interne Win32 qui retourne le HMENU d'une fenetre de menu popup.
constexpr UINT kMenuGetHandleMessage = 0x01E1;

// Etat d'un slider pendant l'affichage du menu.
struct MenuSliderState {
    WidgetMenuSliderSpec spec{};
    HWND menu_hwnd = nullptr;
    RECT item_rect_client{};
    RECT item_rect_screen{};
};

// Etat global des sliders pendant l'affichage du menu.
struct MenuSlidersState {
    HWND hwnd = nullptr;
    HHOOK mouse_hook = nullptr;
    UINT dragging_command_id = 0;
    UINT hovered_command_id = 0;
    COLORREF active_color = RGB(0x22, 0xC5, 0x5E);
    std::vector<MenuSliderState> sliders{};
};

MenuSlidersState g_sliders{};

// ----------------------------------------------------------------------------
// Melange une couleur avec du blanc pour produire un survol visible mais doux.
//
// Parametres :
// - color : couleur d'origine.
// - white_percent : proportion de blanc entre 0 et 100.
//
// Retour :
// - couleur eclaircie.
// ----------------------------------------------------------------------------
COLORREF LightenControlColor(COLORREF color, int white_percent) {
    const int source_percent = 100 - white_percent;
    return RGB(
        ((GetRValue(color) * source_percent) + (0xFF * white_percent)) / 100,
        ((GetGValue(color) * source_percent) + (0xFF * white_percent)) / 100,
        ((GetBValue(color) * source_percent) + (0xFF * white_percent)) / 100
    );
}

// ----------------------------------------------------------------------------
// Recherche l'etat d'un slider par commande.
//
// Parametres :
// - command_id : commande recherchee.
//
// Retour :
// - etat du slider, ou nullptr si absent.
// ----------------------------------------------------------------------------
MenuSliderState* FindSlider(UINT command_id) {
    for (MenuSliderState& slider : g_sliders.sliders) {
        if (slider.spec.command_id == command_id) {
            return &slider;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche l'etat d'un slider depuis un pointeur stocke dans un item Win32.
//
// Parametres :
// - item_data : donnees associees a l'item owner-drawn.
//
// Retour :
// - etat du slider, ou nullptr si les donnees ne pointent pas vers un slider actif.
// ----------------------------------------------------------------------------
MenuSliderState* FindSliderByItemData(ULONG_PTR item_data) {
    if (item_data == 0) {
        return nullptr;
    }

    for (MenuSliderState& slider : g_sliders.sliders) {
        if (reinterpret_cast<ULONG_PTR>(&slider) == item_data) {
            return &slider;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche l'etat d'un slider depuis les donnees transmises par Win32.
//
// Parametres :
// - item_id : identifiant de l'item fourni par WM_MEASUREITEM/WM_DRAWITEM.
// - item_data : donnees associees a l'item owner-drawn.
//
// Retour :
// - etat du slider, ou nullptr si l'item ne correspond pas a un slider.
// ----------------------------------------------------------------------------
MenuSliderState* FindSliderFromMenuItem(UINT item_id, ULONG_PTR item_data) {
    if (MenuSliderState* slider = FindSliderByItemData(item_data)) {
        return slider;
    }
    if (MenuSliderState* slider = FindSlider(item_id)) {
        return slider;
    }
    return FindSlider(static_cast<UINT>(item_data));
}

// ----------------------------------------------------------------------------
// Indique si la fenetre de menu affiche encore le slider donne.
//
// Parametres :
// - slider : slider dont le menu courant doit etre valide.
//
// Retour :
// - true si la fenetre est visible et contient toujours la commande.
// ----------------------------------------------------------------------------
bool IsSliderMenuItemVisible(const MenuSliderState& slider) {
    if (slider.menu_hwnd == nullptr || IsWindowVisible(slider.menu_hwnd) == FALSE) {
        return false;
    }

    const HMENU menu = reinterpret_cast<HMENU>(SendMessageW(
        slider.menu_hwnd,
        kMenuGetHandleMessage,
        0,
        0
    ));
    const int item_count = menu != nullptr ? GetMenuItemCount(menu) : 0;
    for (int position = 0; position < item_count; ++position) {
        if (GetMenuItemID(menu, position) == slider.spec.command_id) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Recherche le slider sous une position ecran.
//
// Parametres :
// - point : position ecran a tester.
//
// Retour :
// - etat du slider, ou nullptr si aucun slider ne contient la position.
// ----------------------------------------------------------------------------
MenuSliderState* FindSliderAtPoint(POINT point) {
    for (MenuSliderState& slider : g_sliders.sliders) {
        if (IsSliderMenuItemVisible(slider)
            && !IsRectEmpty(&slider.item_rect_screen)
            && PtInRect(&slider.item_rect_screen, point) != FALSE) {
            return &slider;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Encadre une valeur de slider.
//
// Parametres :
// - slider : slider concerne.
// - percent : valeur a encadrer.
//
// Retour :
// - valeur valide.
// ----------------------------------------------------------------------------
int ClampSliderValue(const MenuSliderState& slider, int value) {
    return std::clamp(value, slider.spec.minimum_value, slider.spec.maximum_value);
}

// ----------------------------------------------------------------------------
// Convertit une position souris ecran en pourcentage.
//
// Parametres :
// - slider : slider concerne.
// - x : position horizontale ecran.
//
// Retour :
// - pourcentage correspondant.
// ----------------------------------------------------------------------------
int SliderValueFromScreenX(const MenuSliderState& slider, int x) {
    const int track_left = slider.item_rect_screen.left + kSliderHorizontalPadding;
    const int track_right = slider.item_rect_screen.right - kSliderHorizontalPadding;
    const int track_width = std::max(1, track_right - track_left);
    const double ratio = static_cast<double>(std::clamp(x, track_left, track_right) - track_left)
        / static_cast<double>(track_width);
    return ClampSliderValue(
        slider,
        static_cast<int>(std::lround(
            slider.spec.minimum_value + (ratio * (slider.spec.maximum_value - slider.spec.minimum_value))
        ))
    );
}

// ----------------------------------------------------------------------------
// Redessine directement la zone ecran du slider.
//
// Parametres :
// - slider : slider a redessiner.
// ----------------------------------------------------------------------------
void RepaintSliderOnScreen(const MenuSliderState& slider) {
    if (slider.menu_hwnd == nullptr || IsRectEmpty(&slider.item_rect_client)) {
        return;
    }

    InvalidateRect(slider.menu_hwnd, &slider.item_rect_client, FALSE);
    UpdateWindow(slider.menu_hwnd);
}

// ----------------------------------------------------------------------------
// Met a jour le slider survole et redessine les items concernes.
//
// Parametres :
// - slider : slider actuellement sous la souris, ou nullptr.
// ----------------------------------------------------------------------------
void UpdateHoveredSlider(MenuSliderState* slider) {
    const UINT command_id = slider != nullptr ? slider->spec.command_id : 0;
    if (command_id == g_sliders.hovered_command_id) {
        return;
    }

    MenuSliderState* previous = FindSlider(g_sliders.hovered_command_id);
    g_sliders.hovered_command_id = command_id;
    if (previous != nullptr) {
        RepaintSliderOnScreen(*previous);
    }
    if (slider != nullptr) {
        RepaintSliderOnScreen(*slider);
    }
}

// ----------------------------------------------------------------------------
// Applique un pourcentage et notifie l'application.
//
// Parametres :
// - slider : slider concerne.
// - percent : valeur demandee.
// ----------------------------------------------------------------------------
void ApplySliderValue(MenuSliderState& slider, int value) {
    value = ClampSliderValue(slider, value);
    if (value == slider.spec.current_value) {
        return;
    }

    slider.spec.current_value = value;
    if (g_sliders.hwnd != nullptr && slider.spec.changed_message != 0) {
        SendMessageW(
            g_sliders.hwnd,
            slider.spec.changed_message,
            static_cast<WPARAM>(value),
            static_cast<LPARAM>(slider.spec.command_id)
        );
    }
    RepaintSliderOnScreen(slider);
}

// ----------------------------------------------------------------------------
// Dessine un slider dans un HDC.
//
// Parametres :
// - hdc : contexte GDI cible.
// - rect : rectangle de l'item.
// - slider : slider a dessiner.
// - selected : indique si l'item est survole par le menu.
// ----------------------------------------------------------------------------
void PaintSlider(HDC hdc, const RECT& rect, const MenuSliderState& slider, bool selected) {
    const bool dark_mode = IsSystemDarkTheme();
    const COLORREF background_color = dark_mode ? kDarkMenuBackgroundColor : GetSysColor(COLOR_MENU);
    const COLORREF selected_background_color = dark_mode ? kDarkMenuHotBackgroundColor : GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF text_color = dark_mode ? kDarkMenuTextColor : GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);
    const COLORREF muted_color = dark_mode ? kDarkMenuMutedTextColor : GetSysColor(COLOR_GRAYTEXT);
    const COLORREF track_color = selected ? kSliderHotTrackColor : kSliderTrackColor;
    const COLORREF fill_color = selected
        ? LightenControlColor(g_sliders.active_color, 24)
        : g_sliders.active_color;

    HBRUSH background_brush = CreateSolidBrush(selected ? selected_background_color : background_color);
    FillRect(hdc, &rect, background_brush);
    DeleteObject(background_brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text_color);

    RECT label_rect{rect.left + kSliderHorizontalPadding, rect.top + 4, rect.right - 82, rect.top + 20};
    DrawTextW(hdc, slider.spec.label.c_str(), -1, &label_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    wchar_t value_label[24]{};
    wsprintfW(value_label, L"%d%s", slider.spec.current_value, slider.spec.value_suffix);
    SetTextColor(hdc, muted_color);
    RECT value_rect{rect.right - 78, rect.top + 4, rect.right - kSliderHorizontalPadding, rect.top + 20};
    DrawTextW(hdc, value_label, -1, &value_rect, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    const int track_left = rect.left + kSliderHorizontalPadding;
    const int track_right = rect.right - kSliderHorizontalPadding;
    const int track_top = rect.top + kSliderTrackTop;
    const int track_bottom = track_top + kSliderTrackHeight;
    const double ratio = static_cast<double>(slider.spec.current_value - slider.spec.minimum_value)
        / static_cast<double>(slider.spec.maximum_value - slider.spec.minimum_value);
    const int thumb_x = track_left + static_cast<int>((track_right - track_left) * ratio);

    HBRUSH track_brush = CreateSolidBrush(track_color);
    RECT track_rect{track_left, track_top, track_right, track_bottom};
    FillRect(hdc, &track_rect, track_brush);
    DeleteObject(track_brush);

    HBRUSH fill_brush = CreateSolidBrush(fill_color);
    RECT fill_rect{track_left, track_top, thumb_x, track_bottom};
    FillRect(hdc, &fill_rect, fill_brush);
    Ellipse(
        hdc,
        thumb_x - kSliderThumbRadius,
        track_top - kSliderThumbRadius + 2,
        thumb_x + kSliderThumbRadius,
        track_top + kSliderThumbRadius + 2
    );
    DeleteObject(fill_brush);
}

// ----------------------------------------------------------------------------
// Hook souris local utilise pendant TrackPopupMenu.
// ----------------------------------------------------------------------------
LRESULT CALLBACK MenuSliderMouseHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code < 0 || lparam == 0) {
        return CallNextHookEx(g_sliders.mouse_hook, code, wparam, lparam);
    }

    const auto* mouse = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    MenuSliderState* hovered_slider = FindSliderAtPoint(mouse->pt);
    if (wparam == WM_MOUSEMOVE) {
        UpdateHoveredSlider(hovered_slider);
    }
    if (wparam == WM_LBUTTONDOWN) {
        MenuSliderState* slider = hovered_slider;
        if (slider != nullptr) {
            g_sliders.dragging_command_id = slider->spec.command_id;
            ApplySliderValue(*slider, SliderValueFromScreenX(*slider, mouse->pt.x));
            return 1;
        }
    }

    if (wparam == WM_MOUSEMOVE && g_sliders.dragging_command_id != 0) {
        MenuSliderState* slider = FindSlider(g_sliders.dragging_command_id);
        if (slider != nullptr) {
            ApplySliderValue(*slider, SliderValueFromScreenX(*slider, mouse->pt.x));
            return 1;
        }
    }

    if (wparam == WM_LBUTTONUP && g_sliders.dragging_command_id != 0) {
        MenuSliderState* slider = FindSlider(g_sliders.dragging_command_id);
        if (slider != nullptr) {
            ApplySliderValue(*slider, SliderValueFromScreenX(*slider, mouse->pt.x));
        }
        g_sliders.dragging_command_id = 0;
        return 1;
    }

    return CallNextHookEx(g_sliders.mouse_hook, code, wparam, lparam);
}

}  // namespace

// ----------------------------------------------------------------------------
// Ajoute l'item owner-drawn d'un slider a un menu.
// ----------------------------------------------------------------------------
void AppendWidgetMenuSliderItem(HMENU menu, UINT command_id) {
    MenuSliderState* slider = FindSlider(command_id);
    const ULONG_PTR item_data = slider != nullptr
        ? reinterpret_cast<ULONG_PTR>(slider)
        : static_cast<ULONG_PTR>(command_id);
    AppendMenuW(menu, MF_OWNERDRAW, command_id, reinterpret_cast<LPCWSTR>(item_data));
}

// ----------------------------------------------------------------------------
// Active le suivi souris des sliders pendant l'affichage du menu.
// ----------------------------------------------------------------------------
void BeginWidgetMenuSliderTracking(
    HWND hwnd,
    const std::vector<WidgetMenuSliderSpec>& sliders,
    COLORREF active_color
) {
    EndWidgetMenuSliderTracking();
    g_sliders = MenuSlidersState{};
    g_sliders.hwnd = hwnd;
    g_sliders.active_color = active_color;
    g_sliders.sliders.reserve(sliders.size());
    for (WidgetMenuSliderSpec spec : sliders) {
        spec.current_value = std::clamp(spec.current_value, spec.minimum_value, spec.maximum_value);
        g_sliders.sliders.push_back(MenuSliderState{spec});
    }
    g_sliders.mouse_hook = SetWindowsHookExW(WH_MOUSE, MenuSliderMouseHook, nullptr, GetCurrentThreadId());
}

// ----------------------------------------------------------------------------
// Desactive le suivi souris des sliders.
// ----------------------------------------------------------------------------
void EndWidgetMenuSliderTracking() {
    if (g_sliders.mouse_hook != nullptr) {
        UnhookWindowsHookEx(g_sliders.mouse_hook);
    }
    g_sliders = MenuSlidersState{};
}

// ----------------------------------------------------------------------------
// Met a jour la valeur d'un slider visible sans fermer le menu.
// ----------------------------------------------------------------------------
void UpdateWidgetMenuSliderValue(UINT command_id, int value) {
    MenuSliderState* slider = FindSlider(command_id);
    if (slider == nullptr) {
        return;
    }

    slider->spec.current_value = std::clamp(
        value,
        slider->spec.minimum_value,
        slider->spec.maximum_value
    );
    RepaintSliderOnScreen(*slider);
}

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les sliders.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuSlider(MEASUREITEMSTRUCT* measure_item) {
    if (measure_item == nullptr || measure_item->CtlType != ODT_MENU) {
        return false;
    }

    MenuSliderState* slider = FindSliderFromMenuItem(measure_item->itemID, measure_item->itemData);
    if (slider == nullptr) {
        return false;
    }

    measure_item->itemWidth = kSliderItemWidth;
    measure_item->itemHeight = kSliderItemHeight;
    return true;
}

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les sliders.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuSlider(DRAWITEMSTRUCT* draw_item) {
    if (draw_item == nullptr || draw_item->CtlType != ODT_MENU) {
        return false;
    }

    MenuSliderState* slider = FindSliderFromMenuItem(draw_item->itemID, draw_item->itemData);
    if (slider == nullptr) {
        return false;
    }

    slider->item_rect_client = draw_item->rcItem;
    slider->menu_hwnd = WindowFromDC(draw_item->hDC);
    slider->item_rect_screen = draw_item->rcItem;
    if (slider->menu_hwnd != nullptr) {
        MapWindowPoints(slider->menu_hwnd, nullptr, reinterpret_cast<POINT*>(&slider->item_rect_screen), 2);
    }

    const bool selected = (draw_item->itemState & ODS_SELECTED) != 0
        || g_sliders.hovered_command_id == slider->spec.command_id;
    PaintSlider(draw_item->hDC, draw_item->rcItem, *slider, selected);
    return true;
}

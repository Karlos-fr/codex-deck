// ============================================================================
// Codex Glass - Implementation des sous-menus activables
// ----------------------------------------------------------------------------
// Ce fichier dessine et suit les items combinant une checkbox et un sous-menu.
// La checkbox bascule l'option, le libelle et le chevron restent geres par Win32.
// ============================================================================

#include "WidgetMenuToggleSubmenu.h"

#include "../localization/Localization.h"
#include "../window/WidgetWindow.h"

#include <algorithm>

namespace {

// Largeur minimale alignee sur les autres controles owner-drawn.
constexpr UINT kToggleSubmenuMinWidth = 232;

// Hauteur d'un sous-menu activable.
constexpr UINT kToggleSubmenuHeight = 26;

// Marge horizontale du contenu.
constexpr int kToggleSubmenuPadding = 12;

// Largeur reservee a la checkbox.
constexpr int kToggleSubmenuCheckColumnWidth = 28;

// Origine horizontale du libelle, alignee sur les items de menu sans checkbox.
constexpr int kToggleSubmenuLabelLeft = 36;

// Largeur reservee au chevron natif et a sa zone interactive.
constexpr int kToggleSubmenuArrowColumnWidth = 30;

// Taille du carre de checkbox.
constexpr int kToggleSubmenuBoxSize = 13;

// Couleur du fond des menus en mode sombre.
constexpr COLORREF kDarkMenuBackgroundColor = RGB(0x2B, 0x2B, 0x2B);

// Couleur du fond survole en mode sombre.
constexpr COLORREF kDarkMenuHotBackgroundColor = RGB(0x3A, 0x3A, 0x3A);

// Couleur du texte principal en mode sombre.
constexpr COLORREF kDarkMenuTextColor = RGB(0xF3, 0xF3, 0xF3);

// Couleur du texte desactive en mode sombre.
constexpr COLORREF kDarkMenuDisabledTextColor = RGB(0x88, 0x88, 0x88);

// Couleur neutre du contour de checkbox.
constexpr COLORREF kMenuControlTrackColor = RGB(0x68, 0x68, 0x72);

// Couleur eclaircie du contour d'une checkbox survolee.
constexpr COLORREF kMenuControlHotTrackColor = RGB(0x82, 0x82, 0x8C);

// Couleur active partagee avec les controles du menu.
constexpr COLORREF kMenuControlAccentColor = RGB(0x22, 0xC5, 0x5E);

// Message interne Win32 qui retourne le HMENU d'une fenetre de menu popup.
constexpr UINT kMenuGetHandleMessage = 0x01E1;

// Etat temporaire d'un sous-menu activable.
struct MenuToggleSubmenuState {
    WidgetMenuToggleSubmenuSpec spec{};
    HMENU parent_menu = nullptr;
    UINT parent_position = 0;
    HWND menu_hwnd = nullptr;
    RECT item_rect_client{};
    RECT item_rect_screen{};
};

// Etat global limite a la duree de TrackPopupMenu.
struct MenuToggleSubmenusState {
    HWND owner_hwnd = nullptr;
    HHOOK mouse_hook = nullptr;
    UINT pressed_command_id = 0;
    UINT hovered_command_id = 0;
    COLORREF active_color = kMenuControlAccentColor;
    std::vector<MenuToggleSubmenuState> items{};
};

// Etat courant des sous-menus activables.
MenuToggleSubmenusState g_toggle_submenus{};

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
// Recherche un sous-menu activable par sa commande.
//
// Parametres :
// - command_id : commande recherchee.
//
// Retour :
// - etat correspondant, ou nullptr.
// ----------------------------------------------------------------------------
MenuToggleSubmenuState* FindToggleSubmenu(UINT command_id) {
    for (MenuToggleSubmenuState& item : g_toggle_submenus.items) {
        if (item.spec.command_id == command_id) {
            return &item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche un sous-menu activable depuis les donnees owner-drawn.
//
// Parametres :
// - item_data : pointeur stocke par Win32.
//
// Retour :
// - etat correspondant, ou nullptr.
// ----------------------------------------------------------------------------
MenuToggleSubmenuState* FindToggleSubmenuByData(ULONG_PTR item_data) {
    for (MenuToggleSubmenuState& item : g_toggle_submenus.items) {
        if (reinterpret_cast<ULONG_PTR>(&item) == item_data) {
            return &item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Identifie un item depuis les valeurs fournies par WM_MEASUREITEM ou WM_DRAWITEM.
//
// Parametres :
// - item_id : identifiant Win32 de l'item.
// - item_data : donnees owner-drawn de l'item.
//
// Retour :
// - etat correspondant, ou nullptr.
// ----------------------------------------------------------------------------
MenuToggleSubmenuState* FindToggleSubmenuFromMenuItem(UINT item_id, ULONG_PTR item_data) {
    if (MenuToggleSubmenuState* item = FindToggleSubmenuByData(item_data)) {
        return item;
    }
    if (MenuToggleSubmenuState* item = FindToggleSubmenu(item_id)) {
        return item;
    }
    return FindToggleSubmenu(static_cast<UINT>(item_data));
}

// ----------------------------------------------------------------------------
// Recherche l'item visible situe sous un point ecran.
//
// Parametres :
// - point : position ecran a tester.
//
// Retour :
// - etat correspondant, ou nullptr.
// ----------------------------------------------------------------------------
MenuToggleSubmenuState* FindToggleSubmenuAtPoint(POINT point) {
    for (MenuToggleSubmenuState& item : g_toggle_submenus.items) {
        if (item.menu_hwnd != nullptr
            && IsWindowVisible(item.menu_hwnd) != FALSE
            && !IsRectEmpty(&item.item_rect_screen)
            && PtInRect(&item.item_rect_screen, point) != FALSE) {
            const HMENU menu = reinterpret_cast<HMENU>(SendMessageW(
                item.menu_hwnd,
                kMenuGetHandleMessage,
                0,
                0
            ));
            if (menu == nullptr || menu != item.parent_menu) {
                continue;
            }
            return &item;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Indique si un point cible le carre visible de la checkbox.
//
// Parametres :
// - item : item concerne.
// - point : position ecran a tester.
//
// Retour :
// - true uniquement lorsque le point se trouve dans le carre de controle.
// ----------------------------------------------------------------------------
bool IsToggleControlAtPoint(const MenuToggleSubmenuState& item, POINT point) {
    const int center_y = item.item_rect_screen.top
        + ((item.item_rect_screen.bottom - item.item_rect_screen.top) / 2);
    const int box_left = item.item_rect_screen.left + kToggleSubmenuPadding;
    const int box_top = center_y - (kToggleSubmenuBoxSize / 2);
    const RECT box_rect{
        box_left,
        box_top,
        box_left + kToggleSubmenuBoxSize,
        box_top + kToggleSubmenuBoxSize,
    };
    return PtInRect(&box_rect, point) != FALSE;
}

// ----------------------------------------------------------------------------
// Cree la police de menu definie par Windows.
//
// Retour :
// - police GDI a liberer par l'appelant, ou nullptr.
// ----------------------------------------------------------------------------
HFONT CreateToggleSubmenuFont() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) == FALSE) {
        return nullptr;
    }
    return CreateFontIndirectW(&metrics.lfMenuFont);
}

// ----------------------------------------------------------------------------
// Calcule la largeur necessaire au libelle et aux deux zones de controle.
//
// Parametres :
// - item : item a mesurer.
//
// Retour :
// - largeur owner-drawn en pixels.
// ----------------------------------------------------------------------------
UINT MeasureToggleSubmenuWidth(const MenuToggleSubmenuState& item) {
    HDC hdc = GetDC(nullptr);
    if (hdc == nullptr) {
        return kToggleSubmenuMinWidth;
    }

    HFONT font = CreateToggleSubmenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(hdc, font) : nullptr;
    RECT checked_rect{};
    RECT unchecked_rect{};
    DrawTextW(hdc, item.spec.checked_text.c_str(), -1, &checked_rect, DT_SINGLELINE | DT_CALCRECT);
    DrawTextW(hdc, item.spec.unchecked_text.c_str(), -1, &unchecked_rect, DT_SINGLELINE | DT_CALCRECT);
    if (previous_font != nullptr) {
        SelectObject(hdc, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
    ReleaseDC(nullptr, hdc);

    const int text_width = std::max(
        checked_rect.right - checked_rect.left,
        unchecked_rect.right - unchecked_rect.left
    );
    const int width = text_width
        + kToggleSubmenuCheckColumnWidth
        + kToggleSubmenuArrowColumnWidth
        + (kToggleSubmenuPadding * 2);
    return static_cast<UINT>(std::max(static_cast<int>(kToggleSubmenuMinWidth), width));
}

// ----------------------------------------------------------------------------
// Dessine la checkbox carree du composant.
//
// Parametres :
// - hdc : contexte GDI cible.
// - rect : rectangle complet de l'item.
// - checked : etat courant.
// - hovered : indique si la zone de checkbox est survolee.
// - check_color : couleur de la coche.
// ----------------------------------------------------------------------------
void PaintToggleControl(
    HDC hdc,
    const RECT& rect,
    bool checked,
    bool hovered,
    COLORREF check_color
) {
    const int center_y = rect.top + ((rect.bottom - rect.top) / 2);
    const int box_left = rect.left + kToggleSubmenuPadding;
    const int box_top = center_y - (kToggleSubmenuBoxSize / 2);
    const RECT box_rect{
        box_left,
        box_top,
        box_left + kToggleSubmenuBoxSize,
        box_top + kToggleSubmenuBoxSize,
    };

    const COLORREF box_color = checked
        ? (hovered
            ? LightenControlColor(g_toggle_submenus.active_color, 24)
            : g_toggle_submenus.active_color)
        : (hovered ? kMenuControlHotTrackColor : kMenuControlTrackColor);
    HBRUSH box_brush = CreateSolidBrush(box_color);
    HPEN box_pen = CreatePen(
        PS_SOLID,
        1,
        box_color
    );
    HGDIOBJ previous_brush = SelectObject(
        hdc,
        checked ? static_cast<HGDIOBJ>(box_brush) : GetStockObject(NULL_BRUSH)
    );
    HGDIOBJ previous_pen = SelectObject(hdc, box_pen);
    Rectangle(hdc, box_rect.left, box_rect.top, box_rect.right, box_rect.bottom);
    SelectObject(hdc, previous_pen);
    SelectObject(hdc, previous_brush);
    DeleteObject(box_pen);
    DeleteObject(box_brush);

    if (!checked) {
        return;
    }

    const int left = box_left + 2;
    POINT points[]{
        POINT{left, center_y},
        POINT{left + 3, center_y + 3},
        POINT{left + 8, center_y - 3},
    };
    HPEN check_pen = CreatePen(PS_SOLID, 2, check_color);
    previous_pen = SelectObject(hdc, check_pen);
    Polyline(hdc, points, static_cast<int>(_countof(points)));
    SelectObject(hdc, previous_pen);
    DeleteObject(check_pen);
}

// ----------------------------------------------------------------------------
// Dessine le fond, la checkbox et le libelle d'un sous-menu activable.
//
// Parametres :
// - draw_item : structure Win32 de dessin.
// - item : etat courant du composant.
// ----------------------------------------------------------------------------
void PaintToggleSubmenu(DRAWITEMSTRUCT* draw_item, const MenuToggleSubmenuState& item) {
    const bool dark_mode = IsNativeDarkModeEnabled();
    const bool selected = item.spec.enabled && ((draw_item->itemState & ODS_SELECTED) != 0);
    const COLORREF background = dark_mode ? kDarkMenuBackgroundColor : GetSysColor(COLOR_MENU);
    const COLORREF hot_background = dark_mode ? kDarkMenuHotBackgroundColor : GetSysColor(COLOR_HIGHLIGHT);
    COLORREF text_color = dark_mode ? kDarkMenuTextColor : GetSysColor(COLOR_MENUTEXT);
    if (!item.spec.enabled) {
        text_color = dark_mode ? kDarkMenuDisabledTextColor : GetSysColor(COLOR_GRAYTEXT);
    } else if (selected && !dark_mode) {
        text_color = GetSysColor(COLOR_HIGHLIGHTTEXT);
    }

    HBRUSH background_brush = CreateSolidBrush(selected ? hot_background : background);
    FillRect(draw_item->hDC, &draw_item->rcItem, background_brush);
    DeleteObject(background_brush);

    PaintToggleControl(
        draw_item->hDC,
        draw_item->rcItem,
        item.spec.checked,
        g_toggle_submenus.hovered_command_id == item.spec.command_id,
        item.spec.enabled ? RGB(0xFF, 0xFF, 0xFF) : text_color
    );

    SetBkMode(draw_item->hDC, TRANSPARENT);
    SetTextColor(draw_item->hDC, text_color);
    HFONT font = CreateToggleSubmenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(draw_item->hDC, font) : nullptr;
    RECT text_rect{
        draw_item->rcItem.left + kToggleSubmenuLabelLeft,
        draw_item->rcItem.top,
        draw_item->rcItem.right - kToggleSubmenuArrowColumnWidth,
        draw_item->rcItem.bottom,
    };
    const std::wstring& text = item.spec.checked ? item.spec.checked_text : item.spec.unchecked_text;
    DrawTextW(
        draw_item->hDC,
        text.c_str(),
        -1,
        &text_rect,
        DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS
    );
    if (previous_font != nullptr) {
        SelectObject(draw_item->hDC, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }

    // Win32 dessine le chevron du sous-menu ; cette zone reste volontairement libre.
}

// ----------------------------------------------------------------------------
// Redessine immediatement un item dont l'etat vient de changer.
//
// Parametres :
// - item : item a rafraichir.
// ----------------------------------------------------------------------------
void RepaintToggleSubmenu(const MenuToggleSubmenuState& item) {
    if (item.menu_hwnd == nullptr || IsRectEmpty(&item.item_rect_client)) {
        return;
    }
    InvalidateRect(item.menu_hwnd, &item.item_rect_client, FALSE);
    UpdateWindow(item.menu_hwnd);
}

// ----------------------------------------------------------------------------
// Met a jour la checkbox de sous-menu survolee et rafraichit son rendu.
//
// Parametres :
// - item : composant dont la zone checkbox est survolee, ou nullptr.
// ----------------------------------------------------------------------------
void UpdateHoveredToggleSubmenu(MenuToggleSubmenuState* item) {
    const UINT command_id = item != nullptr ? item->spec.command_id : 0;
    if (command_id == g_toggle_submenus.hovered_command_id) {
        return;
    }

    MenuToggleSubmenuState* previous = FindToggleSubmenu(g_toggle_submenus.hovered_command_id);
    g_toggle_submenus.hovered_command_id = command_id;
    if (previous != nullptr) {
        RepaintToggleSubmenu(*previous);
    }
    if (item != nullptr) {
        RepaintToggleSubmenu(*item);
    }
}

// ----------------------------------------------------------------------------
// Bascule l'etat local puis notifie la fenetre proprietaire.
//
// Parametres :
// - item : item active par l'utilisateur.
// ----------------------------------------------------------------------------
void ApplyToggleSubmenuClick(MenuToggleSubmenuState& item) {
    if (!item.spec.enabled) {
        return;
    }
    item.spec.checked = !item.spec.checked;
    if (g_toggle_submenus.owner_hwnd != nullptr) {
        SendMessageW(
            g_toggle_submenus.owner_hwnd,
            WM_COMMAND,
            static_cast<WPARAM>(item.spec.command_id),
            0
        );
    }
    RepaintToggleSubmenu(item);
}

// ----------------------------------------------------------------------------
// Intercepte uniquement les clics dans la zone de bascule du composant.
//
// Parametres :
// - code : code du hook local.
// - wparam : message souris.
// - lparam : informations de position souris.
//
// Retour :
// - 1 si le clic est consomme, sinon le resultat du hook suivant.
// ----------------------------------------------------------------------------
LRESULT CALLBACK MenuToggleSubmenuMouseHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code < 0 || lparam == 0) {
        return CallNextHookEx(g_toggle_submenus.mouse_hook, code, wparam, lparam);
    }

    const auto* mouse = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    MenuToggleSubmenuState* item = FindToggleSubmenuAtPoint(mouse->pt);
    const bool toggle_control = item != nullptr && IsToggleControlAtPoint(*item, mouse->pt);

    if (wparam == WM_MOUSEMOVE) {
        UpdateHoveredToggleSubmenu(toggle_control ? item : nullptr);
    }

    if (wparam == WM_LBUTTONDOWN && toggle_control && item->spec.enabled) {
        g_toggle_submenus.pressed_command_id = item->spec.command_id;
        return 1;
    }
    if (wparam == WM_LBUTTONUP && g_toggle_submenus.pressed_command_id != 0) {
        const UINT pressed_command_id = g_toggle_submenus.pressed_command_id;
        g_toggle_submenus.pressed_command_id = 0;
        if (toggle_control && item->spec.command_id == pressed_command_id) {
            ApplyToggleSubmenuClick(*item);
        }
        return 1;
    }

    return CallNextHookEx(g_toggle_submenus.mouse_hook, code, wparam, lparam);
}

} // namespace

// ----------------------------------------------------------------------------
// Ajoute un sous-menu activable owner-drawn a un menu parent.
// ----------------------------------------------------------------------------
void AppendWidgetMenuToggleSubmenuItem(HMENU menu, HMENU submenu, UINT command_id) {
    MenuToggleSubmenuState* item = FindToggleSubmenu(command_id);
    const int insertion_position = GetMenuItemCount(menu);
    const ULONG_PTR item_data = item != nullptr
        ? reinterpret_cast<ULONG_PTR>(item)
        : static_cast<ULONG_PTR>(command_id);
    const bool enabled = item == nullptr || item->spec.enabled;

    MENUITEMINFOW menu_item{};
    menu_item.cbSize = sizeof(menu_item);
    menu_item.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_DATA;
    menu_item.fType = MFT_OWNERDRAW;
    menu_item.fState = enabled ? MFS_ENABLED : MFS_GRAYED;
    menu_item.wID = command_id;
    menu_item.hSubMenu = submenu;
    menu_item.dwItemData = item_data;
    InsertMenuItemW(menu, insertion_position, TRUE, &menu_item);
    if (item != nullptr) {
        item->parent_menu = menu;
        item->parent_position = static_cast<UINT>(insertion_position);
    }
}

// ----------------------------------------------------------------------------
// Active le suivi souris des sous-menus activables pendant TrackPopupMenu.
// ----------------------------------------------------------------------------
void BeginWidgetMenuToggleSubmenuTracking(
    HWND hwnd,
    const std::vector<WidgetMenuToggleSubmenuSpec>& items,
    COLORREF active_color
) {
    EndWidgetMenuToggleSubmenuTracking();
    g_toggle_submenus.owner_hwnd = hwnd;
    g_toggle_submenus.active_color = active_color;
    g_toggle_submenus.items.reserve(items.size());
    for (const WidgetMenuToggleSubmenuSpec& spec : items) {
        g_toggle_submenus.items.push_back(MenuToggleSubmenuState{spec});
    }
    if (!g_toggle_submenus.items.empty()) {
        g_toggle_submenus.mouse_hook = SetWindowsHookExW(
            WH_MOUSE,
            MenuToggleSubmenuMouseHook,
            nullptr,
            GetCurrentThreadId()
        );
    }
}

// ----------------------------------------------------------------------------
// Desactive le suivi souris et libere l'etat temporaire du composant.
// ----------------------------------------------------------------------------
void EndWidgetMenuToggleSubmenuTracking() {
    if (g_toggle_submenus.mouse_hook != nullptr) {
        UnhookWindowsHookEx(g_toggle_submenus.mouse_hook);
    }
    g_toggle_submenus = MenuToggleSubmenusState{};
}

// ----------------------------------------------------------------------------
// Retraduit les libelles des sous-menus activables encore ouverts.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeWidgetMenuToggleSubmenus(UiLanguage previous_language) {
    for (MenuToggleSubmenuState& item : g_toggle_submenus.items) {
        item.spec.checked_text = RelocalizeText(item.spec.checked_text, previous_language);
        item.spec.unchecked_text = RelocalizeText(item.spec.unchecked_text, previous_language);
    }
}

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les sous-menus activables.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuToggleSubmenu(MEASUREITEMSTRUCT* measure_item) {
    if (measure_item == nullptr || measure_item->CtlType != ODT_MENU) {
        return false;
    }
    MenuToggleSubmenuState* item = FindToggleSubmenuFromMenuItem(
        measure_item->itemID,
        measure_item->itemData
    );
    if (item == nullptr) {
        return false;
    }
    measure_item->itemWidth = MeasureToggleSubmenuWidth(*item);
    measure_item->itemHeight = kToggleSubmenuHeight;
    return true;
}

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les sous-menus activables.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuToggleSubmenu(DRAWITEMSTRUCT* draw_item) {
    if (draw_item == nullptr || draw_item->CtlType != ODT_MENU) {
        return false;
    }
    MenuToggleSubmenuState* item = FindToggleSubmenuFromMenuItem(
        draw_item->itemID,
        draw_item->itemData
    );
    if (item == nullptr) {
        return false;
    }

    item->item_rect_client = draw_item->rcItem;
    item->menu_hwnd = WindowFromDC(draw_item->hDC);
    GetMenuItemRect(
        nullptr,
        item->parent_menu,
        item->parent_position,
        &item->item_rect_screen
    );
    PaintToggleSubmenu(draw_item, *item);
    return true;
}

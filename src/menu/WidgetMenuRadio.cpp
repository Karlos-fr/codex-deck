// ============================================================================
// Codex Glass - Implementation des boutons radio de menu
// ----------------------------------------------------------------------------
// Ce fichier dessine les choix exclusifs et intercepte leurs clics pendant
// TrackPopupMenu sans fermer le menu contextuel.
// ============================================================================

#include "WidgetMenuRadio.h"

#include "../localization/Localization.h"
#include "../window/WidgetWindow.h"

#include <algorithm>

namespace {

// Largeur de l'item, alignee sur celle des sliders.
constexpr UINT kRadioItemWidth = 232;

// Hauteur de l'item radio.
constexpr UINT kRadioItemHeight = 26;

// Marge horizontale du contenu, alignee sur celle des sliders.
constexpr int kRadioHorizontalPadding = 12;

// Largeur de la colonne du bouton radio.
constexpr int kRadioControlColumnWidth = 28;

// Diametre externe du bouton radio.
constexpr int kRadioOuterSize = 13;

// Diametre du point selectionne.
constexpr int kRadioInnerSize = 7;

// Couleur du fond sombre des menus.
constexpr COLORREF kDarkMenuBackgroundColor = RGB(0x2B, 0x2B, 0x2B);

// Couleur du fond sombre d'un item survole.
constexpr COLORREF kDarkMenuHotBackgroundColor = RGB(0x3A, 0x3A, 0x3A);

// Couleur du texte principal en theme sombre.
constexpr COLORREF kDarkMenuTextColor = RGB(0xF3, 0xF3, 0xF3);

// Couleur neutre du contour, identique au rail des sliders.
constexpr COLORREF kMenuControlTrackColor = RGB(0x68, 0x68, 0x72);

// Couleur active, identique au remplissage des sliders.
constexpr COLORREF kMenuControlAccentColor = RGB(0x22, 0xC5, 0x5E);

// Message interne Win32 qui retourne le HMENU d'une fenetre de menu popup.
constexpr UINT kMenuGetHandleMessage = 0x01E1;

// Etat d'un bouton radio pendant l'affichage du menu.
struct MenuRadioState {
    WidgetMenuRadioSpec spec{};
    HWND menu_hwnd = nullptr;
    RECT item_rect_client{};
    RECT item_rect_screen{};
};

// Etat global des boutons radio pendant TrackPopupMenu.
struct MenuRadiosState {
    HWND hwnd = nullptr;
    HHOOK mouse_hook = nullptr;
    COLORREF active_color = kMenuControlAccentColor;
    std::vector<MenuRadioState> radios{};
};

// Etat radio actif sur le thread UI.
MenuRadiosState g_radios{};

// ----------------------------------------------------------------------------
// Recherche un bouton radio par commande.
//
// Parametres :
// - command_id : commande recherchee.
//
// Retour :
// - etat radio, ou nullptr si absent.
// ----------------------------------------------------------------------------
MenuRadioState* FindRadio(UINT command_id) {
    for (MenuRadioState& radio : g_radios.radios) {
        if (radio.spec.command_id == command_id) {
            return &radio;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche un bouton radio depuis les donnees owner-drawn.
//
// Parametres :
// - item_data : donnees associees a l'item.
//
// Retour :
// - etat radio, ou nullptr.
// ----------------------------------------------------------------------------
MenuRadioState* FindRadioByItemData(ULONG_PTR item_data) {
    for (MenuRadioState& radio : g_radios.radios) {
        if (reinterpret_cast<ULONG_PTR>(&radio) == item_data) {
            return &radio;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche un bouton radio depuis un message owner-drawn Win32.
// ----------------------------------------------------------------------------
MenuRadioState* FindRadioFromMenuItem(UINT item_id, ULONG_PTR item_data) {
    if (MenuRadioState* radio = FindRadioByItemData(item_data)) {
        return radio;
    }
    if (MenuRadioState* radio = FindRadio(item_id)) {
        return radio;
    }
    return FindRadio(static_cast<UINT>(item_data));
}

// ----------------------------------------------------------------------------
// Recherche le bouton radio sous une position ecran.
// ----------------------------------------------------------------------------
MenuRadioState* FindRadioAtPoint(POINT point) {
    for (MenuRadioState& radio : g_radios.radios) {
        if (radio.menu_hwnd != nullptr
            && IsWindowVisible(radio.menu_hwnd) != FALSE
            && !IsRectEmpty(&radio.item_rect_screen)
            && PtInRect(&radio.item_rect_screen, point) != FALSE) {
            const HMENU menu = reinterpret_cast<HMENU>(SendMessageW(
                radio.menu_hwnd,
                kMenuGetHandleMessage,
                0,
                0
            ));
            const int item_count = menu != nullptr ? GetMenuItemCount(menu) : 0;
            bool item_present = false;
            for (int position = 0; position < item_count; ++position) {
                if (GetMenuItemID(menu, position) == radio.spec.command_id) {
                    item_present = true;
                    break;
                }
            }
            if (!item_present) {
                continue;
            }
            return &radio;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Cree une police adaptee aux metriques de menu Windows.
//
// Retour :
// - police GDI a liberer, ou nullptr.
// ----------------------------------------------------------------------------
HFONT CreateRadioMenuFont() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) == FALSE) {
        return nullptr;
    }
    return CreateFontIndirectW(&metrics.lfMenuFont);
}

// ----------------------------------------------------------------------------
// Mesure la largeur necessaire au libelle radio.
// ----------------------------------------------------------------------------
UINT MeasureRadioWidth(const MenuRadioState& radio) {
    HDC hdc = GetDC(nullptr);
    if (hdc == nullptr) {
        return kRadioItemWidth;
    }
    HFONT font = CreateRadioMenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(hdc, font) : nullptr;
    RECT text_rect{};
    DrawTextW(hdc, radio.spec.text.c_str(), -1, &text_rect, DT_SINGLELINE | DT_CALCRECT);
    if (previous_font != nullptr) {
        SelectObject(hdc, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
    ReleaseDC(nullptr, hdc);
    const int width = text_rect.right + kRadioControlColumnWidth + (kRadioHorizontalPadding * 2);
    return static_cast<UINT>(std::max(static_cast<int>(kRadioItemWidth), width));
}

// ----------------------------------------------------------------------------
// Redessine directement un bouton radio visible.
// ----------------------------------------------------------------------------
void RepaintRadioOnScreen(const MenuRadioState& radio) {
    if (radio.menu_hwnd == nullptr || IsRectEmpty(&radio.item_rect_client)) {
        return;
    }
    InvalidateRect(radio.menu_hwnd, &radio.item_rect_client, FALSE);
    UpdateWindow(radio.menu_hwnd);
}

// ----------------------------------------------------------------------------
// Redessine tous les boutons radio d'un groupe.
// ----------------------------------------------------------------------------
void RepaintRadioGroup(UINT group_id) {
    for (const MenuRadioState& radio : g_radios.radios) {
        if (radio.spec.group_id == group_id) {
            RepaintRadioOnScreen(radio);
        }
    }
}

// ----------------------------------------------------------------------------
// Dessine le cercle et son point de selection.
// ----------------------------------------------------------------------------
void PaintRadioControl(HDC hdc, const RECT& rect, bool selected) {
    const int center_x = rect.left + kRadioHorizontalPadding + (kRadioOuterSize / 2);
    const int center_y = rect.top + ((rect.bottom - rect.top) / 2);
    HPEN outline_pen = CreatePen(PS_SOLID, 1, selected ? g_radios.active_color : kMenuControlTrackColor);
    HGDIOBJ previous_pen = SelectObject(hdc, outline_pen);
    HGDIOBJ previous_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(
        hdc,
        center_x - (kRadioOuterSize / 2),
        center_y - (kRadioOuterSize / 2),
        center_x + (kRadioOuterSize / 2) + 1,
        center_y + (kRadioOuterSize / 2) + 1
    );
    if (selected) {
        HBRUSH accent_brush = CreateSolidBrush(g_radios.active_color);
        SelectObject(hdc, accent_brush);
        Ellipse(
            hdc,
            center_x - (kRadioInnerSize / 2),
            center_y - (kRadioInnerSize / 2),
            center_x + (kRadioInnerSize / 2) + 1,
            center_y + (kRadioInnerSize / 2) + 1
        );
        SelectObject(hdc, previous_brush);
        DeleteObject(accent_brush);
    } else {
        SelectObject(hdc, previous_brush);
    }
    SelectObject(hdc, previous_pen);
    DeleteObject(outline_pen);
}

// ----------------------------------------------------------------------------
// Dessine un item radio complet dans un HDC.
// ----------------------------------------------------------------------------
void PaintRadio(HDC hdc, const RECT& rect, const MenuRadioState& radio, bool hot) {
    const bool dark_mode = IsNativeDarkModeEnabled();
    const COLORREF background_color = dark_mode ? kDarkMenuBackgroundColor : GetSysColor(COLOR_MENU);
    const COLORREF hot_background_color = dark_mode ? kDarkMenuHotBackgroundColor : GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF text_color = dark_mode ? kDarkMenuTextColor : GetSysColor(hot ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);
    HBRUSH background_brush = CreateSolidBrush(hot ? hot_background_color : background_color);
    FillRect(hdc, &rect, background_brush);
    DeleteObject(background_brush);

    PaintRadioControl(hdc, rect, radio.spec.selected);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text_color);
    HFONT font = CreateRadioMenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(hdc, font) : nullptr;
    RECT text_rect{
        rect.left + kRadioHorizontalPadding + kRadioControlColumnWidth,
        rect.top,
        rect.right - kRadioHorizontalPadding,
        rect.bottom,
    };
    DrawTextW(hdc, radio.spec.text.c_str(), -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    if (previous_font != nullptr) {
        SelectObject(hdc, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
}

// ----------------------------------------------------------------------------
// Selectionne une option, deselectionne son groupe et notifie l'application.
// ----------------------------------------------------------------------------
void ApplyRadioClick(MenuRadioState& radio) {
    for (MenuRadioState& item : g_radios.radios) {
        if (item.spec.group_id == radio.spec.group_id) {
            item.spec.selected = item.spec.command_id == radio.spec.command_id;
        }
    }
    if (g_radios.hwnd != nullptr) {
        SendMessageW(g_radios.hwnd, WM_COMMAND, static_cast<WPARAM>(radio.spec.command_id), 0);
    }
    RepaintRadioGroup(radio.spec.group_id);
}

// ----------------------------------------------------------------------------
// Hook souris local utilise pendant TrackPopupMenu.
// ----------------------------------------------------------------------------
LRESULT CALLBACK MenuRadioMouseHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code < 0 || lparam == 0) {
        return CallNextHookEx(g_radios.mouse_hook, code, wparam, lparam);
    }
    const auto* mouse = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    if (wparam == WM_LBUTTONDOWN || wparam == WM_LBUTTONUP) {
        if (MenuRadioState* radio = FindRadioAtPoint(mouse->pt)) {
            if (wparam == WM_LBUTTONUP) {
                ApplyRadioClick(*radio);
            }
            return 1;
        }
    }
    return CallNextHookEx(g_radios.mouse_hook, code, wparam, lparam);
}

} // namespace

// ----------------------------------------------------------------------------
// Ajoute un bouton radio owner-drawn a un menu.
// ----------------------------------------------------------------------------
void AppendWidgetMenuRadioItem(HMENU menu, UINT command_id) {
    MenuRadioState* radio = FindRadio(command_id);
    const ULONG_PTR item_data = radio != nullptr
        ? reinterpret_cast<ULONG_PTR>(radio)
        : static_cast<ULONG_PTR>(command_id);
    AppendMenuW(menu, MF_OWNERDRAW, command_id, reinterpret_cast<LPCWSTR>(item_data));
}

// ----------------------------------------------------------------------------
// Active le suivi souris des boutons radio.
// ----------------------------------------------------------------------------
void BeginWidgetMenuRadioTracking(
    HWND hwnd,
    const std::vector<WidgetMenuRadioSpec>& radios,
    COLORREF active_color
) {
    EndWidgetMenuRadioTracking();
    g_radios.hwnd = hwnd;
    g_radios.active_color = active_color;
    g_radios.radios.reserve(radios.size());
    for (const WidgetMenuRadioSpec& spec : radios) {
        g_radios.radios.push_back(MenuRadioState{spec});
    }
    g_radios.mouse_hook = SetWindowsHookExW(WH_MOUSE, MenuRadioMouseHook, nullptr, GetCurrentThreadId());
}

// ----------------------------------------------------------------------------
// Desactive le suivi souris des boutons radio.
// ----------------------------------------------------------------------------
void EndWidgetMenuRadioTracking() {
    if (g_radios.mouse_hook != nullptr) {
        UnhookWindowsHookEx(g_radios.mouse_hook);
    }
    g_radios = MenuRadiosState{};
}

// ----------------------------------------------------------------------------
// Retraduit les libelles des radios du menu encore ouvert.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeWidgetMenuRadios(UiLanguage previous_language) {
    for (MenuRadioState& radio : g_radios.radios) {
        radio.spec.text = RelocalizeText(radio.spec.text, previous_language);
    }
}

// ----------------------------------------------------------------------------
// Selectionne un bouton radio visible sans emettre sa commande.
// ----------------------------------------------------------------------------
void SelectWidgetMenuRadioItem(UINT command_id) {
    MenuRadioState* selected = FindRadio(command_id);
    if (selected == nullptr) {
        return;
    }

    for (MenuRadioState& radio : g_radios.radios) {
        if (radio.spec.group_id == selected->spec.group_id) {
            radio.spec.selected = radio.spec.command_id == command_id;
        }
    }
    RepaintRadioGroup(selected->spec.group_id);
}

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les boutons radio.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuRadio(MEASUREITEMSTRUCT* measure_item) {
    if (measure_item == nullptr || measure_item->CtlType != ODT_MENU) {
        return false;
    }
    MenuRadioState* radio = FindRadioFromMenuItem(measure_item->itemID, measure_item->itemData);
    if (radio == nullptr) {
        return false;
    }
    measure_item->itemWidth = MeasureRadioWidth(*radio);
    measure_item->itemHeight = kRadioItemHeight;
    return true;
}

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les boutons radio.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuRadio(DRAWITEMSTRUCT* draw_item) {
    if (draw_item == nullptr || draw_item->CtlType != ODT_MENU) {
        return false;
    }
    MenuRadioState* radio = FindRadioFromMenuItem(draw_item->itemID, draw_item->itemData);
    if (radio == nullptr) {
        return false;
    }
    radio->item_rect_client = draw_item->rcItem;
    radio->menu_hwnd = WindowFromDC(draw_item->hDC);
    radio->item_rect_screen = draw_item->rcItem;
    if (radio->menu_hwnd != nullptr) {
        MapWindowPoints(radio->menu_hwnd, nullptr, reinterpret_cast<POINT*>(&radio->item_rect_screen), 2);
    }
    PaintRadio(draw_item->hDC, draw_item->rcItem, *radio, (draw_item->itemState & ODS_SELECTED) != 0);
    return true;
}

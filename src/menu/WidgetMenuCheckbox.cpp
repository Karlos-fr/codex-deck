// ============================================================================
// Codex Glass - Implementation des checkboxes de menu
// ----------------------------------------------------------------------------
// Ce fichier dessine des items checkbox owner-drawn et intercepte les clics
// pendant TrackPopupMenu pour basculer les options sans fermer le menu.
// ============================================================================

#include "WidgetMenuCheckbox.h"

#include "WidgetMenu.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../window/WidgetWindow.h"

#include <algorithm>

namespace {

// Largeur de l'item owner-drawn, alignee sur celle des sliders.
constexpr UINT kCheckboxItemMinWidth = 232;

// Hauteur de l'item owner-drawn en pixels.
constexpr UINT kCheckboxItemHeight = 26;

// Largeur de la colonne de controle.
constexpr int kCheckboxCheckColumnWidth = 28;

// Marge horizontale du contenu, alignee sur celle des sliders.
constexpr int kCheckboxTextPadding = 12;

// Origine horizontale du libelle, alignee sur les items de menu sans checkbox.
constexpr int kCheckboxLabelLeft = 36;

// Taille du carre de checkbox.
constexpr int kCheckboxBoxSize = 13;

// Couleur du fond sombre natif approximatif des menus Windows 11.
constexpr COLORREF kDarkMenuBackgroundColor = RGB(0x2B, 0x2B, 0x2B);

// Couleur du fond sombre natif approximatif d'un item survole.
constexpr COLORREF kDarkMenuHotBackgroundColor = RGB(0x3A, 0x3A, 0x3A);

// Couleur du texte principal en theme sombre.
constexpr COLORREF kDarkMenuTextColor = RGB(0xF3, 0xF3, 0xF3);

// Couleur neutre du contour des controles, identique au rail des sliders.
constexpr COLORREF kMenuControlTrackColor = RGB(0x68, 0x68, 0x72);

// Couleur legerement eclaircie du contour d'une checkbox survolee.
constexpr COLORREF kMenuControlHotTrackColor = RGB(0x82, 0x82, 0x8C);

// Couleur active des controles, identique au remplissage des sliders.
constexpr COLORREF kMenuControlAccentColor = RGB(0x22, 0xC5, 0x5E);

// Message interne Win32 qui retourne le HMENU d'une fenetre de menu popup.
constexpr UINT kMenuGetHandleMessage = 0x01E1;

// Etat d'une checkbox pendant l'affichage du menu.
struct MenuCheckboxState {
    WidgetMenuCheckboxSpec spec{};
    HWND menu_hwnd = nullptr;
    RECT item_rect_client{};
    RECT item_rect_screen{};
};

// Etat global des checkboxes pendant l'affichage du menu.
struct MenuCheckboxesState {
    HWND hwnd = nullptr;
    HHOOK mouse_hook = nullptr;
    bool mouse_down_consumed = false;
    UINT pressed_command_id = 0;
    UINT hovered_command_id = 0;
    COLORREF active_color = kMenuControlAccentColor;
    std::vector<MenuCheckboxState> checkboxes{};
};

MenuCheckboxesState g_checkboxes{};

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
// Recherche l'etat d'une checkbox par commande.
//
// Parametres :
// - command_id : commande recherchee.
//
// Retour :
// - etat de la checkbox, ou nullptr si absent.
// ----------------------------------------------------------------------------
MenuCheckboxState* FindCheckbox(UINT command_id) {
    for (MenuCheckboxState& checkbox : g_checkboxes.checkboxes) {
        if (checkbox.spec.command_id == command_id) {
            return &checkbox;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche l'etat d'une checkbox depuis un pointeur stocke dans un item Win32.
//
// Parametres :
// - item_data : donnees associees a l'item owner-drawn.
//
// Retour :
// - etat de la checkbox, ou nullptr.
// ----------------------------------------------------------------------------
MenuCheckboxState* FindCheckboxByItemData(ULONG_PTR item_data) {
    if (item_data == 0) {
        return nullptr;
    }

    for (MenuCheckboxState& checkbox : g_checkboxes.checkboxes) {
        if (reinterpret_cast<ULONG_PTR>(&checkbox) == item_data) {
            return &checkbox;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Recherche l'etat d'une checkbox depuis les donnees transmises par Win32.
//
// Parametres :
// - item_id : identifiant de l'item.
// - item_data : donnees associees a l'item owner-drawn.
//
// Retour :
// - etat de la checkbox, ou nullptr.
// ----------------------------------------------------------------------------
MenuCheckboxState* FindCheckboxFromMenuItem(UINT item_id, ULONG_PTR item_data) {
    if (MenuCheckboxState* checkbox = FindCheckboxByItemData(item_data)) {
        return checkbox;
    }
    if (MenuCheckboxState* checkbox = FindCheckbox(item_id)) {
        return checkbox;
    }
    return FindCheckbox(static_cast<UINT>(item_data));
}

// ----------------------------------------------------------------------------
// Recherche la position d'une commande dans un menu popup.
//
// Parametres :
// - menu : menu a parcourir.
// - command_id : commande recherchee.
//
// Retour :
// - position de l'item, ou -1 si la commande est absente.
// ----------------------------------------------------------------------------
int FindCheckboxMenuPosition(HMENU menu, UINT command_id) {
    const int item_count = GetMenuItemCount(menu);
    for (int position = 0; position < item_count; ++position) {
        if (GetMenuItemID(menu, position) == command_id) {
            return position;
        }
    }
    return -1;
}

// ----------------------------------------------------------------------------
// Recherche la checkbox sous une position ecran.
//
// Parametres :
// - point : position ecran a tester.
//
// Retour :
// - etat de la checkbox, ou nullptr.
// ----------------------------------------------------------------------------
MenuCheckboxState* FindCheckboxAtPoint(POINT point) {
    for (MenuCheckboxState& checkbox : g_checkboxes.checkboxes) {
        if (checkbox.menu_hwnd != nullptr
            && IsWindowVisible(checkbox.menu_hwnd) != FALSE
            && !IsRectEmpty(&checkbox.item_rect_screen)
            && PtInRect(&checkbox.item_rect_screen, point) != FALSE) {
            const HMENU menu = reinterpret_cast<HMENU>(SendMessageW(
                checkbox.menu_hwnd,
                kMenuGetHandleMessage,
                0,
                0
            ));
            if (menu == nullptr
                || FindCheckboxMenuPosition(menu, checkbox.spec.command_id) < 0) {
                continue;
            }
            return &checkbox;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Cree une police de menu adaptee aux metriques systeme.
//
// Retour :
// - police GDI a liberer par l'appelant, ou nullptr.
// ----------------------------------------------------------------------------
HFONT CreateCheckboxMenuFont() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) == FALSE) {
        return nullptr;
    }
    return CreateFontIndirectW(&metrics.lfMenuFont);
}

// ----------------------------------------------------------------------------
// Calcule la largeur souhaitee d'une checkbox.
//
// Parametres :
// - checkbox : checkbox a mesurer.
//
// Retour :
// - largeur en pixels.
// ----------------------------------------------------------------------------
UINT MeasureCheckboxWidth(const MenuCheckboxState& checkbox) {
    HDC hdc = GetDC(nullptr);
    if (hdc == nullptr) {
        return kCheckboxItemMinWidth;
    }

    HFONT font = CreateCheckboxMenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(hdc, font) : nullptr;
    RECT checked_rect{0, 0, 0, 0};
    RECT unchecked_rect{0, 0, 0, 0};
    DrawTextW(hdc, checkbox.spec.checked_text.c_str(), -1, &checked_rect, DT_SINGLELINE | DT_CALCRECT);
    DrawTextW(hdc, checkbox.spec.unchecked_text.c_str(), -1, &unchecked_rect, DT_SINGLELINE | DT_CALCRECT);
    if (previous_font != nullptr) {
        SelectObject(hdc, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
    ReleaseDC(nullptr, hdc);

    const int width = std::max(checked_rect.right - checked_rect.left, unchecked_rect.right - unchecked_rect.left)
        + kCheckboxCheckColumnWidth
        + (kCheckboxTextPadding * 2);
    return static_cast<UINT>(std::max<int>(static_cast<int>(kCheckboxItemMinWidth), width));
}

// ----------------------------------------------------------------------------
// Redessine directement la zone ecran de la checkbox.
//
// Parametres :
// - checkbox : checkbox a redessiner.
// ----------------------------------------------------------------------------
void RepaintCheckboxOnScreen(const MenuCheckboxState& checkbox) {
    if (checkbox.menu_hwnd == nullptr || IsRectEmpty(&checkbox.item_rect_client)) {
        return;
    }

    InvalidateRect(checkbox.menu_hwnd, &checkbox.item_rect_client, FALSE);
    UpdateWindow(checkbox.menu_hwnd);
}

// ----------------------------------------------------------------------------
// Met a jour la checkbox survolee et redessine les items concernes.
//
// Parametres :
// - checkbox : checkbox actuellement sous la souris, ou nullptr.
// ----------------------------------------------------------------------------
void UpdateHoveredCheckbox(MenuCheckboxState* checkbox) {
    const UINT command_id = checkbox != nullptr ? checkbox->spec.command_id : 0;
    if (command_id == g_checkboxes.hovered_command_id) {
        return;
    }

    MenuCheckboxState* previous = FindCheckbox(g_checkboxes.hovered_command_id);
    g_checkboxes.hovered_command_id = command_id;
    if (previous != nullptr) {
        RepaintCheckboxOnScreen(*previous);
    }
    if (checkbox != nullptr) {
        RepaintCheckboxOnScreen(*checkbox);
    }
}

// ----------------------------------------------------------------------------
// Dessine une vraie checkbox carree dans la colonne gauche.
//
// Parametres :
// - hdc : contexte GDI cible.
// - rect : rectangle de l'item.
// - checked : indique si la checkbox est cochee.
// - selected : indique si la checkbox est survolee.
// - check_color : couleur de la coche.
// ----------------------------------------------------------------------------
void PaintCheckboxControl(
    HDC hdc,
    const RECT& rect,
    bool checked,
    bool selected,
    COLORREF check_color
) {
    const int center_y = rect.top + ((rect.bottom - rect.top) / 2);
    const int box_left = rect.left + kCheckboxTextPadding;
    const int box_top = center_y - (kCheckboxBoxSize / 2);
    const RECT box_rect{
        box_left,
        box_top,
        box_left + kCheckboxBoxSize,
        box_top + kCheckboxBoxSize,
    };
    const COLORREF box_color = checked
        ? (selected ? LightenControlColor(g_checkboxes.active_color, 24) : g_checkboxes.active_color)
        : (selected ? kMenuControlHotTrackColor : kMenuControlTrackColor);
    HBRUSH box_brush = CreateSolidBrush(checked ? box_color : RGB(0x00, 0x00, 0x00));
    HPEN box_pen = CreatePen(PS_SOLID, 1, box_color);
    HGDIOBJ previous_brush = SelectObject(hdc, checked ? box_brush : GetStockObject(NULL_BRUSH));
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

    HPEN pen = CreatePen(PS_SOLID, 2, check_color);
    previous_pen = SelectObject(hdc, pen);
    Polyline(hdc, points, static_cast<int>(_countof(points)));
    SelectObject(hdc, previous_pen);
    DeleteObject(pen);
}

// ----------------------------------------------------------------------------
// Dessine une checkbox dans un HDC.
//
// Parametres :
// - hdc : contexte GDI cible.
// - rect : rectangle de l'item.
// - checkbox : checkbox a dessiner.
// - selected : indique si l'item est survole par le menu.
// ----------------------------------------------------------------------------
void PaintCheckbox(HDC hdc, const RECT& rect, const MenuCheckboxState& checkbox, bool selected) {
    const bool dark_mode = IsNativeDarkModeEnabled();
    const COLORREF background_color = dark_mode ? kDarkMenuBackgroundColor : GetSysColor(COLOR_MENU);
    const COLORREF selected_background_color = dark_mode ? kDarkMenuHotBackgroundColor : GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF text_color = dark_mode ? kDarkMenuTextColor : GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);

    HBRUSH background_brush = CreateSolidBrush(selected ? selected_background_color : background_color);
    FillRect(hdc, &rect, background_brush);
    DeleteObject(background_brush);

    PaintCheckboxControl(
        hdc,
        rect,
        checkbox.spec.checked,
        selected,
        RGB(0xFF, 0xFF, 0xFF)
    );

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text_color);
    HFONT font = CreateCheckboxMenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(hdc, font) : nullptr;
    RECT text_rect{
        rect.left + kCheckboxLabelLeft,
        rect.top,
        rect.right - kCheckboxTextPadding,
        rect.bottom,
    };
    const std::wstring& text = checkbox.spec.checked ? checkbox.spec.checked_text : checkbox.spec.unchecked_text;
    DrawTextW(hdc, text.c_str(), -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    if (previous_font != nullptr) {
        SelectObject(hdc, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
}

// ----------------------------------------------------------------------------
// Applique le clic d'une checkbox et notifie l'application.
//
// Parametres :
// - checkbox : checkbox concernee.
// ----------------------------------------------------------------------------
void ApplyCheckboxClick(MenuCheckboxState& checkbox) {
    checkbox.spec.checked = !checkbox.spec.checked;

    if (g_checkboxes.hwnd != nullptr) {
        SendMessageW(g_checkboxes.hwnd, WM_COMMAND, static_cast<WPARAM>(checkbox.spec.command_id), 0);
    }

    RepaintCheckboxOnScreen(checkbox);
}

// ----------------------------------------------------------------------------
// Hook souris local utilise pendant TrackPopupMenu.
// ----------------------------------------------------------------------------
LRESULT CALLBACK MenuCheckboxMouseHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code < 0 || lparam == 0) {
        return CallNextHookEx(g_checkboxes.mouse_hook, code, wparam, lparam);
    }

    const auto* mouse = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    MenuCheckboxState* checkbox = FindCheckboxAtPoint(mouse->pt);
    if (wparam == WM_MOUSEMOVE) {
        UpdateHoveredCheckbox(checkbox);
    }
    if (wparam == WM_LBUTTONDOWN && checkbox != nullptr) {
        g_checkboxes.mouse_down_consumed = true;
        g_checkboxes.pressed_command_id = checkbox->spec.command_id;
        return 1;
    }
    if (wparam == WM_LBUTTONUP && g_checkboxes.mouse_down_consumed) {
        const UINT pressed_command_id = g_checkboxes.pressed_command_id;
        g_checkboxes.mouse_down_consumed = false;
        g_checkboxes.pressed_command_id = 0;
        if (checkbox != nullptr
            && pressed_command_id == checkbox->spec.command_id) {
            ApplyCheckboxClick(*checkbox);
        }
        return 1;
    }

    return CallNextHookEx(g_checkboxes.mouse_hook, code, wparam, lparam);
}

} // namespace

// ----------------------------------------------------------------------------
// Ajoute l'item owner-drawn d'une checkbox a un menu.
// ----------------------------------------------------------------------------
void AppendWidgetMenuCheckboxItem(HMENU menu, UINT command_id) {
    MenuCheckboxState* checkbox = FindCheckbox(command_id);
    const ULONG_PTR item_data = checkbox != nullptr
        ? reinterpret_cast<ULONG_PTR>(checkbox)
        : static_cast<ULONG_PTR>(command_id);
    AppendMenuW(menu, MF_OWNERDRAW, command_id, reinterpret_cast<LPCWSTR>(item_data));
}

// ----------------------------------------------------------------------------
// Active le suivi souris des checkboxes pendant l'affichage du menu.
// ----------------------------------------------------------------------------
void BeginWidgetMenuCheckboxTracking(
    HWND hwnd,
    const std::vector<WidgetMenuCheckboxSpec>& checkboxes,
    COLORREF active_color
) {
    EndWidgetMenuCheckboxTracking();
    g_checkboxes = MenuCheckboxesState{};
    g_checkboxes.hwnd = hwnd;
    g_checkboxes.active_color = active_color;
    g_checkboxes.checkboxes.reserve(checkboxes.size());
    for (const WidgetMenuCheckboxSpec& spec : checkboxes) {
        if (spec.mode == WidgetMenuCheckboxMode::Toggle) {
            g_checkboxes.checkboxes.push_back(MenuCheckboxState{spec});
        }
    }
    g_checkboxes.mouse_hook = SetWindowsHookExW(WH_MOUSE, MenuCheckboxMouseHook, nullptr, GetCurrentThreadId());
}

// ----------------------------------------------------------------------------
// Desactive le suivi souris des checkboxes.
// ----------------------------------------------------------------------------
void EndWidgetMenuCheckboxTracking() {
    if (g_checkboxes.mouse_hook != nullptr) {
        UnhookWindowsHookEx(g_checkboxes.mouse_hook);
    }
    g_checkboxes = MenuCheckboxesState{};
}

// ----------------------------------------------------------------------------
// Retraduit les libelles des checkboxes du menu encore ouvert.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeWidgetMenuCheckboxes(UiLanguage previous_language) {
    for (MenuCheckboxState& checkbox : g_checkboxes.checkboxes) {
        checkbox.spec.checked_text = RelocalizeText(
            checkbox.spec.checked_text,
            previous_language
        );
        checkbox.spec.unchecked_text = RelocalizeText(
            checkbox.spec.unchecked_text,
            previous_language
        );
        if (checkbox.spec.command_id == kCommandToggleQuotaWeekly) {
            checkbox.spec.checked_text = T(IDS_MENU_QUOTA_WEEKLY);
            checkbox.spec.unchecked_text = checkbox.spec.checked_text;
        }
    }
}

// ----------------------------------------------------------------------------
// Met a jour une checkbox visible sans emettre sa commande.
// ----------------------------------------------------------------------------
void UpdateWidgetMenuCheckboxChecked(UINT command_id, bool checked) {
    MenuCheckboxState* checkbox = FindCheckbox(command_id);
    if (checkbox == nullptr) {
        return;
    }

    checkbox->spec.checked = checked;
    RepaintCheckboxOnScreen(*checkbox);
}

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les checkboxes.
// ----------------------------------------------------------------------------
bool MeasureWidgetMenuCheckbox(MEASUREITEMSTRUCT* measure_item) {
    if (measure_item == nullptr || measure_item->CtlType != ODT_MENU) {
        return false;
    }

    MenuCheckboxState* checkbox = FindCheckboxFromMenuItem(measure_item->itemID, measure_item->itemData);
    if (checkbox == nullptr) {
        return false;
    }

    measure_item->itemWidth = MeasureCheckboxWidth(*checkbox);
    measure_item->itemHeight = kCheckboxItemHeight;
    return true;
}

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les checkboxes.
// ----------------------------------------------------------------------------
bool DrawWidgetMenuCheckbox(DRAWITEMSTRUCT* draw_item) {
    if (draw_item == nullptr || draw_item->CtlType != ODT_MENU) {
        return false;
    }

    MenuCheckboxState* checkbox = FindCheckboxFromMenuItem(draw_item->itemID, draw_item->itemData);
    if (checkbox == nullptr) {
        return false;
    }

    checkbox->item_rect_client = draw_item->rcItem;
    checkbox->menu_hwnd = WindowFromDC(draw_item->hDC);
    if (checkbox->menu_hwnd != nullptr) {
        const HMENU menu = reinterpret_cast<HMENU>(SendMessageW(
            checkbox->menu_hwnd,
            kMenuGetHandleMessage,
            0,
            0
        ));
        const int position = FindCheckboxMenuPosition(menu, checkbox->spec.command_id);
        if (position >= 0) {
            GetMenuItemRect(
                nullptr,
                menu,
                static_cast<UINT>(position),
                &checkbox->item_rect_screen
            );
        }
    }

    const bool selected = (draw_item->itemState & ODS_SELECTED) != 0
        || g_checkboxes.hovered_command_id == checkbox->spec.command_id;
    PaintCheckbox(draw_item->hDC, draw_item->rcItem, *checkbox, selected);
    return true;
}

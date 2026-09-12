// ============================================================================
// Codex Glass - Rendu owner-draw du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier gere les donnees temporaires, la mesure, la peinture et le
// polissage Win32 des menus owner-draw. La composition des menus reste separee.
// ============================================================================

#include "WidgetMenuOwnerDraw.h"

#include "../localization/Localization.h"
#include "../window/WidgetWindow.h"

#include <dwmapi.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

// Attribut DWM qui force le theme sombre sur une fenetre native.
constexpr DWORD kDwmMenuUseImmersiveDarkModeAttribute = 20;

// Attribut DWM qui controle l'arrondi des coins d'une fenetre.
constexpr DWORD kDwmMenuWindowCornerPreferenceAttribute = 33;

// Preference DWM qui demande des coins arrondis.
constexpr DWORD kDwmMenuWindowCornerRoundPreference = 2;

// Largeur minimale des items owner-draw du menu.
constexpr UINT kOwnerMenuItemMinWidth = 254;

// Largeur minimale d'une liste compacte ajustee a ses libelles.
constexpr UINT kOwnerMenuCompactItemMinWidth = 84;

// Hauteur des commandes owner-draw du menu.
constexpr UINT kOwnerMenuItemHeight = 26;

// Hauteur des separateurs owner-draw du menu.
constexpr UINT kOwnerMenuSeparatorHeight = 9;

// Largeur de la colonne de coche du menu.
constexpr int kOwnerMenuCheckColumnWidth = 28;

// Largeur de la colonne de fleche du menu.
constexpr int kOwnerMenuArrowColumnWidth = 26;

// Marge horizontale du texte dans le menu.
constexpr int kOwnerMenuTextPadding = 8;

// Marge horizontale des separateurs du menu.
constexpr int kOwnerMenuSeparatorPadding = 12;

// Rayon des fonds de survol dans le menu.
constexpr int kOwnerMenuHotRadius = 5;

// Rayon demande pour les fenetres popup de menu.
constexpr int kOwnerMenuWindowCornerRadius = 6;

// Identifiant du timer qui force le premier dessin complet d'une popup.
constexpr UINT_PTR kOwnerMenuInitialPaintTimerId = 0xC0DE;

// Couleur de fond sombre du menu.
constexpr COLORREF kOwnerMenuDarkBackground = RGB(0x2B, 0x2B, 0x2B);

// Couleur du survol sombre du menu.
constexpr COLORREF kOwnerMenuDarkHotBackground = RGB(0x3A, 0x3A, 0x3A);

// Couleur du texte sombre du menu.
constexpr COLORREF kOwnerMenuDarkText = RGB(0xF3, 0xF3, 0xF3);

// Couleur du texte desactive sombre du menu.
constexpr COLORREF kOwnerMenuDarkDisabledText = RGB(0x92, 0x92, 0x92);

// Couleur des separateurs sombres du menu.
constexpr COLORREF kOwnerMenuDarkSeparator = RGB(0x46, 0x46, 0x46);

// Type d'item owner-draw dans le menu contextuel.
enum class OwnerMenuItemKind {
    Command,
    SubMenu,
    Separator,
};

// Donnees attachees aux items owner-draw du menu.
struct OwnerMenuItemData {
    OwnerMenuItemKind kind = OwnerMenuItemKind::Command;
    std::wstring text{};
    bool checked = false;
    bool enabled = true;
    bool compact = false;
};

// Etat du rendu owner-draw pendant l'affichage du menu.
struct OwnerMenuDrawState {
    std::vector<std::unique_ptr<OwnerMenuItemData>> items{};
    HHOOK menu_window_hook = nullptr;
    HHOOK menu_message_hook = nullptr;
    HWND polishing_window = nullptr;
};

OwnerMenuDrawState g_owner_menu_draw{};

// ----------------------------------------------------------------------------
// Force le dessin complet une fois la boucle modale du menu active.
//
// Parametres :
// - hwnd : fenetre popup a redessiner.
// - message : message timer transmis par Win32.
// - timer_id : identifiant du timer a arreter.
// - tick_count : horodatage Win32 non utilise.
// ----------------------------------------------------------------------------
void CALLBACK OwnerMenuInitialPaintTimer(HWND hwnd, UINT message, UINT_PTR timer_id, DWORD tick_count) {
    (void)message;
    (void)tick_count;
    KillTimer(hwnd, timer_id);
    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW
    );
}

// ----------------------------------------------------------------------------
// Indique si le menu contextuel doit etre dessine en sombre.
//
// Retour :
// - true si Windows demande le theme sombre.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsOwnerMenuDarkMode() {
    return IsNativeDarkModeEnabled();
}

// ----------------------------------------------------------------------------
// Renvoie la couleur de fond courante du menu owner-draw.
//
// Retour :
// - couleur de fond selon le theme systeme.
// ----------------------------------------------------------------------------
COLORREF OwnerMenuBackgroundColor() {
    return IsOwnerMenuDarkMode() ? kOwnerMenuDarkBackground : GetSysColor(COLOR_MENU);
}

// ----------------------------------------------------------------------------
// Renvoie la couleur de survol courante du menu owner-draw.
//
// Retour :
// - couleur de survol selon le theme systeme.
// ----------------------------------------------------------------------------
COLORREF OwnerMenuHotBackgroundColor() {
    return IsOwnerMenuDarkMode() ? kOwnerMenuDarkHotBackground : GetSysColor(COLOR_HIGHLIGHT);
}

// ----------------------------------------------------------------------------
// Renvoie la couleur de texte courante du menu owner-draw.
//
// Parametres :
// - enabled : indique si l'item est actif.
// - compact : indique si la largeur doit suivre le libelle.
// - selected : indique si l'item est survole.
//
// Retour :
// - couleur de texte selon l'etat et le theme.
// ----------------------------------------------------------------------------
COLORREF OwnerMenuTextColor(bool enabled, bool selected) {
    if (IsOwnerMenuDarkMode()) {
        return enabled ? kOwnerMenuDarkText : kOwnerMenuDarkDisabledText;
    }
    if (!enabled) {
        return GetSysColor(COLOR_GRAYTEXT);
    }
    return GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT);
}

// ----------------------------------------------------------------------------
// Renvoie la couleur courante des separateurs du menu.
//
// Retour :
// - couleur de separateur selon le theme systeme.
// ----------------------------------------------------------------------------
COLORREF OwnerMenuSeparatorColor() {
    return IsOwnerMenuDarkMode() ? kOwnerMenuDarkSeparator : GetSysColor(COLOR_3DLIGHT);
}

// ----------------------------------------------------------------------------
// Cree une police de menu adaptee aux metriques systeme.
//
// Retour :
// - police GDI a liberer par l'appelant, ou nullptr.
// ----------------------------------------------------------------------------
HFONT CreateOwnerMenuFont() {
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) == FALSE) {
        return nullptr;
    }
    return CreateFontIndirectW(&metrics.lfMenuFont);
}

// ----------------------------------------------------------------------------
// Ajoute une donnee owner-draw conservee pendant TrackPopupMenu.
//
// Parametres :
// - kind : type d'item.
// - text : libelle de l'item.
// - checked : indique si l'item est coche.
// - enabled : indique si l'item est actif.
//
// Retour :
// - pointeur stable tant que le menu est affiche.
// ----------------------------------------------------------------------------
OwnerMenuItemData* AddOwnerMenuItemData(
    OwnerMenuItemKind kind,
    const wchar_t* text,
    bool checked,
    bool enabled,
    bool compact = false
) {
    auto item = std::make_unique<OwnerMenuItemData>();
    item->kind = kind;
    item->text = text != nullptr ? text : L"";
    item->checked = checked;
    item->enabled = enabled;
    item->compact = compact;
    OwnerMenuItemData* raw_item = item.get();
    g_owner_menu_draw.items.push_back(std::move(item));
    return raw_item;
}

// ----------------------------------------------------------------------------
// Retrouve une donnee owner-draw depuis le pointeur Win32.
//
// Parametres :
// - item_data : donnees transmises par WM_MEASUREITEM/WM_DRAWITEM.
//
// Retour :
// - item owner-draw, ou nullptr.
// ----------------------------------------------------------------------------
OwnerMenuItemData* FindOwnerMenuItemData(ULONG_PTR item_data) {
    if (item_data == 0) {
        return nullptr;
    }

    for (const auto& item : g_owner_menu_draw.items) {
        if (reinterpret_cast<ULONG_PTR>(item.get()) == item_data) {
            return item.get();
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// Calcule la largeur souhaitee d'un item owner-draw.
//
// Parametres :
// - item : item a mesurer.
//
// Retour :
// - largeur en pixels.
// ----------------------------------------------------------------------------
UINT MeasureOwnerMenuItemWidth(const OwnerMenuItemData& item) {
    const UINT minimum_width = item.compact
        ? kOwnerMenuCompactItemMinWidth
        : kOwnerMenuItemMinWidth;
    if (item.kind == OwnerMenuItemKind::Separator) {
        return minimum_width;
    }

    HDC hdc = GetDC(nullptr);
    if (hdc == nullptr) {
        return minimum_width;
    }

    HFONT font = CreateOwnerMenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(hdc, font) : nullptr;
    RECT text_rect{0, 0, 0, 0};
    DrawTextW(hdc, item.text.c_str(), -1, &text_rect, DT_SINGLELINE | DT_CALCRECT);
    if (previous_font != nullptr) {
        SelectObject(hdc, previous_font);
    }
    if (font != nullptr) {
        DeleteObject(font);
    }
    ReleaseDC(nullptr, hdc);

    const int width = (text_rect.right - text_rect.left)
        + kOwnerMenuCheckColumnWidth
        + (item.compact ? 0 : kOwnerMenuArrowColumnWidth)
        + (kOwnerMenuTextPadding * 2);
    return static_cast<UINT>(std::max<int>(static_cast<int>(minimum_width), width));
}

// ----------------------------------------------------------------------------
// Configure une fenetre de popup menu pour l'apparence moderne.
//
// Parametres :
// - hwnd : fenetre native de classe #32768 creee par Win32.
// ----------------------------------------------------------------------------
void PolishOwnerMenuWindow(HWND hwnd) {
    if (hwnd == nullptr || g_owner_menu_draw.polishing_window == hwnd) {
        return;
    }
    g_owner_menu_draw.polishing_window = hwnd;

    if (IsOwnerMenuDarkMode()) {
        const BOOL dark_mode = TRUE;
        DwmSetWindowAttribute(
            hwnd,
            kDwmMenuUseImmersiveDarkModeAttribute,
            &dark_mode,
            sizeof(dark_mode)
        );
    }

    const DWORD corner_preference = kDwmMenuWindowCornerRoundPreference;
    DwmSetWindowAttribute(
        hwnd,
        kDwmMenuWindowCornerPreferenceAttribute,
        &corner_preference,
        sizeof(corner_preference)
    );

    RECT rect{};
    if (GetWindowRect(hwnd, &rect) != FALSE && rect.right > rect.left + 8 && rect.bottom > rect.top + 8) {
        const int width = std::max(1L, rect.right - rect.left);
        const int height = std::max(1L, rect.bottom - rect.top);
        HRGN rounded_region = CreateRoundRectRgn(
            0,
            0,
            width + 1,
            height + 1,
            kOwnerMenuWindowCornerRadius,
            kOwnerMenuWindowCornerRadius
        );
        if (rounded_region != nullptr) {
            SetWindowRgn(hwnd, rounded_region, TRUE);
            SetTimer(hwnd, kOwnerMenuInitialPaintTimerId, 1, OwnerMenuInitialPaintTimer);
        }
    }
    g_owner_menu_draw.polishing_window = nullptr;
}

// ----------------------------------------------------------------------------
// Hook CBT local qui repere les fenetres de menu Win32.
// ----------------------------------------------------------------------------
LRESULT CALLBACK OwnerMenuWindowHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HCBT_ACTIVATE) {
        HWND hwnd = reinterpret_cast<HWND>(wparam);
        wchar_t class_name[32]{};
        if (GetClassNameW(hwnd, class_name, static_cast<int>(_countof(class_name))) != 0
            && wcscmp(class_name, L"#32768") == 0) {
            PolishOwnerMenuWindow(hwnd);
        }
    }

    return CallNextHookEx(g_owner_menu_draw.menu_window_hook, code, wparam, lparam);
}

// ----------------------------------------------------------------------------
// Indique si une fenetre est un popup menu Win32.
//
// Parametres :
// - hwnd : fenetre a tester.
//
// Retour :
// - true si la classe native est #32768.
// ----------------------------------------------------------------------------
bool IsNativeMenuWindow(HWND hwnd) {
    if (hwnd == nullptr) {
        return false;
    }

    wchar_t class_name[32]{};
    return GetClassNameW(hwnd, class_name, static_cast<int>(_countof(class_name))) != 0
        && wcscmp(class_name, L"#32768") == 0;
}

// ----------------------------------------------------------------------------
// Hook local qui reapplique l'arrondi apres le positionnement final du menu.
// ----------------------------------------------------------------------------
LRESULT CALLBACK OwnerMenuMessageHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0 && lparam != 0) {
        const auto* message = reinterpret_cast<CWPSTRUCT*>(lparam);
        if ((message->message == WM_WINDOWPOSCHANGED || message->message == WM_SHOWWINDOW)
            && IsNativeMenuWindow(message->hwnd)) {
            PolishOwnerMenuWindow(message->hwnd);
        }
    }

    return CallNextHookEx(g_owner_menu_draw.menu_message_hook, code, wparam, lparam);
}

// ----------------------------------------------------------------------------
// Active le polissage des fenetres de menu creees pendant TrackPopupMenu.
// ----------------------------------------------------------------------------
} // namespace

void BeginOwnerMenuWindowPolish() {
    if (g_owner_menu_draw.menu_window_hook != nullptr) {
        UnhookWindowsHookEx(g_owner_menu_draw.menu_window_hook);
    }
    if (g_owner_menu_draw.menu_message_hook != nullptr) {
        UnhookWindowsHookEx(g_owner_menu_draw.menu_message_hook);
    }
    g_owner_menu_draw.menu_window_hook = SetWindowsHookExW(WH_CBT, OwnerMenuWindowHook, nullptr, GetCurrentThreadId());
    g_owner_menu_draw.menu_message_hook =
        SetWindowsHookExW(WH_CALLWNDPROC, OwnerMenuMessageHook, nullptr, GetCurrentThreadId());
}

// ----------------------------------------------------------------------------
// Desactive le polissage des fenetres de menu.
// ----------------------------------------------------------------------------
void EndOwnerMenuWindowPolish() {
    if (g_owner_menu_draw.menu_window_hook != nullptr) {
        UnhookWindowsHookEx(g_owner_menu_draw.menu_window_hook);
        g_owner_menu_draw.menu_window_hook = nullptr;
    }
    if (g_owner_menu_draw.menu_message_hook != nullptr) {
        UnhookWindowsHookEx(g_owner_menu_draw.menu_message_hook);
        g_owner_menu_draw.menu_message_hook = nullptr;
    }
}

// ----------------------------------------------------------------------------
// Retraduit les items owner-draw et redessine les popups visibles.
//
// Parametres :
// - previous_language : langue effective des textes encore stockes.
// ----------------------------------------------------------------------------
void RelocalizeOwnerMenuItems(UiLanguage previous_language) {
    for (const auto& item : g_owner_menu_draw.items) {
        item->text = RelocalizeText(item->text, previous_language);
    }

    EnumThreadWindows(
        GetCurrentThreadId(),
        [](HWND hwnd, LPARAM) -> BOOL {
            if (IsNativeMenuWindow(hwnd)) {
                RedrawWindow(
                    hwnd,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW
                );
            }
            return TRUE;
        },
        0
    );
}

// ----------------------------------------------------------------------------
// Configure le fond d'un menu owner-draw.
//
// Parametres :
// - menu : menu Win32 a configurer.
// ----------------------------------------------------------------------------
void ConfigureOwnerMenuBackground(HMENU menu) {
    if (menu == nullptr) {
        return;
    }

    static HBRUSH dark_brush = CreateSolidBrush(kOwnerMenuDarkBackground);
    MENUINFO menu_info{};
    menu_info.cbSize = sizeof(menu_info);
    menu_info.fMask = MIM_BACKGROUND;
    menu_info.hbrBack = IsOwnerMenuDarkMode() ? dark_brush : GetSysColorBrush(COLOR_MENU);
    SetMenuInfo(menu, &menu_info);
}

// ----------------------------------------------------------------------------
// Cree un popup menu configure pour le rendu owner-draw.
//
// Retour :
// - handle du menu cree.
// ----------------------------------------------------------------------------
HMENU CreateOwnerPopupMenu() {
    HMENU menu = CreatePopupMenu();
    ConfigureOwnerMenuBackground(menu);
    return menu;
}

// ----------------------------------------------------------------------------
// Dessine une coche d'item de menu.
//
// Parametres :
// - hdc : contexte GDI cible.
// - rect : rectangle de l'item.
// - color : couleur de la coche.
// ----------------------------------------------------------------------------
void DrawOwnerMenuCheck(HDC hdc, const RECT& rect, COLORREF color) {
    const int center_y = rect.top + ((rect.bottom - rect.top) / 2);
    const int left = rect.left + 9;
    POINT points[]{
        POINT{left, center_y},
        POINT{left + 4, center_y + 4},
        POINT{left + 13, center_y - 5},
    };

    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ previous_pen = SelectObject(hdc, pen);
    Polyline(hdc, points, static_cast<int>(_countof(points)));
    SelectObject(hdc, previous_pen);
    DeleteObject(pen);
}

// ----------------------------------------------------------------------------
// Dessine un separateur de menu.
//
// Parametres :
// - draw_item : structure Win32 de dessin.
// ----------------------------------------------------------------------------
void DrawOwnerMenuSeparator(DRAWITEMSTRUCT* draw_item) {
    const RECT rect = draw_item->rcItem;
    HBRUSH background_brush = CreateSolidBrush(OwnerMenuBackgroundColor());
    FillRect(draw_item->hDC, &rect, background_brush);
    DeleteObject(background_brush);

    const int y = rect.top + ((rect.bottom - rect.top) / 2);
    HPEN pen = CreatePen(PS_SOLID, 1, OwnerMenuSeparatorColor());
    HGDIOBJ previous_pen = SelectObject(draw_item->hDC, pen);
    MoveToEx(draw_item->hDC, rect.left + kOwnerMenuSeparatorPadding, y, nullptr);
    LineTo(draw_item->hDC, rect.right - kOwnerMenuSeparatorPadding, y);
    SelectObject(draw_item->hDC, previous_pen);
    DeleteObject(pen);
}

// ----------------------------------------------------------------------------
// Dessine un item texte de menu.
//
// Parametres :
// - draw_item : structure Win32 de dessin.
// - item : donnees de l'item.
// ----------------------------------------------------------------------------
void DrawOwnerMenuTextItem(DRAWITEMSTRUCT* draw_item, const OwnerMenuItemData& item) {
    const RECT rect = draw_item->rcItem;
    const bool selected = item.enabled && ((draw_item->itemState & ODS_SELECTED) != 0);
    HBRUSH background_brush = CreateSolidBrush(OwnerMenuBackgroundColor());
    FillRect(draw_item->hDC, &rect, background_brush);
    DeleteObject(background_brush);

    if (selected) {
        RECT hot_rect{rect.left + 4, rect.top + 1, rect.right - 4, rect.bottom - 1};
        HBRUSH hot_brush = CreateSolidBrush(OwnerMenuHotBackgroundColor());
        HPEN hot_pen = CreatePen(PS_SOLID, 1, OwnerMenuHotBackgroundColor());
        HGDIOBJ previous_brush = SelectObject(draw_item->hDC, hot_brush);
        HGDIOBJ previous_pen = SelectObject(draw_item->hDC, hot_pen);
        RoundRect(
            draw_item->hDC,
            hot_rect.left,
            hot_rect.top,
            hot_rect.right,
            hot_rect.bottom,
            kOwnerMenuHotRadius,
            kOwnerMenuHotRadius
        );
        SelectObject(draw_item->hDC, previous_pen);
        SelectObject(draw_item->hDC, previous_brush);
        DeleteObject(hot_pen);
        DeleteObject(hot_brush);
    }

    const COLORREF text_color = OwnerMenuTextColor(item.enabled, selected);
    if (item.checked) {
        DrawOwnerMenuCheck(draw_item->hDC, rect, text_color);
    }

    SetBkMode(draw_item->hDC, TRANSPARENT);
    SetTextColor(draw_item->hDC, text_color);
    HFONT font = CreateOwnerMenuFont();
    HGDIOBJ previous_font = font != nullptr ? SelectObject(draw_item->hDC, font) : nullptr;
    RECT text_rect{
        rect.left + kOwnerMenuCheckColumnWidth + kOwnerMenuTextPadding,
        rect.top,
        rect.right - (item.compact ? kOwnerMenuTextPadding : kOwnerMenuArrowColumnWidth),
        rect.bottom,
    };
    DrawTextW(
        draw_item->hDC,
        item.text.c_str(),
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

    // Les menus Win32 conservent leur indicateur de sous-menu meme en owner-draw.
    // On reserve seulement l'espace afin d'eviter un double chevron.
}

// ----------------------------------------------------------------------------
// Ajoute une commande simple au menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// - command_id : identifiant de commande envoye par WM_COMMAND.
// - text : libelle affiche.
// - checked : indique si l'entree doit etre cochee.
// ----------------------------------------------------------------------------
void AppendCommandMenuItem(HMENU menu, UINT command_id, const wchar_t* text, bool checked) {
    OwnerMenuItemData* item = AddOwnerMenuItemData(OwnerMenuItemKind::Command, text, checked, true);
    AppendMenuW(
        menu,
        MF_OWNERDRAW | (checked ? MF_CHECKED : MF_UNCHECKED),
        command_id,
        reinterpret_cast<LPCWSTR>(item)
    );
}

// ----------------------------------------------------------------------------
// Ajoute un separateur owner-draw au menu.
//
// Parametres :
// - menu : menu Win32 a alimenter.
// ----------------------------------------------------------------------------
void AppendMenuSeparator(HMENU menu) {
    OwnerMenuItemData* item = AddOwnerMenuItemData(OwnerMenuItemKind::Separator, L"", false, false);
    AppendMenuW(
        menu,
        MF_OWNERDRAW | MF_DISABLED,
        0,
        reinterpret_cast<LPCWSTR>(item)
    );
}

// ----------------------------------------------------------------------------
// Ajoute un sous-menu au menu parent.
//
// Parametres :
// - menu : menu parent.
// - submenu : sous-menu deja cree.
// - text : libelle affiche pour le sous-menu.
// - enabled : indique si le sous-menu est utilisable.
// ----------------------------------------------------------------------------
void AppendSubMenu(HMENU menu, HMENU submenu, const wchar_t* text, bool enabled) {
    OwnerMenuItemData* item = AddOwnerMenuItemData(OwnerMenuItemKind::SubMenu, text, false, enabled);
    MENUITEMINFOW menu_item{};
    menu_item.cbSize = sizeof(menu_item);
    menu_item.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_DATA;
    menu_item.fType = MFT_OWNERDRAW;
    menu_item.fState = enabled ? MFS_ENABLED : MFS_GRAYED;
    menu_item.hSubMenu = submenu;
    menu_item.dwItemData = reinterpret_cast<ULONG_PTR>(item);
    InsertMenuItemW(menu, GetMenuItemCount(menu), TRUE, &menu_item);
}

// ----------------------------------------------------------------------------
// Ajoute une commande simple mesuree au plus pres de son libelle.
// ----------------------------------------------------------------------------
void AppendCompactCommandMenuItem(
    HMENU menu,
    UINT command_id,
    const wchar_t* text,
    bool checked
) {
    OwnerMenuItemData* item = AddOwnerMenuItemData(
        OwnerMenuItemKind::Command,
        text,
        checked,
        true,
        true
    );
    AppendMenuW(
        menu,
        MF_OWNERDRAW | (checked ? MF_CHECKED : MF_UNCHECKED),
        command_id,
        reinterpret_cast<LPCWSTR>(item)
    );
}

// ----------------------------------------------------------------------------
// Reinitialise l'etat temporaire des items owner-draw.
// ----------------------------------------------------------------------------
void ResetOwnerMenuDrawState() {
    g_owner_menu_draw = OwnerMenuDrawState{};
}

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les items owner-drawn du menu contextuel.
// ----------------------------------------------------------------------------
bool MeasureOwnerMenuItem(MEASUREITEMSTRUCT* measure_item) {
    if (measure_item == nullptr || measure_item->CtlType != ODT_MENU) {
        return false;
    }

    OwnerMenuItemData* item = FindOwnerMenuItemData(measure_item->itemData);
    if (item == nullptr) {
        return false;
    }

    measure_item->itemWidth = MeasureOwnerMenuItemWidth(*item);
    measure_item->itemHeight = item->kind == OwnerMenuItemKind::Separator
        ? kOwnerMenuSeparatorHeight
        : kOwnerMenuItemHeight;
    return true;
}

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les items owner-drawn du menu contextuel.
// ----------------------------------------------------------------------------
bool DrawOwnerMenuItem(DRAWITEMSTRUCT* draw_item) {
    if (draw_item == nullptr || draw_item->CtlType != ODT_MENU) {
        return false;
    }

    OwnerMenuItemData* item = FindOwnerMenuItemData(draw_item->itemData);
    if (item == nullptr) {
        return false;
    }

    if (item->kind == OwnerMenuItemKind::Separator) {
        DrawOwnerMenuSeparator(draw_item);
        return true;
    }

    DrawOwnerMenuTextItem(draw_item, *item);
    return true;
}

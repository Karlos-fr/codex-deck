// ============================================================================
// Codex Glass - Bandeau du menu contextuel
// ----------------------------------------------------------------------------
// Ce fichier isole le rendu du bandeau de marque et son suivi de curseur. Il
// reste independant de la construction metier des menus.
// ============================================================================

#include "WidgetMenuBanner.h"

#include "WidgetMenu.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <string>

namespace {

// Hauteur du bandeau de marque du menu.
constexpr UINT kMenuBannerHeight = 42;

// Largeur minimale du bandeau de marque du menu.
constexpr UINT kMenuBannerWidth = 282;

// Taille de l'icone affichee dans le bandeau.
constexpr int kMenuBannerIconSize = 24;

// Marge horizontale du bandeau.
constexpr int kMenuBannerHorizontalPadding = 12;

// Couleur de fond du bandeau.
constexpr COLORREF kMenuBannerBackground = RGB(0x18, 0x2A, 0x26);

// Couleur de fond du bandeau au survol.
constexpr COLORREF kMenuBannerHotBackground = RGB(0x20, 0x37, 0x31);

// Couleur du liseret inferieur du bandeau.
constexpr COLORREF kMenuBannerBorder = RGB(0x2F, 0x4A, 0x43);

// Couleur du texte du bandeau.
constexpr COLORREF kMenuBannerText = RGB(0xF4, 0xF7, 0xF5);

// Couleur du lien GitHub du bandeau.
constexpr COLORREF kMenuBannerLinkText = RGB(0xA9, 0xC8, 0xBE);

// Etat temporaire du curseur du bandeau pendant l'affichage du menu.
struct MenuBannerCursorState {
    HHOOK mouse_hook = nullptr;
    RECT banner_rect_screen{};
};

MenuBannerCursorState g_menu_banner_cursor{};

// ----------------------------------------------------------------------------
// Indique si le curseur est dans le bandeau.
//
// Parametres :
// - point : position ecran du curseur.
//
// Retour :
// - true si le curseur survole le bandeau cliquable.
// ----------------------------------------------------------------------------
bool IsPointInMenuBanner(POINT point) {
    return !IsRectEmpty(&g_menu_banner_cursor.banner_rect_screen)
        && PtInRect(&g_menu_banner_cursor.banner_rect_screen, point) != FALSE;
}

// ----------------------------------------------------------------------------
// Hook souris local qui adapte le curseur au bandeau.
// ----------------------------------------------------------------------------
LRESULT CALLBACK MenuBannerCursorMouseHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0 && lparam != 0 && wparam == WM_MOUSEMOVE) {
        const auto* mouse = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
        SetCursor(LoadCursorW(nullptr, IsPointInMenuBanner(mouse->pt) ? IDC_HAND : IDC_ARROW));
    }

    return CallNextHookEx(g_menu_banner_cursor.mouse_hook, code, wparam, lparam);
}

} // namespace

// ----------------------------------------------------------------------------
// Active le suivi du curseur du bandeau.
// ----------------------------------------------------------------------------
void BeginMenuBannerCursorTracking() {
    if (g_menu_banner_cursor.mouse_hook != nullptr) {
        UnhookWindowsHookEx(g_menu_banner_cursor.mouse_hook);
    }
    g_menu_banner_cursor = MenuBannerCursorState{};
    g_menu_banner_cursor.mouse_hook = SetWindowsHookExW(WH_MOUSE, MenuBannerCursorMouseHook, nullptr, GetCurrentThreadId());
}

// ----------------------------------------------------------------------------
// Desactive le suivi du curseur du bandeau.
// ----------------------------------------------------------------------------
void EndMenuBannerCursorTracking() {
    if (g_menu_banner_cursor.mouse_hook != nullptr) {
        UnhookWindowsHookEx(g_menu_banner_cursor.mouse_hook);
    }
    g_menu_banner_cursor = MenuBannerCursorState{};
}

// ----------------------------------------------------------------------------
// Mesure le bandeau de marque en tete du menu.
//
// Parametres :
// - measure_item : structure Win32 a renseigner.
//
// Retour :
// - true si la mesure correspond au bandeau.
// ----------------------------------------------------------------------------
bool MeasureMenuBanner(MEASUREITEMSTRUCT* measure_item) {
    if (measure_item == nullptr
        || measure_item->CtlType != ODT_MENU
        || measure_item->itemID != kCommandMenuBanner) {
        return false;
    }

    measure_item->itemWidth = kMenuBannerWidth;
    measure_item->itemHeight = kMenuBannerHeight;
    return true;
}

// ----------------------------------------------------------------------------
// Charge l'icone de l'application pour le bandeau du menu.
//
// Retour :
// - icone partagee ou icone systeme de secours.
// ----------------------------------------------------------------------------
HICON LoadMenuBannerIcon() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HICON icon = reinterpret_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        kMenuBannerIconSize,
        kMenuBannerIconSize,
        LR_SHARED | LR_DEFAULTCOLOR
    ));
    return icon != nullptr ? icon : LoadIconW(nullptr, IDI_APPLICATION);
}

// ----------------------------------------------------------------------------
// Dessine le bandeau de marque en tete du menu.
//
// Parametres :
// - draw_item : structure Win32 de dessin.
// ----------------------------------------------------------------------------
void DrawMenuBanner(DRAWITEMSTRUCT* draw_item) {
    const RECT rect = draw_item->rcItem;
    const bool selected = (draw_item->itemState & ODS_SELECTED) != 0;

    HBRUSH background_brush = CreateSolidBrush(selected ? kMenuBannerHotBackground : kMenuBannerBackground);
    FillRect(draw_item->hDC, &rect, background_brush);
    DeleteObject(background_brush);

    HBRUSH border_brush = CreateSolidBrush(kMenuBannerBorder);
    RECT border_rect{rect.left, rect.bottom - 1, rect.right, rect.bottom};
    FillRect(draw_item->hDC, &border_rect, border_brush);
    DeleteObject(border_brush);

    const int icon_left = rect.left + kMenuBannerHorizontalPadding;
    const int icon_top = rect.top + 8;
    DrawIconEx(
        draw_item->hDC,
        icon_left,
        icon_top,
        LoadMenuBannerIcon(),
        kMenuBannerIconSize,
        kMenuBannerIconSize,
        0,
        nullptr,
        DI_NORMAL
    );

    SetBkMode(draw_item->hDC, TRANSPARENT);
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    HFONT title_font = nullptr;
    HFONT link_font = nullptr;
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) != FALSE) {
        metrics.lfMenuFont.lfWeight = FW_SEMIBOLD;
        title_font = CreateFontIndirectW(&metrics.lfMenuFont);
        metrics.lfMenuFont.lfWeight = FW_NORMAL;
        metrics.lfMenuFont.lfHeight = static_cast<LONG>(static_cast<float>(metrics.lfMenuFont.lfHeight) * 0.86F);
        link_font = CreateFontIndirectW(&metrics.lfMenuFont);
    }

    HGDIOBJ previous_font = nullptr;
    if (title_font != nullptr) {
        previous_font = SelectObject(draw_item->hDC, title_font);
    }

    RECT title_rect{
        icon_left + kMenuBannerIconSize + 9,
        rect.top + 4,
        rect.right - kMenuBannerHorizontalPadding,
        rect.top + 21,
    };
    const std::wstring title = T(IDS_APP_TITLE) + L" (" + CODEX_DECK_FILE_VERSION_STRING + L")";
    SetTextColor(draw_item->hDC, kMenuBannerText);
    DrawTextW(
        draw_item->hDC,
        title.c_str(),
        -1,
        &title_rect,
        DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS
    );

    if (link_font != nullptr) {
        SelectObject(draw_item->hDC, link_font);
    }

    RECT link_rect{
        icon_left + kMenuBannerIconSize + 9,
        rect.top + 18,
        rect.right - kMenuBannerHorizontalPadding,
        rect.bottom - 3,
    };
    g_menu_banner_cursor.banner_rect_screen = rect;
    HWND menu_hwnd = WindowFromDC(draw_item->hDC);
    if (menu_hwnd != nullptr) {
        MapWindowPoints(menu_hwnd, nullptr, reinterpret_cast<POINT*>(&g_menu_banner_cursor.banner_rect_screen), 2);
    }

    SetTextColor(draw_item->hDC, kMenuBannerLinkText);
    DrawTextW(
        draw_item->hDC,
        kCodexGlassRepositoryLabel,
        -1,
        &link_rect,
        DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS
    );

    if (previous_font != nullptr) {
        SelectObject(draw_item->hDC, previous_font);
    }
    if (title_font != nullptr) {
        DeleteObject(title_font);
    }
    if (link_font != nullptr) {
        DeleteObject(link_font);
    }
}

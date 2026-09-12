// ============================================================================
// Codex Glass - Implementation de l'icone de notification Windows
// ----------------------------------------------------------------------------
// Ce fichier regroupe uniquement la gestion de l'icone tray et son etat local.
// Les actions declenchees par les messages tray restent orchestrees par
// WidgetApp afin de ne pas melanger shell et logique applicative.
// ============================================================================

#include "WidgetTrayIcon.h"

#include "../icon/AppIcon.h"
#include "WidgetShellMessages.h"

#include <shellapi.h>

#include <algorithm>
#include <string_view>

namespace {

// Identifiant de l'icone ajoutee dans la zone de notification.
constexpr UINT kTrayIconId = 1;

// Indique si l'icone de notification est actuellement installee.
bool g_tray_icon_added = false;

// ----------------------------------------------------------------------------
// Copie un texte borne dans le champ fixe de NOTIFYICONDATA.
//
// Parametres :
// - tooltip : texte source qui ne doit pas necessairement etre termine par zero.
// - destination : tableau Win32 rempli et termine par zero.
// ----------------------------------------------------------------------------
template <std::size_t Capacity>
void CopyTrayTooltip(
    std::wstring_view tooltip,
    wchar_t (&destination)[Capacity]
) {
    const std::size_t maximum_length = Capacity - 1U;
    const std::size_t copy_length = std::min(tooltip.size(), maximum_length);
    std::copy_n(tooltip.data(), copy_length, destination);
    destination[copy_length] = L'\0';
}

// ----------------------------------------------------------------------------
// Remplit la structure de notification utilisee par Shell_NotifyIcon.
//
// Parametres :
// - hwnd : handle de la fenetre qui recevra les messages de l'icone.
// - notify_icon_data : structure Win32 a initialiser.
// - tooltip : texte compact affiche au survol.
// ----------------------------------------------------------------------------
void InitializeNotifyIconData(
    HWND hwnd,
    NOTIFYICONDATAW& notify_icon_data,
    std::wstring_view tooltip
) {
    notify_icon_data = {};
    notify_icon_data.cbSize = sizeof(notify_icon_data);
    notify_icon_data.hWnd = hwnd;
    notify_icon_data.uID = kTrayIconId;
    notify_icon_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    notify_icon_data.uCallbackMessage = TrayIconMessage();
    notify_icon_data.hIcon = LoadTrayIcon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    CopyTrayTooltip(tooltip, notify_icon_data.szTip);
}

// ----------------------------------------------------------------------------
// Demande au shell d'utiliser le comportement moderne des icones de notification.
//
// Parametres :
// - notify_icon_data : structure deja associee a l'icone installee.
// ----------------------------------------------------------------------------
void SetNotifyIconVersion(NOTIFYICONDATAW& notify_icon_data) {
    notify_icon_data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &notify_icon_data);
}

} // namespace

// ----------------------------------------------------------------------------
// Ajoute l'icone de l'application dans la zone de notification Windows.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire de l'icone.
// - tooltip : texte compact affiche au survol de l'icone.
// ----------------------------------------------------------------------------
void AddTrayIcon(HWND hwnd, std::wstring_view tooltip) {
    if (g_tray_icon_added) {
        NOTIFYICONDATAW existing_icon_data{};
        InitializeNotifyIconData(hwnd, existing_icon_data, tooltip);
        if (Shell_NotifyIconW(NIM_MODIFY, &existing_icon_data) != FALSE) {
            SetNotifyIconVersion(existing_icon_data);
            return;
        }
    }

    NOTIFYICONDATAW notify_icon_data{};
    InitializeNotifyIconData(hwnd, notify_icon_data, tooltip);
    g_tray_icon_added = Shell_NotifyIconW(NIM_ADD, &notify_icon_data) != FALSE;
    if (g_tray_icon_added) {
        SetNotifyIconVersion(notify_icon_data);
    }
}

// ----------------------------------------------------------------------------
// Retire l'icone de l'application de la zone de notification Windows.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire de l'icone.
// ----------------------------------------------------------------------------
void RemoveTrayIcon(HWND hwnd) {
    if (!g_tray_icon_added) {
        return;
    }

    NOTIFYICONDATAW notify_icon_data{};
    InitializeNotifyIconData(hwnd, notify_icon_data, L"");
    Shell_NotifyIconW(NIM_DELETE, &notify_icon_data);
    g_tray_icon_added = false;
}

// ----------------------------------------------------------------------------
// Met a jour le libelle de l'icone de notification deja installee.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire de l'icone.
// - tooltip : nouveau texte compact affiche au survol de l'icone.
// ----------------------------------------------------------------------------
void RefreshTrayIcon(HWND hwnd, std::wstring_view tooltip) {
    if (!g_tray_icon_added) {
        return;
    }

    NOTIFYICONDATAW notify_icon_data{};
    InitializeNotifyIconData(hwnd, notify_icon_data, tooltip);
    Shell_NotifyIconW(NIM_MODIFY, &notify_icon_data);
}

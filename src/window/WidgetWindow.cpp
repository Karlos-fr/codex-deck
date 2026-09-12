// ============================================================================
// Codex Glass - Implementation de la fenetre Win32
// ----------------------------------------------------------------------------
// Ce fichier regroupe les helpers de fenetre native, DWM, dark mode,
// positionnement et DPI du widget.
// ============================================================================

#include "WidgetWindow.h"

#include "../icon/AppIcon.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../rendering/WidgetRenderProviderInfo.h"

#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>

namespace {

// Nom interne de la classe de fenetre Win32 utilisee par RegisterClassExW.
constexpr wchar_t kWindowClassName[] = L"CodexGlassWindowClass";

// Largeur initiale du widget en pixels.
constexpr int kInitialWindowWidth = 340;

// Hauteur initiale du widget avec sa ligne de statut basse en pixels.
constexpr int kInitialWindowHeight = 194;

// Largeur du widget minimal laissant un espace marque entre ses cartes KPI.
constexpr int kMinimalWindowWidth = 240;

// Hauteur du widget minimal assurant deux cartes KPI carrees.
constexpr int kMinimalWindowHeight = 164;

// Hauteur du mode minimal lorsque toutes les cartes sont masquees.
constexpr int kMinimalWindowEmptyHeight = 80;

// Hauteur d'une rangee de cartes du mode minimal.
constexpr int kMinimalQuotaRowHeight = 96;

// Espacement entre deux rangees du mode minimal.
constexpr int kMinimalQuotaRowGap = 12;

// Cote commun des cartes des modes horizontal et vertical.
constexpr int kOrientedQuotaCardSize = 96;

// Espacement commun entre deux cartes orientees.
constexpr int kOrientedQuotaCardGap = 12;

// Marge interieure reprise du mode Minimal sur chaque bord.
constexpr int kOrientedPanelPadding = 14;

// Numerateur du facteur de reduction des deux modes orientes.
constexpr int kOrientedScaleNumerator = 3;

// Denominateur du facteur de reduction des deux modes orientes.
constexpr int kOrientedScaleDenominator = 4;

// Longueur minimale lorsque tous les quotas sont masques.
constexpr int kOrientedEmptyWindowLength = 124;

// Marges entourant la ligne ou la colonne de cartes sur son axe long.
constexpr int kOrientedCardsMargin = 2 * kOrientedPanelPadding;

// Hauteur du titre compact avant application de la reduction.
constexpr int kOrientedTitleHeight = 24;

// Hauteur horizontale contenant le titre compact et une rangee reduite.
constexpr int kHorizontalWindowHeight = 111;

// Largeur verticale entourant une carte reduite de ses marges proportionnelles.
constexpr int kVerticalWindowWidth = 93;

// Hauteur du widget reservant un graphe complet, son switch et son statut bas.
constexpr int kGraphWindowHeight = 404;

// Hauteur logique ajoutee par ligne de limite supplementaire.
constexpr int kAdditionalUsageRowHeight = 58;

// Largeur minimale acceptee pour les modes non minimaux.
constexpr int kMinimumWindowWidth = 240;

// Preference DWM demandant des coins arrondis natifs et anticreneles.
constexpr DWM_WINDOW_CORNER_PREFERENCE kWindowCornerPreference = DWMWCP_ROUND;

// Attribut DWM demandant a Windows d'utiliser le theme sombre immersif.
constexpr DWORD kDwmUseImmersiveDarkModeAttribute = 20;

// Opacite minimale acceptee pour le fond du widget.
constexpr double kMinimumBackgroundOpacity = 0.20;

// Opacite maximale acceptee pour le fond du widget.
constexpr double kMaximumBackgroundOpacity = 1.00;

// DPI de reference utilise par les dimensions logiques du widget.
constexpr UINT kReferenceDpi = 96;

// ----------------------------------------------------------------------------
// Indique si le graphe compact doit etre dessine.
//
// Parametres :
// - settings : reglages d'affichage courants.
//
// Retour :
// - true si le mode complet et le graphe sont actifs.
// - false sinon.
// ----------------------------------------------------------------------------
bool ShouldRenderGraph(const AppSettings& settings) {
    return settings.display_mode == WidgetDisplayMode::Complete && settings.show_graph;
}

// ----------------------------------------------------------------------------
// Indique si le mode minimal est actif.
//
// Parametres :
// - settings : reglages d'affichage courants.
//
// Retour :
// - true si le widget doit afficher uniquement la synthese minimale.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsMinimalDisplayMode(const AppSettings& settings) {
    return settings.display_mode == WidgetDisplayMode::Minimal;
}

// ----------------------------------------------------------------------------
// Reduit une dimension logique de vingt-cinq pour cent en arrondissant au-dessus.
//
// Parametres :
// - value : dimension de reference avant reduction.
//
// Retour :
// - dimension entiere reduite sans rogner le contenu.
// ----------------------------------------------------------------------------
int ScaleOrientedDimension(int value) {
    return ((value * kOrientedScaleNumerator) + kOrientedScaleDenominator - 1)
        / kOrientedScaleDenominator;
}

// ----------------------------------------------------------------------------
// Calcule la largeur du mode horizontal selon les quotas visibles.
//
// Parametres :
// - snapshot : releve contenant les limites disponibles.
// - visibility : choix utilisateur des quotas affiches.
//
// Retour :
// - largeur horizontale en pixels logiques.
// ----------------------------------------------------------------------------
int HorizontalWindowWidth(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
) {
    const std::size_t visible_quota_count = CountVisibleQuotaRows(snapshot, visibility, true);
    if (visible_quota_count == 0) {
        return ScaleOrientedDimension(kOrientedEmptyWindowLength);
    }
    return ScaleOrientedDimension(kOrientedCardsMargin
        + static_cast<int>(visible_quota_count) * kOrientedQuotaCardSize
        + static_cast<int>(visible_quota_count - 1U) * kOrientedQuotaCardGap);
}

// ----------------------------------------------------------------------------
// Calcule la hauteur du mode vertical selon les quotas visibles.
//
// Parametres :
// - snapshot : releve contenant les limites disponibles.
// - visibility : choix utilisateur des quotas affiches.
//
// Retour :
// - hauteur verticale en pixels logiques.
// ----------------------------------------------------------------------------
int VerticalWindowHeight(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
) {
    const std::size_t visible_quota_count = CountVisibleQuotaRows(snapshot, visibility, true);
    if (visible_quota_count == 0) {
        return ScaleOrientedDimension(kOrientedEmptyWindowLength);
    }
    return ScaleOrientedDimension(
        kOrientedCardsMargin
        + kOrientedTitleHeight
        + static_cast<int>(visible_quota_count) * kOrientedQuotaCardSize
        + static_cast<int>(visible_quota_count - 1U) * kOrientedQuotaCardGap
    );
}

// ----------------------------------------------------------------------------
// Retourne le DPI associe a une fenetre ou au systeme avant creation.
//
// Parametres :
// - hwnd : fenetre a interroger, eventuellement nullptr.
//
// Retour :
// - DPI courant, ou le DPI de reference en secours.
// ----------------------------------------------------------------------------
UINT ResolveWindowDpi(HWND hwnd) {
    if (hwnd != nullptr) {
        const UINT window_dpi = GetDpiForWindow(hwnd);
        if (window_dpi != 0) {
            return window_dpi;
        }
    }

    const UINT system_dpi = GetDpiForSystem();
    return system_dpi == 0 ? kReferenceDpi : system_dpi;
}

// ----------------------------------------------------------------------------
// Convertit une dimension logique en pixels physiques pour un DPI donne.
//
// Parametres :
// - logical_size : taille logique exprimee pour 96 DPI.
// - dpi : DPI cible.
//
// Retour :
// - taille physique arrondie en pixels.
// ----------------------------------------------------------------------------
int ScaleLogicalSizeToPixels(int logical_size, UINT dpi) {
    return MulDiv(logical_size, static_cast<int>(dpi), static_cast<int>(kReferenceDpi));
}

// ----------------------------------------------------------------------------
// Liste les modes d'application sombres proposes par UxTheme.
// ----------------------------------------------------------------------------
enum class PreferredAppMode {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max,
};

// Fonction UxTheme indiquant si Windows prefere les applications sombres.
using ShouldAppsUseDarkModeFunction = bool(WINAPI*)();

// Fonction UxTheme autorisant le theme sombre pour une fenetre.
using AllowDarkModeForWindowFunction = bool(WINAPI*)(HWND, bool);

// Fonction UxTheme configurant le mode sombre des controles natifs.
using SetPreferredAppModeFunction = PreferredAppMode(WINAPI*)(PreferredAppMode);

// Fonction UxTheme vidant le cache des themes de menus.
using FlushMenuThemesFunction = void(WINAPI*)();

// ----------------------------------------------------------------------------
// Regroupe les fonctions UxTheme chargees dynamiquement.
// ----------------------------------------------------------------------------
struct NativeThemeApi {
    // Module uxtheme.dll charge dynamiquement.
    HMODULE module = nullptr;

    // Fonction Windows indiquant la preference sombre globale.
    ShouldAppsUseDarkModeFunction should_apps_use_dark_mode = nullptr;

    // Fonction Windows autorisant le theme sombre pour une fenetre.
    AllowDarkModeForWindowFunction allow_dark_mode_for_window = nullptr;

    // Fonction Windows choisissant le mode sombre applicatif.
    SetPreferredAppModeFunction set_preferred_app_mode = nullptr;

    // Fonction Windows vidant le cache des menus sombres.
    FlushMenuThemesFunction flush_menu_themes = nullptr;
};

// Fonctions UxTheme chargees une seule fois.
NativeThemeApi g_native_theme_api;

// Indique si les fonctions UxTheme ont deja ete tentees.
bool g_native_theme_api_loaded = false;

// ----------------------------------------------------------------------------
// Charge dynamiquement les fonctions non documentees du dark mode Win32.
//
// Retour :
// - structure contenant les fonctions disponibles.
// ----------------------------------------------------------------------------
NativeThemeApi& LoadNativeThemeApi() {
    if (g_native_theme_api_loaded) {
        return g_native_theme_api;
    }

    g_native_theme_api_loaded = true;
    g_native_theme_api.module = LoadLibraryW(L"uxtheme.dll");
    if (g_native_theme_api.module == nullptr) {
        return g_native_theme_api;
    }

    g_native_theme_api.should_apps_use_dark_mode = reinterpret_cast<ShouldAppsUseDarkModeFunction>(
        GetProcAddress(g_native_theme_api.module, MAKEINTRESOURCEA(132))
    );
    g_native_theme_api.allow_dark_mode_for_window = reinterpret_cast<AllowDarkModeForWindowFunction>(
        GetProcAddress(g_native_theme_api.module, MAKEINTRESOURCEA(133))
    );
    g_native_theme_api.set_preferred_app_mode = reinterpret_cast<SetPreferredAppModeFunction>(
        GetProcAddress(g_native_theme_api.module, MAKEINTRESOURCEA(135))
    );
    g_native_theme_api.flush_menu_themes = reinterpret_cast<FlushMenuThemesFunction>(
        GetProcAddress(g_native_theme_api.module, MAKEINTRESOURCEA(136))
    );

    return g_native_theme_api;
}

// ----------------------------------------------------------------------------
// Indique si Windows demande le theme sombre pour les applications.
//
// Retour :
// - true si le theme sombre systeme est actif.
// - false sinon ou si l'information est indisponible.
// ----------------------------------------------------------------------------
bool ShouldUseNativeDarkMode() {
    NativeThemeApi& theme_api = LoadNativeThemeApi();
    if (theme_api.should_apps_use_dark_mode == nullptr) {
        return false;
    }

    return theme_api.should_apps_use_dark_mode();
}

// ----------------------------------------------------------------------------
// Demande a DWM d'utiliser les coins arrondis natifs de Windows.
//
// Parametres :
// - hwnd : handle de la fenetre a decorer par DWM.
// ----------------------------------------------------------------------------
void ApplyDwmWindowCorners(HWND hwnd) {
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &kWindowCornerPreference,
        sizeof(kWindowCornerPreference)
    );
}

// ----------------------------------------------------------------------------
// Convertit l'opacite du reglage en alpha Win32.
//
// Parametres :
// - settings : reglages contenant l'opacite.
//
// Retour :
// - valeur d'alpha comprise entre 0 et 255.
// ----------------------------------------------------------------------------
BYTE WindowOpacityAlpha(const AppSettings& settings) {
    const double opacity = std::clamp(
        settings.background_opacity,
        kMinimumBackgroundOpacity,
        kMaximumBackgroundOpacity
    );
    return static_cast<BYTE>((opacity * 255.0) + 0.5);
}

// ----------------------------------------------------------------------------
// Applique l'icone applicative a une fenetre deja creee.
//
// Parametres :
// - hwnd : handle de la fenetre a mettre a jour.
// ----------------------------------------------------------------------------
void ApplyWindowIcons(HWND hwnd) {
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadApplicationIcon(0, 0)));
    SendMessageW(
        hwnd,
        WM_SETICON,
        ICON_SMALL,
        reinterpret_cast<LPARAM>(LoadApplicationIcon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)))
    );
}

// ----------------------------------------------------------------------------
// Verifie si un rectangle touche un ecran actuellement disponible.
//
// Parametres :
// - rect : rectangle de fenetre a verifier.
//
// Retour :
// - true si le rectangle est rattache a un moniteur.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsWindowRectOnVisibleMonitor(const RECT& rect) {
    return MonitorFromRect(&rect, MONITOR_DEFAULTTONULL) != nullptr;
}

// ----------------------------------------------------------------------------
// Ajuste un rectangle pour rester dans la zone de travail du moniteur cible.
//
// Parametres :
// - requested_rect : rectangle demande.
//
// Retour :
// - rectangle decale si necessaire.
// ----------------------------------------------------------------------------
RECT FitWindowRectToMonitorWorkArea(const RECT& requested_rect) {
    RECT adjusted_rect = requested_rect;
    HMONITOR monitor = MonitorFromRect(&adjusted_rect, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
        return adjusted_rect;
    }

    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
        return adjusted_rect;
    }

    const RECT work_area = monitor_info.rcWork;
    const int width = adjusted_rect.right - adjusted_rect.left;
    const int height = adjusted_rect.bottom - adjusted_rect.top;

    if (adjusted_rect.right > work_area.right) {
        adjusted_rect.left = work_area.right - width;
        adjusted_rect.right = work_area.right;
    }

    if (adjusted_rect.left < work_area.left) {
        adjusted_rect.left = work_area.left;
        adjusted_rect.right = adjusted_rect.left + width;
    }

    if (adjusted_rect.bottom > work_area.bottom) {
        adjusted_rect.top = work_area.bottom - height;
        adjusted_rect.bottom = work_area.bottom;
    }

    if (adjusted_rect.top < work_area.top) {
        adjusted_rect.top = work_area.top;
        adjusted_rect.bottom = adjusted_rect.top + height;
    }

    return adjusted_rect;
}

} // namespace

// ----------------------------------------------------------------------------
// Enregistre la classe Win32 de la fenetre principale.
//
// Parametres :
// - instance : handle de l'instance courante de l'application.
// - window_proc : procedure de fenetre qui traitera les messages.
//
// Retour :
// - true si la classe de fenetre a ete enregistree correctement.
// - false si l'enregistrement a echoue.
// ----------------------------------------------------------------------------
bool RegisterMainWindowClass(HINSTANCE instance, WNDPROC window_proc) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadApplicationIcon(0, 0);
    window_class.hIconSm = LoadApplicationIcon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    window_class.lpszClassName = kWindowClassName;

    return RegisterClassExW(&window_class) != 0;
}

// ----------------------------------------------------------------------------
// Cree la fenetre principale de l'application.
//
// Parametres :
// - instance : handle de l'instance courante de l'application.
// - settings : reglages initiaux de la fenetre.
// - create_parameter : pointeur transmis a WM_NCCREATE.
//
// Retour :
// - handle de la fenetre creee, ou nullptr en cas d'erreur.
// ----------------------------------------------------------------------------
HWND CreateMainWindow(HINSTANCE instance, const AppSettings& settings, void* create_parameter) {
    const RECT initial_rect = settings.window_rect;
    const DWORD topmost_style = settings.always_on_top ? WS_EX_TOPMOST : 0;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | topmost_style,
        kWindowClassName,
        T(IDS_APP_TITLE).c_str(),
        WS_POPUP,
        initial_rect.left,
        initial_rect.top,
        initial_rect.right - initial_rect.left,
        initial_rect.bottom - initial_rect.top,
        nullptr,
        nullptr,
        instance,
        create_parameter
    );

    if (hwnd) {
        ApplyWindowIcons(hwnd);
        ApplyNativeDarkMode(hwnd);
        ApplyDwmWindowCorners(hwnd);
        ApplyClickThroughSetting(hwnd, settings);
    }

    return hwnd;
}

// ----------------------------------------------------------------------------
// Calcule une position initiale visible sur l'ecran principal.
//
// Retour :
// - rectangle initial conseille pour le widget.
// ----------------------------------------------------------------------------
RECT CalculateInitialWindowRect() {
    RECT work_area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);

    // Marge entre le widget et le bord droit de la zone de travail.
    constexpr int right_margin = 32;

    // Marge entre le widget et le bord superieur de la zone de travail.
    constexpr int top_margin = 48;

    const UINT dpi = ResolveWindowDpi(nullptr);
    const int initial_width = ScaleLogicalSizeToPixels(kInitialWindowWidth, dpi);
    const int initial_height = ScaleLogicalSizeToPixels(kInitialWindowHeight, dpi);
    const int left = work_area.right - initial_width - right_margin;
    const int top = work_area.top + top_margin;

    return RECT{
        left,
        top,
        left + initial_width,
        top + initial_height,
    };
}

// ----------------------------------------------------------------------------
// Retourne un rectangle visible, en remplacant un ancien ecran disparu.
//
// Parametres :
// - requested_rect : rectangle charge depuis les reglages.
//
// Retour :
// - rectangle conserve s'il est visible, sinon rectangle initial par defaut.
// ----------------------------------------------------------------------------
RECT EnsureWindowRectVisible(const RECT& requested_rect) {
    if (IsWindowRectOnVisibleMonitor(requested_rect)) {
        return requested_rect;
    }

    return CalculateInitialWindowRect();
}

// ----------------------------------------------------------------------------
// Replace le widget si son rectangle courant n'est plus sur un ecran visible.
//
// Parametres :
// - hwnd : handle de la fenetre a verifier.
// - applied_rect : rectangle applique si la fenetre a ete deplacee.
//
// Retour :
// - true si la fenetre a ete replacee.
// - false sinon.
// ----------------------------------------------------------------------------
bool EnsureCurrentWindowVisible(HWND hwnd, RECT& applied_rect) {
    RECT window_rect{};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return false;
    }

    const RECT visible_rect = EnsureWindowRectVisible(window_rect);
    if (visible_rect.left == window_rect.left
        && visible_rect.top == window_rect.top
        && visible_rect.right == window_rect.right
        && visible_rect.bottom == window_rect.bottom) {
        return false;
    }

    SetWindowPos(
        hwnd,
        nullptr,
        visible_rect.left,
        visible_rect.top,
        visible_rect.right - visible_rect.left,
        visible_rect.bottom - visible_rect.top,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
    applied_rect = visible_rect;
    return true;
}

// ----------------------------------------------------------------------------
// Applique le rectangle conseille par Windows apres un changement de DPI.
//
// Parametres :
// - hwnd : handle de la fenetre a repositionner.
// - suggested_rect : rectangle recommande par le message WM_DPICHANGED.
//
// Retour :
// - true si le rectangle a ete applique.
// - false sinon.
// ----------------------------------------------------------------------------
bool ApplyDpiSuggestedRect(HWND hwnd, const RECT* suggested_rect) {
    if (suggested_rect == nullptr) {
        return false;
    }

    SetWindowPos(
        hwnd,
        nullptr,
        suggested_rect->left,
        suggested_rect->top,
        suggested_rect->right - suggested_rect->left,
        suggested_rect->bottom - suggested_rect->top,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
    return true;
}

// ----------------------------------------------------------------------------
// Retourne la hauteur de fenetre adaptee au mode courant.
//
// Parametres :
// - settings : reglages d'affichage courants.
// - snapshot : releve utilise pour compter les quotas visibles.
//
// Retour :
// - hauteur en pixels.
// ----------------------------------------------------------------------------
int PreferredWindowHeight(const AppSettings& settings, const UsageSnapshot& snapshot) {
    if (IsMinimalDisplayMode(settings)) {
        const std::size_t visible_quota_count = CountVisibleQuotaRows(
            snapshot,
            settings.quota_visibility,
            true
        );
        if (visible_quota_count == 0) {
            return kMinimalWindowEmptyHeight;
        }
        const std::size_t row_count = (visible_quota_count + 1U) / 2U;
        return kMinimalWindowHeight
            + static_cast<int>(row_count - 1U)
                * (kMinimalQuotaRowHeight + kMinimalQuotaRowGap);
    }
    if (settings.display_mode == WidgetDisplayMode::Horizontal) {
        return kHorizontalWindowHeight;
    }
    if (settings.display_mode == WidgetDisplayMode::Vertical) {
        return VerticalWindowHeight(snapshot, settings.quota_visibility);
    }

    const int base_height_without_quota_rows = ShouldRenderGraph(settings)
        ? kGraphWindowHeight - (2 * kAdditionalUsageRowHeight)
        : kInitialWindowHeight - (2 * kAdditionalUsageRowHeight);
    const std::size_t visible_quota_rows = CountVisibleQuotaRows(
        snapshot,
        settings.quota_visibility,
        settings.display_mode == WidgetDisplayMode::Complete
    );
    return base_height_without_quota_rows
        + static_cast<int>(visible_quota_rows) * kAdditionalUsageRowHeight;
}

// ----------------------------------------------------------------------------
// Ajuste la hauteur de fenetre pour le mode d'affichage courant.
//
// Parametres :
// - hwnd : handle de la fenetre a redimensionner.
// - settings : reglages d'affichage courants.
// ----------------------------------------------------------------------------
void ApplyPreferredWindowHeight(HWND hwnd, const AppSettings& settings, const UsageSnapshot& snapshot) {
    RECT window_rect{};
    GetWindowRect(hwnd, &window_rect);
    const UINT dpi = ResolveWindowDpi(hwnd);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        window_rect.right - window_rect.left,
        ScaleLogicalSizeToPixels(PreferredWindowHeight(settings, snapshot), dpi),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
    );
}

// ----------------------------------------------------------------------------
// Retourne la largeur de fenetre adaptee au mode courant.
//
// Parametres :
// - settings : reglages d'affichage courants.
// - snapshot : releve utilise pour compter les quotas visibles.
//
// Retour :
// - largeur en pixels.
// ----------------------------------------------------------------------------
int PreferredWindowWidth(const AppSettings& settings, const UsageSnapshot& snapshot) {
    if (IsMinimalDisplayMode(settings)) {
        return kMinimalWindowWidth;
    }
    if (settings.display_mode == WidgetDisplayMode::Horizontal) {
        return HorizontalWindowWidth(snapshot, settings.quota_visibility);
    }
    if (settings.display_mode == WidgetDisplayMode::Vertical) {
        return kVerticalWindowWidth;
    }
    return kInitialWindowWidth;
}

// ----------------------------------------------------------------------------
// Ajuste la taille de fenetre pour le mode courant.
// ----------------------------------------------------------------------------
void ApplyPreferredWindowSize(HWND hwnd, const AppSettings& settings, const UsageSnapshot& snapshot) {
    RECT window_rect{};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return;
    }

    const UINT dpi = ResolveWindowDpi(hwnd);
    const int preferred_width = ScaleLogicalSizeToPixels(PreferredWindowWidth(settings, snapshot), dpi);
    const int preferred_height = ScaleLogicalSizeToPixels(PreferredWindowHeight(settings, snapshot), dpi);
    RECT preferred_rect{
        window_rect.left,
        window_rect.top,
        window_rect.left + preferred_width,
        window_rect.top + preferred_height,
    };
    preferred_rect = FitWindowRectToMonitorWorkArea(preferred_rect);

    SetWindowPos(
        hwnd,
        nullptr,
        preferred_rect.left,
        preferred_rect.top,
        preferred_rect.right - preferred_rect.left,
        preferred_rect.bottom - preferred_rect.top,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

// ----------------------------------------------------------------------------
// Applique les limites de taille adaptees au mode courant.
//
// Parametres :
// - min_max_info : structure Win32 a renseigner.
// - settings : reglages d'affichage courants.
// ----------------------------------------------------------------------------
void ApplyModeMinimumSize(
    HWND hwnd,
    MINMAXINFO* min_max_info,
    const AppSettings& settings,
    const UsageSnapshot& snapshot
) {
    if (min_max_info == nullptr) {
        return;
    }

    const UINT dpi = ResolveWindowDpi(hwnd);
    const bool fixed_compact_width = IsMinimalDisplayMode(settings)
        || settings.display_mode == WidgetDisplayMode::Horizontal
        || settings.display_mode == WidgetDisplayMode::Vertical;
    const int minimum_width = fixed_compact_width
        ? PreferredWindowWidth(settings, snapshot)
        : kMinimumWindowWidth;
    min_max_info->ptMinTrackSize.x = ScaleLogicalSizeToPixels(minimum_width, dpi);
    min_max_info->ptMinTrackSize.y = ScaleLogicalSizeToPixels(PreferredWindowHeight(settings, snapshot), dpi);
}

// ----------------------------------------------------------------------------
// Applique le reglage de premier plan a la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a mettre a jour.
// - always_on_top : indique si la fenetre doit rester au premier plan.
// ----------------------------------------------------------------------------
void ApplyAlwaysOnTopSetting(HWND hwnd, bool always_on_top) {
    SetWindowPos(
        hwnd,
        always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    );
}

// ----------------------------------------------------------------------------
// Applique les styles d'interaction et d'opacite a la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a mettre a jour.
// - settings : reglages d'interaction et d'opacite.
// ----------------------------------------------------------------------------
void ApplyClickThroughSetting(HWND hwnd, const AppSettings& settings) {
    LONG_PTR extended_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const bool glass_effect_mode = settings.glass_effect_mode == GlassEffectMode::Enabled;
    const bool needs_layered = settings.click_through || (!glass_effect_mode && settings.background_opacity < kMaximumBackgroundOpacity);

    if (needs_layered) {
        extended_style |= WS_EX_LAYERED;
    } else {
        extended_style &= ~WS_EX_LAYERED;
    }

    extended_style &= ~WS_EX_TRANSPARENT;

    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, extended_style);

    if (needs_layered) {
        SetLayeredWindowAttributes(hwnd, 0, WindowOpacityAlpha(settings), LWA_ALPHA);
    }
}

// ----------------------------------------------------------------------------
// Applique le dark mode natif a la fenetre et aux menus Win32.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void ApplyNativeDarkMode(HWND hwnd) {
    NativeThemeApi& theme_api = LoadNativeThemeApi();
    const bool use_dark_mode = ShouldUseNativeDarkMode();
    if (theme_api.set_preferred_app_mode != nullptr) {
        theme_api.set_preferred_app_mode(PreferredAppMode::AllowDark);
    }

    if (theme_api.allow_dark_mode_for_window != nullptr) {
        theme_api.allow_dark_mode_for_window(hwnd, use_dark_mode);
    }

    const BOOL dark_mode_value = use_dark_mode ? TRUE : FALSE;
    DwmSetWindowAttribute(
        hwnd,
        kDwmUseImmersiveDarkModeAttribute,
        &dark_mode_value,
        sizeof(dark_mode_value)
    );

    SetWindowTheme(hwnd, use_dark_mode ? L"DarkMode_Explorer" : L"Explorer", nullptr);

    if (theme_api.flush_menu_themes != nullptr) {
        theme_api.flush_menu_themes();
    }
}

// ----------------------------------------------------------------------------
// Indique si Windows demande le theme sombre natif.
//
// Retour :
// - true si le theme sombre systeme est actif.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsNativeDarkModeEnabled() {
    return ShouldUseNativeDarkMode();
}

// ============================================================================
// Codex Deck - Implementation de la resolution de theme
// ----------------------------------------------------------------------------
// Ce fichier lit le theme applicatif Windows et fournit les palettes concretes
// utilisees par le renderer natif.
// ============================================================================

#include "Theme.h"

#include <dwmapi.h>

namespace {

// Chemin registre du theme applicatif Windows.
constexpr wchar_t kPersonalizeRegistryPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

// Valeur registre indiquant si les applications utilisent le theme clair.
constexpr wchar_t kAppsUseLightThemeValue[] = L"AppsUseLightTheme";

// ----------------------------------------------------------------------------
// Cree une couleur Direct2D opaque.
//
// Parametres :
// - red : composante rouge normalisee.
// - green : composante verte normalisee.
// - blue : composante bleue normalisee.
//
// Retour :
// - couleur Direct2D opaque.
// ----------------------------------------------------------------------------
D2D1_COLOR_F Rgb(float red, float green, float blue) {
    return D2D1::ColorF(red, green, blue, 1.0F);
}

}  // namespace

// ----------------------------------------------------------------------------
// Resolut un mode utilisateur en theme concret.
// ----------------------------------------------------------------------------
ResolvedTheme ResolveTheme(ThemeMode requested, bool system_dark) {
    switch (requested) {
    case ThemeMode::Dark:
        return ResolvedTheme::Dark;
    case ThemeMode::Light:
        return ResolvedTheme::Light;
    case ThemeMode::System:
    default:
        return system_dark ? ResolvedTheme::Dark : ResolvedTheme::Light;
    }
}

// ----------------------------------------------------------------------------
// Indique si Windows demande le theme applicatif sombre.
// ----------------------------------------------------------------------------
bool IsSystemDarkTheme() {
    DWORD value = 1;
    DWORD byte_count = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kPersonalizeRegistryPath,
        kAppsUseLightThemeValue,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &byte_count
    );
    return status == ERROR_SUCCESS && value == 0;
}

// ----------------------------------------------------------------------------
// Applique les attributs systeme de fenetre pour un theme resolu.
// ----------------------------------------------------------------------------
void ApplySystemWindowTheme(HWND hwnd, ResolvedTheme theme) {
    const BOOL dark = theme == ResolvedTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

// ----------------------------------------------------------------------------
// Retourne la palette correspondant au theme resolu.
// ----------------------------------------------------------------------------
ThemePalette PaletteForTheme(ResolvedTheme theme) {
    if (theme == ResolvedTheme::Dark) {
        return ThemePalette{
            Rgb(0.07F, 0.08F, 0.10F),
            Rgb(0.12F, 0.14F, 0.17F),
            Rgb(0.17F, 0.19F, 0.23F),
            Rgb(0.25F, 0.28F, 0.33F),
            Rgb(0.92F, 0.95F, 0.98F),
            Rgb(0.56F, 0.62F, 0.70F),
            Rgb(0.22F, 0.58F, 0.91F),
            Rgb(0.20F, 0.72F, 0.46F),
            Rgb(0.94F, 0.68F, 0.24F),
            Rgb(0.90F, 0.26F, 0.32F),
        };
    }
    return ThemePalette{
        Rgb(0.96F, 0.97F, 0.98F),
        Rgb(1.0F, 1.0F, 1.0F),
        Rgb(0.90F, 0.93F, 0.96F),
        Rgb(0.76F, 0.80F, 0.86F),
        Rgb(0.09F, 0.11F, 0.14F),
        Rgb(0.39F, 0.45F, 0.53F),
        Rgb(0.03F, 0.38F, 0.77F),
        Rgb(0.05F, 0.55F, 0.30F),
        Rgb(0.74F, 0.44F, 0.05F),
        Rgb(0.76F, 0.12F, 0.18F),
    };
}

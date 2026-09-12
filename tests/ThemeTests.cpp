// ============================================================================
// Codex Deck - Tests de resolution du theme
// ----------------------------------------------------------------------------
// Ce fichier valide la selection pure Light/Dark sans lire le registre Windows.
// ============================================================================

#include "theme/Theme.h"

// ----------------------------------------------------------------------------
// Verifie la resolution des themes explicites et du mode systeme.
//
// Retour :
// - zero si toutes les combinaisons attendues sont stables.
// ----------------------------------------------------------------------------
int main() {
    if (ResolveTheme(ThemeMode::System, true) != ResolvedTheme::Dark) {
        return 1;
    }
    if (ResolveTheme(ThemeMode::System, false) != ResolvedTheme::Light) {
        return 2;
    }
    if (ResolveTheme(ThemeMode::Dark, false) != ResolvedTheme::Dark) {
        return 3;
    }
    if (ResolveTheme(ThemeMode::Light, true) != ResolvedTheme::Light) {
        return 4;
    }
    return 0;
}

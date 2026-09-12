// ============================================================================
// Codex Deck - Tests du layout principal
// ----------------------------------------------------------------------------
// Ce fichier valide la geometrie Tree + Workbench avec donnees numeriques
// pures, sans Direct2D ni fenetre Win32.
// ============================================================================

#include "ui/MainLayout.h"

#include <cmath>

namespace {

// ----------------------------------------------------------------------------
// Compare deux flottants avec tolerance.
//
// Parametres :
// - left : valeur gauche.
// - right : valeur droite.
//
// Retour :
// - true si les valeurs sont equivalentes pour le layout.
// ----------------------------------------------------------------------------
bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 0.01F;
}

// ----------------------------------------------------------------------------
// Calcule la largeur d'un rectangle.
//
// Parametres :
// - rect : rectangle source.
//
// Retour :
// - largeur positive ou nulle.
// ----------------------------------------------------------------------------
float Width(const LayoutRect& rect) {
    return rect.right - rect.left;
}

}  // namespace

// ----------------------------------------------------------------------------
// Execute les validations de layout principal.
//
// Retour :
// - zero si la geometrie respecte les contraintes V1.
// ----------------------------------------------------------------------------
int main() {
    const MainLayoutRects rects = ComputeMainLayout(SizeF{1440.0F, 900.0F}, 300.0F, 42.0F, 120.0F);
    if (!NearlyEqual(Width(rects.tree), 300.0F)) {
        return 1;
    }
    if (!NearlyEqual(Width(rects.workbench), 1140.0F)) {
        return 2;
    }
    if (rects.tree.right > rects.workbench.left || rects.workbench.left < rects.tree.right) {
        return 3;
    }
    if (!NearlyEqual(rects.activity_bar.bottom, 42.0F)) {
        return 4;
    }
    if (!NearlyEqual(Width(ComputeMainLayout(SizeF{1000.0F, 700.0F}, 100.0F, 42.0F, 100.0F).tree), 220.0F)) {
        return 5;
    }
    return NearlyEqual(Width(ComputeMainLayout(SizeF{1000.0F, 700.0F}, 900.0F, 42.0F, 100.0F).tree), 480.0F) ? 0 : 6;
}

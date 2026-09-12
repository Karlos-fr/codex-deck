// ============================================================================
// Codex Deck - Tests de liste virtualisee
// ----------------------------------------------------------------------------
// Ce fichier valide les primitives de geometrie fixe sans creer de fenetre.
// ============================================================================

#include "ui/ScrollState.h"
#include "ui/VirtualListLayout.h"

// ----------------------------------------------------------------------------
// Verifie les plages visibles sur grand volume.
//
// Retour :
// - zero si les bornes sont stables et petites.
// ----------------------------------------------------------------------------
int TestVisibleRange() {
    const auto middle = ComputeVisibleRange(10'000, 30.0F, 12'000.0F, 600.0F, 2);
    if (middle.first >= middle.last) {
        return 1;
    }
    if ((middle.last - middle.first) > 25) {
        return 2;
    }
    if (middle.first != 398 || middle.last != 422) {
        return 3;
    }

    const auto top = ComputeVisibleRange(10'000, 30.0F, 0.0F, 600.0F, 2);
    if (top.first != 0 || top.last != 22) {
        return 4;
    }

    const auto bottom = ComputeVisibleRange(10'000, 30.0F, 299'700.0F, 600.0F, 2);
    if (bottom.first != 9'988 || bottom.last != 10'000) {
        return 5;
    }

    const auto empty = ComputeVisibleRange(10'000, 30.0F, 0.0F, 0.0F, 2);
    return empty.first == 0 && empty.last == 0 ? 0 : 6;
}

// ----------------------------------------------------------------------------
// Verifie clamp, scroll et ensure-visible.
//
// Retour :
// - zero si l'etat de scroll reste borne.
// ----------------------------------------------------------------------------
int TestScrollState() {
    ScrollState scroll{};
    scroll.content_extent = 3000.0F;
    scroll.viewport_extent = 600.0F;

    scroll.ScrollBy(2500.0F);
    if (scroll.offset != 2400.0F) {
        return 10;
    }

    scroll.ScrollBy(-3000.0F);
    if (scroll.offset != 0.0F) {
        return 11;
    }

    scroll.EnsureVisible(30, 30.0F);
    if (scroll.offset != 330.0F) {
        return 12;
    }

    scroll.EnsureVisible(15, 30.0F);
    if (scroll.offset != 330.0F) {
        return 13;
    }

    scroll.EnsureVisible(5, 30.0F);
    return scroll.offset == 150.0F ? 0 : 14;
}

// ----------------------------------------------------------------------------
// Execute les tests de virtualisation.
//
// Retour :
// - zero si tous les scenarios passent.
// ----------------------------------------------------------------------------
int main() {
    if (const int result = TestVisibleRange(); result != 0) {
        return result;
    }
    if (const int result = TestScrollState(); result != 0) {
        return result;
    }
    return 0;
}

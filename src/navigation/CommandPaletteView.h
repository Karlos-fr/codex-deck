// ============================================================================
// Codex Deck - Vue de Command Palette
// ----------------------------------------------------------------------------
// Ce module dessine l'overlay de palette sur une surface Direct2D composee par
// DirectComposition, sans executer directement les commandes.
// ============================================================================

#pragma once

#include "CommandPaletteModel.h"

#include "../theme/ThemePalette.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <memory>
#include <span>

// ----------------------------------------------------------------------------
// Dessine l'overlay de Command Palette.
// ----------------------------------------------------------------------------
class CommandPaletteView {
public:
    // ------------------------------------------------------------------------
    // Cree une vue sans ressources natives.
    // ------------------------------------------------------------------------
    CommandPaletteView();

    // ------------------------------------------------------------------------
    // Libere les ressources opaques.
    // ------------------------------------------------------------------------
    ~CommandPaletteView();

    // ------------------------------------------------------------------------
    // Dessine l'overlay et les entrees visibles.
    //
    // Parametres :
    // - dc : contexte Direct2D cible.
    // - bounds : rectangle total de fenetre.
    // - query : texte de recherche courant.
    // - entries : entrees scorees.
    // - selected_index : entree selectionnee.
    // - palette : palette resolue.
    // ------------------------------------------------------------------------
    void Render(
        ID2D1DeviceContext* dc,
        const D2D1_RECT_F& bounds,
        std::wstring_view query,
        std::span<const PaletteEntry> entries,
        std::size_t selected_index,
        const ThemePalette& palette
    );
};

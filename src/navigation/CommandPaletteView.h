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
#include <optional>
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
    // - cursor_index : position du caret dans la requete.
    // - mode : contenu fonctionnel affiche.
    // - caret_visible : indique si le caret doit etre dessine.
    // - palette : palette resolue.
    // ------------------------------------------------------------------------
    void Render(
        ID2D1DeviceContext* dc,
        const D2D1_RECT_F& bounds,
        std::wstring_view query,
        std::span<const PaletteEntry> entries,
        std::size_t selected_index,
        std::size_t cursor_index,
        CommandPaletteMode mode,
        bool caret_visible,
        const ThemePalette& palette
    );

    // ------------------------------------------------------------------------
    // Retourne l'entree touchee dans la fenetre actuellement visible.
    // ------------------------------------------------------------------------
    std::optional<std::size_t> HitTestEntry(
        const D2D1_RECT_F& bounds,
        std::size_t entry_count,
        std::size_t selected_index,
        float x,
        float y
    ) const;

    // ------------------------------------------------------------------------
    // Indique si un point touche le champ de saisie de la palette.
    // ------------------------------------------------------------------------
    bool IsPointOnQuery(const D2D1_RECT_F& bounds, std::size_t entry_count, float x, float y) const;

    // ------------------------------------------------------------------------
    // Indique si un point appartient au panneau de palette.
    // ------------------------------------------------------------------------
    bool Contains(const D2D1_RECT_F& bounds, std::size_t entry_count, float x, float y) const;
};

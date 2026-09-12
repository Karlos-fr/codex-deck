// ============================================================================
// Codex Deck - Vue Direct2D de la barre d'activite
// ----------------------------------------------------------------------------
// Ce module dessine une barre compacte de compteurs globaux. Il ne modifie pas
// le catalogue et ne lance aucune commande Codex.
// ============================================================================

#pragma once

#include "ActivityBarModel.h"

#include "../theme/ThemePalette.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <memory>

// ----------------------------------------------------------------------------
// Dessine la barre d'activite compacte.
// ----------------------------------------------------------------------------
class ActivityBarView {
public:
    // ------------------------------------------------------------------------
    // Cree une vue sans ressources natives.
    // ------------------------------------------------------------------------
    ActivityBarView();

    // ------------------------------------------------------------------------
    // Libere les ressources opaques.
    // ------------------------------------------------------------------------
    ~ActivityBarView();

    // ------------------------------------------------------------------------
    // Dessine les compteurs d'activite.
    //
    // Parametres :
    // - dc : contexte Direct2D cible.
    // - bounds : rectangle de rendu.
    // - counts : compteurs a afficher.
    // - active_filter : filtre actif.
    // - palette : palette resolue.
    // ------------------------------------------------------------------------
    void Render(
        ID2D1DeviceContext* dc,
        const D2D1_RECT_F& bounds,
        const ActivityCounts& counts,
        SessionFilter active_filter,
        const ThemePalette& palette
    );
};

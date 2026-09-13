// ============================================================================
// Codex Deck - Vue de l'overlay nouvelle session
// ----------------------------------------------------------------------------
// Ce module dessine le formulaire global sans lancer de picker, de RPC ou de
// persistance.
// ============================================================================

#pragma once

#include "NewSessionOverlayModel.h"

#include "../theme/ThemePalette.h"

#include <d2d1_1.h>

// Zone interactive du formulaire de creation.
enum class NewSessionOverlayHitTarget {
    None,
    Project,
    Workspace,
    Browse,
    Model,
    Prompt,
    Cancel,
    Create,
};

// Vue Direct2D du formulaire global de creation.
class NewSessionOverlayView {
public:
    // Dessine le formulaire et son champ actif.
    void Render(ID2D1DeviceContext* context, const D2D1_RECT_F& bounds, const NewSessionOverlayModel& model, const ThemePalette& palette);

    // Retourne la zone interactive situee sous un point client.
    NewSessionOverlayHitTarget HitTest(const D2D1_RECT_F& bounds, float x, float y) const;
};

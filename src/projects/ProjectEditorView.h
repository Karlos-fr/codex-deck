// ============================================================================
// Codex Deck - Vue de l'editeur de projet
// ----------------------------------------------------------------------------
// Ce module dessine les overlays create/rename/delete d'un projet logique sans
// acceder au stockage ni au selecteur de dossier.
// ============================================================================

#pragma once

#include "ProjectEditorModel.h"

#include "../theme/ThemePalette.h"

#include <d2d1_1.h>

// Zone interactive de l'editeur de projet.
enum class ProjectEditorHitTarget {
    None,
    Name,
    Cancel,
    Submit,
};

// Vue Direct2D compacte de gestion d'un projet.
class ProjectEditorView {
public:
    // Dessine l'overlay correspondant au modele courant.
    void Render(ID2D1DeviceContext* context, const D2D1_RECT_F& bounds, const ProjectEditorModel& model, const ThemePalette& palette);

    // Retourne la zone interactive sous un point client.
    ProjectEditorHitTarget HitTest(const D2D1_RECT_F& bounds, const ProjectEditorModel& model, float x, float y) const;
};

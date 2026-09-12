// ============================================================================
// Codex Deck - Implementation de l'etat de defilement
// ----------------------------------------------------------------------------
// Ce fichier borne le scroll et deplace le viewport seulement lorsque la ligne
// cible n'est plus visible.
// ============================================================================

#include "ScrollState.h"

#include <algorithm>

// ----------------------------------------------------------------------------
// Borne le decalage courant.
// ----------------------------------------------------------------------------
void ScrollState::Clamp() {
    const float maximum = std::max(0.0F, content_extent - viewport_extent);
    offset = std::clamp(offset, 0.0F, maximum);
}

// ----------------------------------------------------------------------------
// Applique un delta de scroll puis borne le resultat.
// ----------------------------------------------------------------------------
void ScrollState::ScrollBy(float delta) {
    offset += delta;
    Clamp();
}

// ----------------------------------------------------------------------------
// Rend une ligne visible si elle est hors viewport.
// ----------------------------------------------------------------------------
void ScrollState::EnsureVisible(std::size_t index, float row_height) {
    if (row_height <= 0.0F || viewport_extent <= 0.0F) {
        return;
    }

    const float row_top = static_cast<float>(index) * row_height;
    const float row_bottom = row_top + row_height;
    if (row_top < offset) {
        offset = row_top;
    } else if (row_bottom > offset + viewport_extent) {
        offset = row_bottom - viewport_extent;
    }
    Clamp();
}

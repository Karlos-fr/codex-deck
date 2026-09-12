// ============================================================================
// Codex Deck - Tests de geometrie de la fenetre principale
// ----------------------------------------------------------------------------
// Ce fichier verifie les contrats purs de taille minimale sans creer de fenetre.
// ============================================================================

#include "window/DeckWindow.h"

// ----------------------------------------------------------------------------
// Verifie la taille minimale du client principal.
//
// Retour :
// - zero si le contrat de taille est stable, un sinon.
// ----------------------------------------------------------------------------
int main() {
    const SIZE minimum = DeckMinimumClientSize();
    if (minimum.cx != 960 || minimum.cy != 640) {
        return 1;
    }
    return 0;
}

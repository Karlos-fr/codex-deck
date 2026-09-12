// ============================================================================
// Codex Deck - Orchestration applicative
// ----------------------------------------------------------------------------
// Ce module pilote le cycle de vie Win32 de la coquille. Il delegue la creation
// de fenetre a DeckWindow et le dessin a DeckRenderer.
// ============================================================================

#pragma once

#include "../rendering/DeckRenderer.h"
#include "../settings/DeckSettings.h"
#include "../theme/Theme.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Application native principale de Codex Deck.
// ----------------------------------------------------------------------------
class DeckApp {
public:
    // ------------------------------------------------------------------------
    // Libere les ressources applicatives possedees.
    // ------------------------------------------------------------------------
    ~DeckApp();

    // ------------------------------------------------------------------------
    // Lance la boucle de messages de l'application.
    //
    // Parametres :
    // - instance : instance du module executable.
    // - command_show : mode d'affichage initial demande par Windows.
    //
    // Retour :
    // - code de sortie du processus.
    // ------------------------------------------------------------------------
    int Run(HINSTANCE instance, int command_show);

    // ------------------------------------------------------------------------
    // Procedure Win32 statique redirigeant vers l'instance applicative.
    // ------------------------------------------------------------------------
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    // ------------------------------------------------------------------------
    // Traite un message Win32 pour la fenetre principale.
    // ------------------------------------------------------------------------
    LRESULT HandleWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    // ------------------------------------------------------------------------
    // Recalcule le theme courant et applique les attributs systeme.
    // ------------------------------------------------------------------------
    void RefreshTheme(HWND hwnd);

    // Renderer Direct2D minimal de la coquille.
    DeckRenderer renderer_;

    // Etat visuel affiche par le renderer.
    DeckVisualState visual_state_{};

    // Reglages locaux de la coquille.
    DeckSettings settings_{};

    // Theme concret actuellement applique.
    ResolvedTheme resolved_theme_ = ResolvedTheme::Light;
};

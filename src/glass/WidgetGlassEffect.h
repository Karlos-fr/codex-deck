// ============================================================================
// Codex Glass - Coordinateur GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare le runtime GlassEffect. Il coordonne le renderer
// D3D11 et la capture, sans dessiner directement dans le widget.
// ============================================================================

#pragma once

#include "WidgetGlassEffectCapture.h"
#include "WidgetGlassEffectRenderer.h"

struct WidgetGlassEffectFrame;
struct HWND__;
using HWND = HWND__*;

// ----------------------------------------------------------------------------
// Coordonne le pipeline GlassEffect.
// ----------------------------------------------------------------------------
class WidgetGlassEffect {
public:
    // ------------------------------------------------------------------------
    // Initialise paresseusement le runtime GlassEffect.
    //
    // Retour :
    // - true si le runtime est pret.
    // - false si le socle D3D11 n'est pas disponible.
    // ------------------------------------------------------------------------
    bool EnsureInitialized(HWND hwnd);

    // ------------------------------------------------------------------------
    // Capture le fond hors du chemin de peinture principal.
    // ------------------------------------------------------------------------
    bool TickCapture();

    // ------------------------------------------------------------------------
    // Associe une fenetre compagne a la capture DXGI principale.
    //
    // Parametres :
    // - hwnd : fenetre compagne, ou nullptr pour la retirer.
    // ------------------------------------------------------------------------
    void SetCompanionWindow(HWND hwnd);

    // ------------------------------------------------------------------------
    // Retourne la derniere frame capturee disponible pour le rendu.
    //
    // Retour :
    // - pointeur non possede, ou nullptr si aucune frame n'est disponible.
    // ------------------------------------------------------------------------
    const WidgetGlassEffectFrame* LatestFrame() const;

    // ------------------------------------------------------------------------
    // Retourne la frame capturee pour la fenetre compagne.
    // ------------------------------------------------------------------------
    const WidgetGlassEffectFrame* LatestCompanionFrame() const;

    // ------------------------------------------------------------------------
    // Libere les ressources GlassEffect.
    // ------------------------------------------------------------------------
    void Shutdown();

    // ------------------------------------------------------------------------
    // Indique si le runtime GlassEffect est initialise.
    //
    // Retour :
    // - true si EnsureInitialized a reussi.
    // ------------------------------------------------------------------------
    bool IsInitialized() const;

private:
    WidgetGlassEffectRenderer renderer_;
    WidgetGlassEffectCapture capture_;
};

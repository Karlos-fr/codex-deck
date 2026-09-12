// ============================================================================
// Codex Glass - Capture GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare le module de capture DXGI du fond du widget. Il garde la
// derniere capture dans une texture GPU possedee par l'application.
// ============================================================================

#pragma once

#include <memory>

struct WidgetGlassEffectFrame;

struct HWND__;
using HWND = HWND__*;

struct ID3D11Device;
struct ID3D11DeviceContext;

// ----------------------------------------------------------------------------
// Capture le fond ecran autour du widget hors du chemin WM_PAINT.
// ----------------------------------------------------------------------------
class WidgetGlassEffectCapture {
public:
    // ------------------------------------------------------------------------
    // Cree le module sans ouvrir de duplication DXGI.
    // ------------------------------------------------------------------------
    WidgetGlassEffectCapture();

    // ------------------------------------------------------------------------
    // Detruit le module et libere les ressources DXGI.
    // ------------------------------------------------------------------------
    ~WidgetGlassEffectCapture();

    // ------------------------------------------------------------------------
    // Initialise la capture avec les ressources D3D11 partagees.
    //
    // Parametres :
    // - hwnd : fenetre du widget a echantillonner.
    // - device : device D3D11 utilise pour les textures possedees.
    // - context : contexte immediat utilise pour copier les crops.
    //
    // Retour :
    // - true si la capture peut etre tentee.
    // ------------------------------------------------------------------------
    bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);

    // ------------------------------------------------------------------------
    // Capture le fond courant autour du widget.
    //
    // Retour :
    // - true si une texture possedee a ete mise a jour.
    // ------------------------------------------------------------------------
    bool CaptureNextFrame();

    // ------------------------------------------------------------------------
    // Definit une fenetre compagne capturee depuis la meme frame DXGI.
    //
    // Parametres :
    // - hwnd : fenetre compagne, ou nullptr pour la retirer.
    // ------------------------------------------------------------------------
    void SetCompanionWindow(HWND hwnd);

    // ------------------------------------------------------------------------
    // Retourne le dernier snapshot GPU disponible.
    //
    // Retour :
    // - pointeur non possede, ou nullptr si aucune frame n'est disponible.
    // ------------------------------------------------------------------------
    const WidgetGlassEffectFrame* LatestFrame() const;

    // ------------------------------------------------------------------------
    // Retourne le dernier snapshot GPU de la fenetre compagne.
    //
    // Retour :
    // - pointeur non possede, ou nullptr si aucune frame n'est disponible.
    // ------------------------------------------------------------------------
    const WidgetGlassEffectFrame* LatestCompanionFrame() const;

    // ------------------------------------------------------------------------
    // Libere les ressources de capture.
    // ------------------------------------------------------------------------
    void Shutdown();

    // ------------------------------------------------------------------------
    // Indique si une capture est active.
    //
    // Retour :
    // - true si une texture capturee est disponible.
    // ------------------------------------------------------------------------
    bool IsActive() const;

    struct Impl;

private:
    // Implementation privee contenant les types DXGI/D3D11.
    std::unique_ptr<Impl> impl_;
};

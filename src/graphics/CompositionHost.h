// ============================================================================
// Codex Deck - Hote DirectComposition
// ----------------------------------------------------------------------------
// Ce module possede le pipeline D3D11, DXGI, Direct2D et DirectComposition. Il
// expose seulement un contexte Direct2D pret a dessiner au renderer.
// ============================================================================

#pragma once

#include "GraphicsError.h"

#include <d2d1_1.h>
#include <windows.h>

#include <expected>

// ----------------------------------------------------------------------------
// Gere la surface composee utilisee par la fenetre principale.
// ----------------------------------------------------------------------------
class CompositionHost {
public:
    // ------------------------------------------------------------------------
    // Cree un hote sans allouer de ressources graphiques.
    // ------------------------------------------------------------------------
    CompositionHost() = default;

    // ------------------------------------------------------------------------
    // Libere les ressources graphiques possedees.
    // ------------------------------------------------------------------------
    ~CompositionHost();

    CompositionHost(const CompositionHost&) = delete;
    CompositionHost& operator=(const CompositionHost&) = delete;

    // ------------------------------------------------------------------------
    // Initialise les devices graphiques et la cible DirectComposition.
    //
    // Parametres :
    // - hwnd : fenetre Win32 recevant la composition.
    //
    // Retour :
    // - succes vide ou erreur structuree.
    // ------------------------------------------------------------------------
    std::expected<void, GraphicsError> Initialize(HWND hwnd);

    // ------------------------------------------------------------------------
    // Adapte la swap chain a la taille cliente courante.
    //
    // Parametres :
    // - width : largeur en pixels.
    // - height : hauteur en pixels.
    // - dpi : densite courante en points par pouce.
    // ------------------------------------------------------------------------
    void Resize(UINT width, UINT height, float dpi);

    // ------------------------------------------------------------------------
    // Prepare le contexte Direct2D pour un dessin.
    //
    // Retour :
    // - contexte Direct2D non possede ou erreur structuree.
    // ------------------------------------------------------------------------
    std::expected<ID2D1DeviceContext*, GraphicsError> BeginDraw();

    // ------------------------------------------------------------------------
    // Termine le dessin et presente la swap chain.
    //
    // Retour :
    // - succes vide ou erreur structuree.
    // ------------------------------------------------------------------------
    std::expected<void, GraphicsError> EndDraw();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

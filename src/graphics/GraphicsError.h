// ============================================================================
// Codex Deck - Erreurs graphiques natives
// ----------------------------------------------------------------------------
// Ce module decrit les echecs du pipeline D3D11, Direct2D et DirectComposition.
// Il ne depend pas de l'interface utilisateur.
// ============================================================================

#pragma once

#include <string>

// ----------------------------------------------------------------------------
// Liste les familles d'erreurs du pipeline graphique.
// ----------------------------------------------------------------------------
enum class GraphicsErrorCode {
    // Echec de creation du device D3D11.
    D3DDeviceCreation,

    // Echec de creation du device ou contexte Direct2D.
    D2DDeviceCreation,

    // Echec de creation ou redimensionnement de la swap chain.
    SwapChainCreation,

    // Echec de creation du device DirectComposition.
    CompositionDeviceCreation,

    // Echec de creation de la cible DirectComposition liee a HWND.
    CompositionTargetCreation,

    // Echec de liaison de la surface DXGI au contexte Direct2D.
    SurfaceBinding,

    // Echec de presentation de la swap chain.
    PresentFailed,
};

// ----------------------------------------------------------------------------
// Transporte une erreur graphique structuree.
// ----------------------------------------------------------------------------
struct GraphicsError {
    // Famille de l'erreur.
    GraphicsErrorCode code;

    // HRESULT Windows associe a l'echec.
    long hresult;

    // Message court destine au diagnostic local.
    std::wstring message;
};

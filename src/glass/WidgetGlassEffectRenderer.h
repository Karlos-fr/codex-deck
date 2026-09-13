// ============================================================================
// Codex Glass - Renderer GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare le socle D3D11 du futur rendu GlassEffect. En phase F, il
// initialise seulement le device et ne dessine aucun pixel capture.
// ============================================================================

#pragma once

#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;

// ----------------------------------------------------------------------------
// Gere les ressources D3D11 de base du pipeline GlassEffect.
// ----------------------------------------------------------------------------
class WidgetGlassEffectRenderer {
public:
    // ------------------------------------------------------------------------
    // Cree un renderer sans initialiser D3D11.
    // ------------------------------------------------------------------------
    WidgetGlassEffectRenderer();

    // ------------------------------------------------------------------------
    // Detruit le renderer et ses ressources D3D11.
    // ------------------------------------------------------------------------
    ~WidgetGlassEffectRenderer();

    // ------------------------------------------------------------------------
    // Initialise le device D3D11 si necessaire.
    //
    // Retour :
    // - true si le renderer est disponible.
    // - false si D3D11 n'a pas pu etre initialise.
    // ------------------------------------------------------------------------
    bool Initialize();

    // ------------------------------------------------------------------------
    // Libere toutes les ressources D3D11 possedees.
    // ------------------------------------------------------------------------
    void Shutdown();

    // ------------------------------------------------------------------------
    // Indique si le renderer D3D11 est pret.
    //
    // Retour :
    // - true si Initialize a reussi.
    // ------------------------------------------------------------------------
    bool IsInitialized() const;

    // ------------------------------------------------------------------------
    // Retourne le device D3D11 initialise.
    //
    // Retour :
    // - pointeur brut non possede, ou nullptr si le renderer est inactif.
    // ------------------------------------------------------------------------
    ID3D11Device* Device() const;

    // ------------------------------------------------------------------------
    // Retourne le contexte immediat D3D11 initialise.
    //
    // Retour :
    // - pointeur brut non possede, ou nullptr si le renderer est inactif.
    // ------------------------------------------------------------------------
    ID3D11DeviceContext* Context() const;

private:
    struct Impl;

    // Implementation privee gardant D3D11 hors du header public.
    std::unique_ptr<Impl> impl_;
};

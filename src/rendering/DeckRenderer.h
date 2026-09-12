// ============================================================================
// Codex Deck - Rendu Direct2D minimal
// ----------------------------------------------------------------------------
// Ce module possede les ressources Direct2D/DirectWrite du bootstrap. Il dessine
// uniquement la coquille visuelle et ne connait ni Codex, ni le stockage.
// ============================================================================

#pragma once

#include <windows.h>

#include <string>

// ----------------------------------------------------------------------------
// Decrit le contenu textuel minimal rendu par la coquille.
// ----------------------------------------------------------------------------
struct DeckVisualState {
    // Titre principal de la fenetre.
    std::wstring title = L"Codex Deck";

    // Sous-titre de bootstrap affiche sous le titre.
    std::wstring subtitle = L"Native Codex workbench";
};

// ----------------------------------------------------------------------------
// Gere les ressources graphiques et le dessin de la coquille native.
// ----------------------------------------------------------------------------
class DeckRenderer {
public:
    // ------------------------------------------------------------------------
    // Initialise les factories et les ressources liees a la fenetre.
    //
    // Parametres :
    // - hwnd : fenetre cible du rendu.
    //
    // Retour :
    // - true si le rendu peut commencer.
    // ------------------------------------------------------------------------
    bool Initialize(HWND hwnd);

    // ------------------------------------------------------------------------
    // Redimensionne la cible Direct2D pour suivre le client Win32.
    //
    // Parametres :
    // - hwnd : fenetre dont la taille cliente sert de reference.
    // ------------------------------------------------------------------------
    void Resize(HWND hwnd);

    // ------------------------------------------------------------------------
    // Dessine l'etat visuel courant.
    //
    // Parametres :
    // - hwnd : fenetre cible, utilisee pour recreer les ressources au besoin.
    // - state : textes a afficher.
    // ------------------------------------------------------------------------
    void Render(HWND hwnd, const DeckVisualState& state);

    // ------------------------------------------------------------------------
    // Libere les ressources graphiques dependantes du device.
    // ------------------------------------------------------------------------
    void DiscardDeviceResources();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

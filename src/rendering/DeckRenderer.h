// ============================================================================
// Codex Deck - Rendu Direct2D minimal
// ----------------------------------------------------------------------------
// Ce module possede les ressources Direct2D/DirectWrite du bootstrap. Il dessine
// uniquement la coquille visuelle et ne connait ni Codex, ni le stockage.
// ============================================================================

#pragma once

#include "../graphics/CompositionHost.h"
#include "../theme/ThemePalette.h"

#include <windows.h>

#include <memory>
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
    // Cree un renderer sans allouer encore de ressources natives.
    // ------------------------------------------------------------------------
    DeckRenderer();

    // ------------------------------------------------------------------------
    // Libere les ressources de rendu opaques.
    // ------------------------------------------------------------------------
    ~DeckRenderer();

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
    // - palette : couleurs resolues a utiliser.
    // ------------------------------------------------------------------------
    void Render(HWND hwnd, const DeckVisualState& state, const ThemePalette& palette);

    // ------------------------------------------------------------------------
    // Traite une molette verticale pour la zone Tree.
    //
    // Parametres :
    // - delta : delta Win32 de molette.
    // ------------------------------------------------------------------------
    void OnMouseWheel(int delta);

    // ------------------------------------------------------------------------
    // Traite un clic pointeur pour la zone Tree.
    //
    // Parametres :
    // - x : position horizontale en pixels client.
    // - y : position verticale en pixels client.
    // ------------------------------------------------------------------------
    void OnPointerDown(float x, float y);

    // ------------------------------------------------------------------------
    // Traite une touche clavier de navigation Tree.
    //
    // Parametres :
    // - virtual_key : code touche Win32.
    //
    // Retour :
    // - true si la touche a ete consommee.
    // ------------------------------------------------------------------------
    bool OnKeyDown(WPARAM virtual_key);

    // ------------------------------------------------------------------------
    // Traite un caractere texte pour la Command Palette.
    //
    // Parametres :
    // - character : caractere UTF-16 issu de WM_CHAR.
    //
    // Retour :
    // - true si le caractere a ete consomme.
    // ------------------------------------------------------------------------
    bool OnChar(wchar_t character);

    // ------------------------------------------------------------------------
    // Libere les ressources graphiques dependantes du device.
    // ------------------------------------------------------------------------
    void DiscardDeviceResources();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Codex Deck - Rendu Direct2D minimal
// ----------------------------------------------------------------------------
// Ce module possede les ressources Direct2D/DirectWrite du bootstrap. Il dessine
// uniquement la coquille visuelle et ne connait ni Codex, ni le stockage.
// ============================================================================

#pragma once

#include "../graphics/CompositionHost.h"
#include "../app/DeckCommand.h"
#include "../model/SessionCatalog.h"
#include "../theme/ThemePalette.h"

#include <windows.h>

#include <memory>
#include <functional>
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

// Handler des commandes de cycle de vie deleguees a DeckApp.
using DeckCommandHandler = std::function<void(DeckCommand)>;

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

    // Installe le handler des commandes qui sortent du renderer.
    void SetCommandHandler(DeckCommandHandler handler);

    // Met a jour le catalogue de modeles propose dans l'overlay de creation.
    void SetAvailableModels(std::vector<CodexModelInfo> models);

    // Selectionne un thread cree ou restaure depuis le thread UI.
    void SelectThread(const CodexThreadId& thread_id);

    // Affiche une erreur de creation dans l'overlay encore ouvert.
    void SetSessionCreationError(std::wstring message);

    // Remplace le workspace du formulaire apres le picker natif.
    void SetNewSessionWorkspacePath(std::filesystem::path workspace);

    // Ouvre l'editeur d'un nouveau projet apres choix de sa racine.
    void OpenNewProjectEditor(std::filesystem::path root);

    // Selectionne et deploie un projet cree depuis le worker stockage.
    void SelectProject(ProjectId project_id);

    // Affiche une erreur de gestion de projet dans l'overlay courant.
    void SetProjectEditorError(std::wstring message);

    // Ferme l'editeur apres une mutation terminee sans selection cible.
    void CloseProjectEditor();

    // Publie les threads archives charges a la demande.
    void SetArchiveThreads(std::vector<CodexThreadSummary> threads);

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
    // - catalog : snapshot courant publie par le worker de sessions.
    // ------------------------------------------------------------------------
    void Render(
        HWND hwnd,
        const DeckVisualState& state,
        const ThemePalette& palette,
        std::shared_ptr<const SessionCatalogSnapshot> catalog
    );

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
    // Traite un mouvement pointeur pour le drag interne Tree.
    //
    // Parametres :
    // - x : position horizontale en pixels client.
    // - y : position verticale en pixels client.
    // ------------------------------------------------------------------------
    void OnPointerMove(float x, float y);

    // ------------------------------------------------------------------------
    // Signale que le pointeur a quitte la fenetre.
    // ------------------------------------------------------------------------
    void OnPointerLeave();

    // ------------------------------------------------------------------------
    // Termine un clic ou drag pointeur pour le Tree.
    //
    // Parametres :
    // - x : position horizontale en pixels client.
    // - y : position verticale en pixels client.
    // ------------------------------------------------------------------------
    void OnPointerUp(float x, float y);

    // ------------------------------------------------------------------------
    // Indique si un point client touche le separateur redimensionnable.
    //
    // Parametres :
    // - x : position horizontale en pixels client.
    // - y : position verticale en pixels client.
    //
    // Retour :
    // - true si le pointeur est sur le separateur.
    // ------------------------------------------------------------------------
    bool IsPointOnSplitter(float x, float y) const;

    // ------------------------------------------------------------------------
    // Indique si un point touche le champ de saisie de la palette ouverte.
    // ------------------------------------------------------------------------
    bool IsPointOnTextInput(float x, float y) const;

    // ------------------------------------------------------------------------
    // Avance les animations legeres du renderer.
    //
    // Retour :
    // - true si un repaint reste utile.
    // ------------------------------------------------------------------------
    bool AdvanceAnimations();

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

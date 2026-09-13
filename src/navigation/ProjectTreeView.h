// ============================================================================
// Codex Deck - Vue Direct2D du Tree projets/sessions
// ----------------------------------------------------------------------------
// Ce module dessine les lignes visibles d'un modele aplati. Il traduit les
// primitives graphiques mais ne possede ni catalogue, ni stockage.
// ============================================================================

#pragma once

#include "ProjectTreeModel.h"

#include "../theme/ThemePalette.h"
#include "../ui/ScrollState.h"
#include "../ui/TreeVisualAnimation.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <functional>
#include <memory>
#include <map>
#include <optional>
#include <span>

// Callback de selection de session.
using SelectThreadHandler = std::move_only_function<void(const CodexThreadId&)>;

// Callback de bascule de projet.
using ToggleProjectHandler = std::move_only_function<void(ProjectId)>;

// Callback d'ouverture d'une ligne More.
using OpenMoreHandler = std::move_only_function<void(ProjectId)>;

// ----------------------------------------------------------------------------
// Dessine et pilote la surface Tree.
// ----------------------------------------------------------------------------
class ProjectTreeView {
public:
    // ------------------------------------------------------------------------
    // Cree une vue sans ressources.
    // ------------------------------------------------------------------------
    ProjectTreeView();

    // ------------------------------------------------------------------------
    // Libere les ressources opaques.
    // ------------------------------------------------------------------------
    ~ProjectTreeView();

    // ------------------------------------------------------------------------
    // Installe les callbacks d'interaction.
    //
    // Parametres :
    // - select_thread : callback de selection.
    // - toggle_project : callback de bascule projet.
    // - open_more : callback de ligne More.
    // ------------------------------------------------------------------------
    void SetHandlers(
        SelectThreadHandler select_thread,
        ToggleProjectHandler toggle_project,
        OpenMoreHandler open_more
    );

    // ------------------------------------------------------------------------
    // Dessine les lignes visibles.
    //
    // Parametres :
    // - dc : contexte Direct2D cible.
    // - bounds : rectangle de rendu.
    // - rows : lignes aplaties.
    // - scroll : defilement courant.
    // - selected_thread : session selectionnee.
    // - selected_row : ligne selectionnee au clavier.
    // - hovered_row : ligne actuellement survolee par le pointeur.
    // - scrollbar_opacity : opacite courante de scrollbar.
    // - scrollbar_width : largeur visuelle courante de scrollbar.
    // - title_marquee_animations : progressions animees par session.
    // - palette : palette resolue.
    // ------------------------------------------------------------------------
    void Render(
        ID2D1DeviceContext* dc,
        const D2D1_RECT_F& bounds,
        std::span<const TreeRow> rows,
        const ScrollState& scroll,
        const std::optional<CodexThreadId>& selected_thread,
        std::optional<std::size_t> selected_row,
        std::optional<std::size_t> hovered_row,
        float scrollbar_opacity,
        float scrollbar_width,
        std::map<CodexThreadId, MarqueeAnimationState>& title_marquee_animations,
        const ThemePalette& palette
    );

    // ------------------------------------------------------------------------
    // Active une ligne et appelle le callback correspondant.
    //
    // Parametres :
    // - row : ligne activee.
    // ------------------------------------------------------------------------
    void ActivateRow(const TreeRow& row);

    // ------------------------------------------------------------------------
    // Retourne la hauteur fixe de ligne.
    //
    // Retour :
    // - hauteur en DIPs.
    // ------------------------------------------------------------------------
    float RowHeight() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

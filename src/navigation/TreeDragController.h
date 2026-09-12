// ============================================================================
// Codex Deck - Controleur de drag interne du Tree
// ----------------------------------------------------------------------------
// Ce module detecte un drag session vers projet et produit une intention
// d'affectation. Il ne modifie jamais SQLite et n'appelle aucun service Codex.
// ============================================================================

#pragma once

#include "ProjectTreeModel.h"

#include <optional>

// ----------------------------------------------------------------------------
// Point de pointeur exprime en DIPs.
// ----------------------------------------------------------------------------
struct TreeDragPoint {
    // Position horizontale.
    float x = 0.0F;

    // Position verticale.
    float y = 0.0F;
};

// ----------------------------------------------------------------------------
// Etat courant du drag interne.
// ----------------------------------------------------------------------------
enum class TreeDragState {
    // Aucun drag en cours.
    Idle,

    // Une session est armee mais le seuil de mouvement n'est pas atteint.
    Primed,

    // Le seuil est atteint et une cible peut etre survolee.
    Dragging,
};

// ----------------------------------------------------------------------------
// Intention d'affectation produite au drop.
// ----------------------------------------------------------------------------
struct TreeDragAssignment {
    // Thread de session deplace.
    CodexThreadId thread_id;

    // Projet cible, ou rien pour Unassigned.
    std::optional<ProjectId> project_id;
};

// ----------------------------------------------------------------------------
// Detecte et materialise un drag interne session vers projet.
// ----------------------------------------------------------------------------
class TreeDragController {
public:
    // ------------------------------------------------------------------------
    // Arme le drag a partir d'une ligne potentiellement source.
    //
    // Parametres :
    // - source : ligne sous le pointeur.
    // - point : position initiale.
    // ------------------------------------------------------------------------
    void Begin(const TreeRow& source, TreeDragPoint point);

    // ------------------------------------------------------------------------
    // Met a jour le drag et la cible survolee.
    //
    // Parametres :
    // - point : position courante.
    // - target : ligne survolee, ou rien hors Tree.
    //
    // Retour :
    // - etat courant apres mise a jour.
    // ------------------------------------------------------------------------
    TreeDragState Update(TreeDragPoint point, const TreeRow* target);

    // ------------------------------------------------------------------------
    // Termine le drag et retourne l'affectation demandee.
    //
    // Retour :
    // - affectation si une cible valide etait survolee.
    // ------------------------------------------------------------------------
    std::optional<TreeDragAssignment> Drop();

    // ------------------------------------------------------------------------
    // Annule le drag courant.
    // ------------------------------------------------------------------------
    void Cancel();

    // ------------------------------------------------------------------------
    // Retourne l'etat courant.
    //
    // Retour :
    // - etat du drag.
    // ------------------------------------------------------------------------
    TreeDragState State() const;

private:
    // Etat courant.
    TreeDragState state_ = TreeDragState::Idle;

    // Position initiale du drag arme.
    TreeDragPoint origin_{};

    // Thread source arme.
    std::optional<CodexThreadId> thread_id_;

    // Projet cible survole, ou Unassigned.
    std::optional<std::optional<ProjectId>> hovered_project_;
};

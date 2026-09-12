// ============================================================================
// Codex Deck - Implementation du controleur de drag interne
// ----------------------------------------------------------------------------
// Ce fichier garde le drag sous forme d'intention pure afin que l'affectation
// soit appliquee plus haut par ProjectAssignmentService.
// ============================================================================

#include "TreeDragController.h"

#include <cmath>

namespace {

// Distance minimale avant de transformer un clic arme en drag.
constexpr float kDragThresholdDips = 6.0F;

// ----------------------------------------------------------------------------
// Calcule une distance au carre.
//
// Parametres :
// - a : premier point.
// - b : second point.
//
// Retour :
// - distance euclidienne au carre.
// ----------------------------------------------------------------------------
float DistanceSquared(TreeDragPoint a, TreeDragPoint b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// ----------------------------------------------------------------------------
// Extrait une cible d'affectation depuis une ligne.
//
// Parametres :
// - target : ligne survolee.
//
// Retour :
// - projet cible, ou rien pour Unassigned, ou nullopt si invalide.
// ----------------------------------------------------------------------------
std::optional<std::optional<ProjectId>> AssignmentTarget(const TreeRow& target) {
    if (target.kind == TreeRowKind::Project && target.project_id) {
        return target.project_id;
    }
    if (target.kind == TreeRowKind::UnassignedHeader) {
        return std::optional<ProjectId>{};
    }
    return std::nullopt;
}

}  // namespace

// ----------------------------------------------------------------------------
// Arme le drag a partir d'une ligne potentiellement source.
// ----------------------------------------------------------------------------
void TreeDragController::Begin(const TreeRow& source, TreeDragPoint point) {
    Cancel();
    if (source.kind != TreeRowKind::Session || !source.thread_id) {
        return;
    }
    state_ = TreeDragState::Primed;
    origin_ = point;
    thread_id_ = source.thread_id;
}

// ----------------------------------------------------------------------------
// Met a jour le drag et la cible survolee.
// ----------------------------------------------------------------------------
TreeDragState TreeDragController::Update(TreeDragPoint point, const TreeRow* target) {
    if (state_ == TreeDragState::Idle || !thread_id_) {
        return TreeDragState::Idle;
    }
    constexpr float threshold_squared = kDragThresholdDips * kDragThresholdDips;
    if (state_ == TreeDragState::Primed && DistanceSquared(origin_, point) <= threshold_squared) {
        return state_;
    }
    state_ = TreeDragState::Dragging;
    hovered_project_.reset();
    if (target != nullptr) {
        hovered_project_ = AssignmentTarget(*target);
    }
    return state_;
}

// ----------------------------------------------------------------------------
// Termine le drag et retourne l'affectation demandee.
// ----------------------------------------------------------------------------
std::optional<TreeDragAssignment> TreeDragController::Drop() {
    if (state_ != TreeDragState::Dragging || !thread_id_ || !hovered_project_) {
        Cancel();
        return std::nullopt;
    }
    TreeDragAssignment assignment{*thread_id_, *hovered_project_};
    Cancel();
    return assignment;
}

// ----------------------------------------------------------------------------
// Annule le drag courant.
// ----------------------------------------------------------------------------
void TreeDragController::Cancel() {
    state_ = TreeDragState::Idle;
    thread_id_.reset();
    hovered_project_.reset();
}

// ----------------------------------------------------------------------------
// Retourne l'etat courant.
// ----------------------------------------------------------------------------
TreeDragState TreeDragController::State() const {
    return state_;
}

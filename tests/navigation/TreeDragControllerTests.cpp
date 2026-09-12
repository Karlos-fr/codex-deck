// ============================================================================
// Codex Deck - Tests du controleur de drag Tree
// ----------------------------------------------------------------------------
// Ce fichier valide le drag interne session vers projet avec donnees
// synthetiques, sans OLE et sans ecriture SQLite directe.
// ============================================================================

#include "navigation/TreeDragController.h"

#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Cree une ligne session minimale.
//
// Retour :
// - ligne de session de test.
// ----------------------------------------------------------------------------
TreeRow SessionRow() {
    TreeRow row{};
    row.kind = TreeRowKind::Session;
    row.thread_id = CodexThreadId{"thr_drag"};
    return row;
}

// ----------------------------------------------------------------------------
// Cree une ligne projet minimale.
//
// Retour :
// - ligne de projet de test.
// ----------------------------------------------------------------------------
TreeRow ProjectRow() {
    TreeRow row{};
    row.kind = TreeRowKind::Project;
    row.project_id = ProjectId{42};
    return row;
}

}  // namespace

// ----------------------------------------------------------------------------
// Lance les validations de drag interne.
//
// Retour :
// - zero si le controleur ne produit que des affectations explicites.
// ----------------------------------------------------------------------------
int main() {
    TreeDragController controller;
    const TreeDragPoint origin{10.0F, 10.0F};
    controller.Begin(SessionRow(), origin);
    if (controller.Update(TreeDragPoint{12.0F, 12.0F}, nullptr) != TreeDragState::Primed) {
        return 1;
    }
    const TreeRow project_target = ProjectRow();
    if (controller.Update(TreeDragPoint{20.0F, 10.0F}, &project_target) != TreeDragState::Dragging) {
        return 2;
    }
    const auto assignment = controller.Drop();
    if (!assignment || assignment->thread_id != "thr_drag" || assignment->project_id != ProjectId{42}) {
        return 3;
    }

    controller.Begin(ProjectRow(), origin);
    if (controller.Update(TreeDragPoint{30.0F, 10.0F}, &project_target) != TreeDragState::Idle) {
        return 4;
    }
    if (controller.Drop().has_value()) {
        return 5;
    }

    controller.Begin(SessionRow(), origin);
    const TreeRow unassigned{TreeRowKind::UnassignedHeader};
    controller.Update(TreeDragPoint{30.0F, 10.0F}, &unassigned);
    const auto unassigned_drop = controller.Drop();
    return unassigned_drop && !unassigned_drop->project_id ? 0 : 6;
}

// ============================================================================
// Codex Deck - Record de session fusionne
// ----------------------------------------------------------------------------
// Ce module definit la vue modele combinant thread Codex et metadonnees locales
// legeres, sans historique de conversation.
// ============================================================================

#pragma once

#include "../codex/CodexTypes.h"
#include "../projects/ProjectTypes.h"

#include <optional>

// ----------------------------------------------------------------------------
// Session fusionnee entre Codex et l'organisation locale.
// ----------------------------------------------------------------------------
struct SessionRecord {
    // Resume Codex ou cache equivalent.
    CodexThreadSummary codex;

    // Projet associe, absent pour Unassigned.
    std::optional<ProjectId> project_id;

    // Source de l'association.
    AssignmentSource assignment_source = AssignmentSource::Automatic;

    // Favori local.
    bool favorite = false;

    // Statut runtime/cache courant.
    SessionStatus status = SessionStatus::Idle;

    // Indique si le thread est present dans la derniere liste Codex.
    bool present_in_codex = true;
};

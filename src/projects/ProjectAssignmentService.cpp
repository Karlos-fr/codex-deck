// ============================================================================
// Codex Deck - Implementation du service d'assignation projet
// ----------------------------------------------------------------------------
// Ce fichier publie d'abord les changements locaux visibles, puis revient au
// snapshot precedent si la persistance SQLite echoue.
// ============================================================================

#include "ProjectAssignmentService.h"

namespace {

// ----------------------------------------------------------------------------
// Applique une assignation dans une copie de snapshot.
//
// Parametres :
// - snapshot : snapshot a modifier.
// - thread_id : session cible.
// - project_id : projet cible ou Unassigned.
// - source : source a appliquer.
//
// Retour :
// - true si une session a ete modifiee.
// ----------------------------------------------------------------------------
bool ApplyAssignment(
    SessionCatalogSnapshot& snapshot,
    const CodexThreadId& thread_id,
    std::optional<ProjectId> project_id,
    AssignmentSource source
) {
    for (SessionRecord& session : snapshot.sessions) {
        if (session.codex.id == thread_id) {
            session.project_id = project_id;
            session.assignment_source = source;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Construit une erreur pour une session absente du snapshot.
//
// Parametres :
// - thread_id : session introuvable.
//
// Retour :
// - erreur de contrainte locale.
// ----------------------------------------------------------------------------
StorageError MissingSessionError(const CodexThreadId& thread_id) {
    return StorageError{StorageErrorCode::ConstraintFailed, 0, "Session not found in catalog: " + thread_id};
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree un service d'assignation.
// ----------------------------------------------------------------------------
ProjectAssignmentService::ProjectAssignmentService(
    SessionCatalog& catalog,
    SessionMetadataRepository& metadata_repository,
    AssignmentErrorHandler error_handler
)
    : catalog_(catalog),
      metadata_repository_(metadata_repository),
      error_handler_(std::move(error_handler)) {
}

// ----------------------------------------------------------------------------
// Assigne manuellement une session a un projet ou a Unassigned.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> ProjectAssignmentService::AssignManual(
    const CodexThreadId& thread_id,
    std::optional<ProjectId> project_id
) {
    auto previous = catalog_.Current();
    if (!previous) {
        return std::unexpected(MissingSessionError(thread_id));
    }

    SessionCatalogSnapshot optimistic = *previous;
    if (!ApplyAssignment(optimistic, thread_id, project_id, AssignmentSource::Manual)) {
        return std::unexpected(MissingSessionError(thread_id));
    }
    catalog_.Publish(std::move(optimistic));

    auto persisted = metadata_repository_.SetAssignment(thread_id, project_id, AssignmentSource::Manual);
    if (!persisted) {
        catalog_.Publish(*previous);
        if (error_handler_) {
            error_handler_(persisted.error());
        }
        return std::unexpected(persisted.error());
    }
    return {};
}

// ----------------------------------------------------------------------------
// Repasse une session en association automatique.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> ProjectAssignmentService::RevertToAutomatic(const CodexThreadId& thread_id) {
    auto previous = catalog_.Current();
    if (!previous) {
        return std::unexpected(MissingSessionError(thread_id));
    }

    SessionCatalogSnapshot optimistic = *previous;
    std::optional<ProjectId> current_project;
    for (const SessionRecord& session : previous->sessions) {
        if (session.codex.id == thread_id) {
            current_project = session.project_id;
            break;
        }
    }
    if (!ApplyAssignment(optimistic, thread_id, current_project, AssignmentSource::Automatic)) {
        return std::unexpected(MissingSessionError(thread_id));
    }
    catalog_.Publish(std::move(optimistic));

    auto persisted = metadata_repository_.SetAssignment(thread_id, current_project, AssignmentSource::Automatic);
    if (!persisted) {
        catalog_.Publish(*previous);
        if (error_handler_) {
            error_handler_(persisted.error());
        }
        return std::unexpected(persisted.error());
    }
    return {};
}

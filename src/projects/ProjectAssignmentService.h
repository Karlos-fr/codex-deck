// ============================================================================
// Codex Deck - Service d'assignation projet
// ----------------------------------------------------------------------------
// Ce module applique des mutations locales optimistes sur le catalogue puis les
// persiste dans SQLite, sans exposer SQLite a l'UI.
// ============================================================================

#pragma once

#include "ProjectTypes.h"

#include "../model/SessionCatalog.h"
#include "../storage/SessionMetadataRepository.h"

#include <expected>
#include <functional>

// Callback appele lorsqu'une persistance d'assignation echoue.
using AssignmentErrorHandler = std::move_only_function<void(const StorageError&)>;

// ----------------------------------------------------------------------------
// Applique les assignations manuelles et automatiques locales.
// ----------------------------------------------------------------------------
class ProjectAssignmentService {
public:
    // ------------------------------------------------------------------------
    // Cree un service d'assignation.
    //
    // Parametres :
    // - catalog : catalogue publie non possede.
    // - metadata_repository : repository SQLite non possede.
    // - error_handler : callback optionnel d'erreur UI.
    // ------------------------------------------------------------------------
    ProjectAssignmentService(
        SessionCatalog& catalog,
        SessionMetadataRepository& metadata_repository,
        AssignmentErrorHandler error_handler = {}
    );

    // ------------------------------------------------------------------------
    // Assigne manuellement une session a un projet ou a Unassigned.
    //
    // Parametres :
    // - thread_id : session cible.
    // - project_id : projet choisi, ou absence explicite pour Unassigned.
    //
    // Retour :
    // - succes vide ou erreur de persistance apres rollback.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> AssignManual(const CodexThreadId& thread_id, std::optional<ProjectId> project_id);

    // ------------------------------------------------------------------------
    // Repasse une session en association automatique.
    //
    // Parametres :
    // - thread_id : session cible.
    //
    // Retour :
    // - succes vide ou erreur de persistance apres rollback.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> RevertToAutomatic(const CodexThreadId& thread_id);

private:
    // Catalogue publie non possede.
    SessionCatalog& catalog_;

    // Repository SQLite non possede.
    SessionMetadataRepository& metadata_repository_;

    // Callback d'erreur optionnel.
    AssignmentErrorHandler error_handler_;
};

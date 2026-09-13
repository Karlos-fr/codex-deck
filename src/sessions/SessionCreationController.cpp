// ============================================================================
// Codex Deck - Implementation du controller de creation de session
// ----------------------------------------------------------------------------
// Ce fichier garantit qu'un echec du prompt initial ne supprime jamais le
// thread deja cree et publie.
// ============================================================================

#include "SessionCreationController.h"

// Cree un controller sur des services non possedes.
SessionCreationController::SessionCreationController(
    CodexClient& client,
    SessionCatalog& catalog,
    ProjectAssignmentService& assignment_service
)
    : client_(client), catalog_(catalog), assignment_service_(assignment_service) {
}

// Cree, publie et associe une session puis lance son prompt optionnel.
void SessionCreationController::CreateSession(CreateSessionRequest request, SessionCreationCompletion completion) {
    StartThreadOptions options{request.cwd, request.model};
    client_.StartThread(std::move(options), [this, request = std::move(request), completion = std::move(completion)](
        std::expected<CodexThreadSummary, CodexError> started
    ) mutable {
        if (!started) {
            completion(std::unexpected(SessionCreationError{started.error(), std::nullopt}));
            return;
        }

        SessionCatalogSnapshot snapshot = catalog_.Current() ? *catalog_.Current() : SessionCatalogSnapshot{};
        SessionRecord record{};
        record.codex = *started;
        record.project_id = request.project_id;
        record.assignment_source = request.project_id ? AssignmentSource::Manual : AssignmentSource::Automatic;
        snapshot.sessions.push_back(std::move(record));
        catalog_.Publish(std::move(snapshot));

        if (request.project_id) {
            if (auto assigned = assignment_service_.AssignManual(started->id, request.project_id); !assigned) {
                completion(std::unexpected(SessionCreationError{std::nullopt, assigned.error()}));
                return;
            }
        }

        CreatedSession created{*started, request.project_id, std::nullopt};
        if (!request.initial_prompt || request.initial_prompt->empty()) {
            completion(std::move(created));
            return;
        }
        client_.StartTurn(
            StartTurnOptions{started->id, *request.initial_prompt},
            [created = std::move(created), completion = std::move(completion)](
                std::expected<CodexTurnId, CodexError> turn
            ) mutable {
                if (!turn) {
                    created.initial_prompt_error = turn.error();
                }
                completion(std::move(created));
            }
        );
    });
}

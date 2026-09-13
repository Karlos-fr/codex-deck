// ============================================================================
// Codex Deck - Controller de creation de session
// ----------------------------------------------------------------------------
// Ce module compose thread/start, association locale et prompt initial. Il ne
// choisit ni projet ni dossier et ne manipule aucune vue.
// ============================================================================

#pragma once

#include "SessionCreationTypes.h"

#include "../codex/CodexClient.h"
#include "../model/SessionCatalog.h"
#include "../projects/ProjectAssignmentService.h"

// Controller asynchrone du cycle de creation d'une session.
class SessionCreationController {
public:
    // Cree un controller sur des services non possedes.
    SessionCreationController(CodexClient& client, SessionCatalog& catalog, ProjectAssignmentService& assignment_service);

    // Cree, publie et associe une session puis lance son prompt optionnel.
    void CreateSession(CreateSessionRequest request, SessionCreationCompletion completion);

private:
    // Client app-server non possede.
    CodexClient& client_;
    // Catalogue publie non possede.
    SessionCatalog& catalog_;
    // Service d'association non possede.
    ProjectAssignmentService& assignment_service_;
};

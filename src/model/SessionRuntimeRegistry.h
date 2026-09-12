// ============================================================================
// Codex Deck - Registre runtime de sessions
// ----------------------------------------------------------------------------
// Ce module est la source runtime unique des etats Idle, Working,
// NeedsAttention, Completed et Error pour les sessions Codex.
// ============================================================================

#pragma once

#include "SessionRuntime.h"

#include "../codex/CodexError.h"
#include "../codex/CodexTypes.h"

#include <expected>
#include <map>

// ----------------------------------------------------------------------------
// Registre mutable des sessions runtime.
// ----------------------------------------------------------------------------
class SessionRuntimeRegistry {
public:
    // ------------------------------------------------------------------------
    // Applique une notification Codex au runtime concerne.
    //
    // Parametres :
    // - notification : notification brute recue du transport.
    // ------------------------------------------------------------------------
    void ApplyNotification(const CodexNotification& notification);

    // ------------------------------------------------------------------------
    // Applique une requete serveur d'approbation.
    //
    // Parametres :
    // - request : requete serveur a convertir en PendingApproval.
    //
    // Retour :
    // - approval stockee ou erreur si la requete n'est pas supportee.
    // ------------------------------------------------------------------------
    std::expected<PendingApproval, CodexError> ApplyApprovalRequest(const CodexServerRequest& request);

    // ------------------------------------------------------------------------
    // Resout une approbation par son identifiant JSON-RPC.
    //
    // Parametres :
    // - request_id : identifiant JSON-RPC.
    // - wire_decision : decision envoyee au serveur.
    //
    // Retour :
    // - true si une approval correspondante a ete retiree.
    // ------------------------------------------------------------------------
    bool ResolveApproval(const nlohmann::json& request_id, std::string_view wire_decision);

    // ------------------------------------------------------------------------
    // Retourne le runtime d'un thread s'il existe.
    //
    // Parametres :
    // - thread_id : identifiant du thread.
    //
    // Retour :
    // - pointeur non possede ou nullptr.
    // ------------------------------------------------------------------------
    const SessionRuntime* Find(const CodexThreadId& thread_id) const;

private:
    // ------------------------------------------------------------------------
    // Retourne ou cree le runtime d'un thread.
    //
    // Parametres :
    // - thread_id : identifiant du thread.
    //
    // Retour :
    // - reference mutable.
    // ------------------------------------------------------------------------
    SessionRuntime& Ensure(const CodexThreadId& thread_id);

    // Sessions indexees par threadId.
    std::map<CodexThreadId, SessionRuntime> sessions_;
};

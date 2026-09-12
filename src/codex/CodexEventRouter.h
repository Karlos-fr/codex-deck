// ============================================================================
// Codex Deck - Routeur d'evenements Codex
// ----------------------------------------------------------------------------
// Ce module relie les notifications et requetes serveur du transport au registre
// runtime, tout en publiant des evenements bruts pour le futur Workbench.
// ============================================================================

#pragma once

#include "JsonRpcTransport.h"

#include "../model/SessionRuntimeRegistry.h"

#include <functional>

// ----------------------------------------------------------------------------
// Evenement brut destine a la timeline future.
// ----------------------------------------------------------------------------
struct CodexTimelineEvent {
    // Thread concerne par l'evenement.
    CodexThreadId thread_id;

    // Methode app-server recue.
    std::string method;

    // Parametres bruts conserves en memoire.
    nlohmann::json params;

    // Timestamp local de reception.
    std::int64_t received_at = 0;
};

// ----------------------------------------------------------------------------
// Route notifications, approvals et evenements timeline.
// ----------------------------------------------------------------------------
class CodexEventRouter {
public:
    // Handler appele pour chaque evenement timeline brut.
    using TimelineEventHandler = std::move_only_function<void(CodexTimelineEvent)>;

    // ------------------------------------------------------------------------
    // Cree un routeur lie au transport et au registre.
    //
    // Parametres :
    // - transport : transport JSON-RPC non possede.
    // - registry : registre runtime a mettre a jour.
    // ------------------------------------------------------------------------
    CodexEventRouter(JsonRpcTransport& transport, SessionRuntimeRegistry& registry);

    // ------------------------------------------------------------------------
    // Branche les callbacks du transport vers ce routeur.
    // ------------------------------------------------------------------------
    void AttachTransportHandlers();

    // ------------------------------------------------------------------------
    // Installe le handler timeline.
    //
    // Parametres :
    // - handler : callback a appeler pour les notifications utiles.
    // ------------------------------------------------------------------------
    void SetTimelineEventHandler(TimelineEventHandler handler);

    // ------------------------------------------------------------------------
    // Resout une approval et repond au serveur.
    //
    // Parametres :
    // - request_id : identifiant JSON-RPC de l'approval.
    // - wire_decision : decision a envoyer telle quelle.
    //
    // Retour :
    // - succes vide ou erreur d'ecriture/correlation.
    // ------------------------------------------------------------------------
    std::expected<void, CodexError> ResolveApproval(const nlohmann::json& request_id, std::string wire_decision);

    // ------------------------------------------------------------------------
    // Route une notification recue.
    //
    // Parametres :
    // - notification : notification transport.
    // ------------------------------------------------------------------------
    void HandleNotification(CodexNotification notification);

    // ------------------------------------------------------------------------
    // Route une requete serveur recue.
    //
    // Parametres :
    // - request : requete transport.
    // ------------------------------------------------------------------------
    void HandleServerRequest(CodexServerRequest request);

private:
    // Transport JSON-RPC non possede.
    JsonRpcTransport& transport_;

    // Registre runtime non possede.
    SessionRuntimeRegistry& registry_;

    // Handler timeline optionnel.
    TimelineEventHandler timeline_handler_;
};

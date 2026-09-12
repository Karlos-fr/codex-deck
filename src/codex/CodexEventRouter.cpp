// ============================================================================
// Codex Deck - Implementation du routeur d'evenements
// ----------------------------------------------------------------------------
// Ce fichier applique les notifications au runtime, convertit les approvals en
// etat NeedsAttention et repond aux approvals par leur id JSON-RPC exact.
// ============================================================================

#include "CodexEventRouter.h"

#include <chrono>
#include <utility>

namespace {

// ----------------------------------------------------------------------------
// Retourne un timestamp local de reception.
//
// Retour :
// - millisecondes depuis epoch systeme.
// ----------------------------------------------------------------------------
std::int64_t NowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

// ----------------------------------------------------------------------------
// Lit le threadId d'un objet params.
//
// Parametres :
// - params : params JSON source.
//
// Retour :
// - threadId ou chaine vide.
// ----------------------------------------------------------------------------
std::string ThreadIdFromParams(const nlohmann::json& params) {
    return params.is_object() && params.contains("threadId") && params.at("threadId").is_string()
        ? params.at("threadId").get<std::string>()
        : std::string{};
}

// ----------------------------------------------------------------------------
// Construit une erreur de correlation d'approval.
//
// Retour :
// - erreur InvalidResponse.
// ----------------------------------------------------------------------------
CodexError ApprovalNotFound() {
    return CodexError{CodexErrorCode::InvalidResponse, L"Approval introuvable pour cet id JSON-RPC"};
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree un routeur lie au transport et au registre.
// ----------------------------------------------------------------------------
CodexEventRouter::CodexEventRouter(JsonRpcTransport& transport, SessionRuntimeRegistry& registry)
    : transport_(transport),
      registry_(registry) {
}

// ----------------------------------------------------------------------------
// Branche les callbacks du transport vers ce routeur.
// ----------------------------------------------------------------------------
void CodexEventRouter::AttachTransportHandlers() {
    transport_.SetNotificationHandler([this](CodexNotification notification) {
        HandleNotification(std::move(notification));
    });
    transport_.SetServerRequestHandler([this](CodexServerRequest request) {
        HandleServerRequest(std::move(request));
    });
}

// ----------------------------------------------------------------------------
// Installe le handler timeline.
// ----------------------------------------------------------------------------
void CodexEventRouter::SetTimelineEventHandler(TimelineEventHandler handler) {
    timeline_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Resout une approval et repond au serveur.
// ----------------------------------------------------------------------------
std::expected<void, CodexError> CodexEventRouter::ResolveApproval(const nlohmann::json& request_id, std::string wire_decision) {
    if (!registry_.ResolveApproval(request_id, wire_decision)) {
        return std::unexpected(ApprovalNotFound());
    }
    return transport_.SendResponse(request_id, {{"decision", std::move(wire_decision)}});
}

// ----------------------------------------------------------------------------
// Route une notification recue.
// ----------------------------------------------------------------------------
void CodexEventRouter::HandleNotification(CodexNotification notification) {
    registry_.ApplyNotification(notification);
    if (timeline_handler_) {
        timeline_handler_(CodexTimelineEvent{
            ThreadIdFromParams(notification.params),
            notification.method,
            notification.params,
            NowMillis(),
        });
    }
}

// ----------------------------------------------------------------------------
// Route une requete serveur recue.
// ----------------------------------------------------------------------------
void CodexEventRouter::HandleServerRequest(CodexServerRequest request) {
    const auto approval = registry_.ApplyApprovalRequest(request);
    if (approval && timeline_handler_) {
        timeline_handler_(CodexTimelineEvent{
            approval->thread_id,
            request.method,
            request.params,
            NowMillis(),
        });
    }
}

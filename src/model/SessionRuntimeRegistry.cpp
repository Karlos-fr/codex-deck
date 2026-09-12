// ============================================================================
// Codex Deck - Implementation du registre runtime
// ----------------------------------------------------------------------------
// Ce fichier applique les transitions issues des notifications et approbations
// Codex sans dupliquer une machine d'etat dans les vues.
// ============================================================================

#include "SessionRuntimeRegistry.h"

#include <chrono>

namespace {

// Methode d'approbation de commande supportee en V1.
constexpr char kCommandApprovalMethod[] = "item/commandExecution/requestApproval";

// Methode d'approbation de changement de fichier supportee en V1.
constexpr char kFileApprovalMethod[] = "item/fileChange/requestApproval";

// ----------------------------------------------------------------------------
// Retourne un timestamp local monotone pour l'activite.
//
// Retour :
// - millisecondes depuis epoch systeme.
// ----------------------------------------------------------------------------
std::int64_t NowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

// ----------------------------------------------------------------------------
// Lit une chaine optionnelle dans des params JSON.
//
// Parametres :
// - params : objet JSON source.
// - key : nom du champ.
//
// Retour :
// - valeur texte ou chaine vide.
// ----------------------------------------------------------------------------
std::string StringValue(const nlohmann::json& params, const char* key) {
    return params.is_object() && params.contains(key) && params.at(key).is_string()
        ? params.at(key).get<std::string>()
        : std::string{};
}

// ----------------------------------------------------------------------------
// Lit une liste de decisions wire.
//
// Parametres :
// - params : objet JSON source.
//
// Retour :
// - decisions annoncees ou accept/decline par defaut.
// ----------------------------------------------------------------------------
std::vector<std::string> Decisions(const nlohmann::json& params) {
    std::vector<std::string> decisions;
    if (params.is_object() && params.contains("availableDecisions") && params.at("availableDecisions").is_array()) {
        for (const nlohmann::json& decision : params.at("availableDecisions")) {
            if (decision.is_string()) {
                decisions.push_back(decision.get<std::string>());
            }
        }
    }
    if (decisions.empty()) {
        decisions.push_back("accept");
        decisions.push_back("decline");
    }
    return decisions;
}

// ----------------------------------------------------------------------------
// Indique si une methode est une approval supportee.
//
// Parametres :
// - method : methode app-server.
//
// Retour :
// - true pour les approvals V1.
// ----------------------------------------------------------------------------
bool IsApprovalMethod(const std::string& method) {
    return method == kCommandApprovalMethod || method == kFileApprovalMethod;
}

// ----------------------------------------------------------------------------
// Construit une erreur de runtime.
//
// Parametres :
// - message : diagnostic court.
//
// Retour :
// - erreur InvalidResponse.
// ----------------------------------------------------------------------------
CodexError RuntimeError(const wchar_t* message) {
    return CodexError{CodexErrorCode::InvalidResponse, message};
}

}  // namespace

// ----------------------------------------------------------------------------
// Applique une notification Codex au runtime concerne.
// ----------------------------------------------------------------------------
void SessionRuntimeRegistry::ApplyNotification(const CodexNotification& notification) {
    const std::string thread_id = StringValue(notification.params, "threadId");
    if (thread_id.empty()) {
        return;
    }

    SessionRuntime& runtime = Ensure(thread_id);
    runtime.latest_activity = NowMillis();
    if (notification.method == "turn/started") {
        runtime.status = SessionStatus::Working;
        runtime.current_turn = StringValue(notification.params, "turnId");
        runtime.last_error.clear();
    } else if (notification.method == "turn/completed") {
        runtime.status = SessionStatus::Completed;
        runtime.current_turn.reset();
        runtime.pending_approval.reset();
    } else if (notification.method == "turn/error" || notification.method == "error") {
        runtime.status = SessionStatus::Error;
        runtime.current_turn.reset();
        runtime.pending_approval.reset();
        runtime.last_error = StringValue(notification.params, "message");
    }
}

// ----------------------------------------------------------------------------
// Applique une requete serveur d'approbation.
// ----------------------------------------------------------------------------
std::expected<PendingApproval, CodexError> SessionRuntimeRegistry::ApplyApprovalRequest(const CodexServerRequest& request) {
    if (!IsApprovalMethod(request.method)) {
        return std::unexpected(RuntimeError(L"Requete serveur non supportee"));
    }

    const std::string thread_id = StringValue(request.params, "threadId");
    if (thread_id.empty()) {
        return std::unexpected(RuntimeError(L"Approval sans threadId"));
    }

    PendingApproval approval{};
    approval.request_id = request.id;
    approval.thread_id = thread_id;
    const std::string turn_id = StringValue(request.params, "turnId");
    if (!turn_id.empty()) {
        approval.turn_id = turn_id;
    }
    approval.method = request.method;
    approval.title = StringValue(request.params, "title");
    approval.detail = StringValue(request.params, "detail");
    approval.available_decisions = Decisions(request.params);

    SessionRuntime& runtime = Ensure(thread_id);
    runtime.status = SessionStatus::NeedsAttention;
    runtime.current_turn = approval.turn_id;
    runtime.pending_approval = approval;
    runtime.latest_activity = NowMillis();
    return approval;
}

// ----------------------------------------------------------------------------
// Resout une approbation par son identifiant JSON-RPC.
// ----------------------------------------------------------------------------
bool SessionRuntimeRegistry::ResolveApproval(const nlohmann::json& request_id, std::string_view wire_decision) {
    (void)wire_decision;
    for (auto& [thread_id, runtime] : sessions_) {
        (void)thread_id;
        if (runtime.pending_approval && runtime.pending_approval->request_id == request_id) {
            runtime.pending_approval.reset();
            runtime.status = SessionStatus::Working;
            runtime.latest_activity = NowMillis();
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Retourne le runtime d'un thread s'il existe.
// ----------------------------------------------------------------------------
const SessionRuntime* SessionRuntimeRegistry::Find(const CodexThreadId& thread_id) const {
    const auto found = sessions_.find(thread_id);
    return found == sessions_.end() ? nullptr : &found->second;
}

// ----------------------------------------------------------------------------
// Retourne ou cree le runtime d'un thread.
// ----------------------------------------------------------------------------
SessionRuntime& SessionRuntimeRegistry::Ensure(const CodexThreadId& thread_id) {
    auto [iterator, inserted] = sessions_.try_emplace(thread_id);
    if (inserted) {
        iterator->second.thread_id = thread_id;
    }
    return iterator->second;
}

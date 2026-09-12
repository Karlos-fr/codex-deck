// ============================================================================
// Codex Deck - Tests du registre runtime de session
// ----------------------------------------------------------------------------
// Ce fichier valide les transitions d'etat partagees par les futures vues Tree,
// ActivityBar et Workbench.
// ============================================================================

#include "model/SessionRuntimeRegistry.h"

// ----------------------------------------------------------------------------
// Verifie les transitions principales d'un thread.
//
// Retour :
// - zero si le registre suit les notifications et approvals.
// ----------------------------------------------------------------------------
int main() {
    SessionRuntimeRegistry registry;

    registry.ApplyNotification(CodexNotification{
        "turn/started",
        {{"threadId", "thr_123"}, {"turnId", "turn_1"}},
    });
    const SessionRuntime* runtime = registry.Find("thr_123");
    if (runtime == nullptr || runtime->status != SessionStatus::Working || runtime->current_turn != "turn_1") {
        return 1;
    }

    const CodexServerRequest request{
        12,
        "item/commandExecution/requestApproval",
        {{"threadId", "thr_123"}, {"turnId", "turn_1"}, {"title", "Run tests"}},
    };
    auto approval = registry.ApplyApprovalRequest(request);
    runtime = registry.Find("thr_123");
    if (!approval || runtime == nullptr || runtime->status != SessionStatus::NeedsAttention || !runtime->pending_approval) {
        return 2;
    }
    if (runtime->pending_approval->available_decisions.size() != 2) {
        return 3;
    }

    if (!registry.ResolveApproval(12, "accept")) {
        return 4;
    }
    runtime = registry.Find("thr_123");
    if (runtime == nullptr || runtime->status != SessionStatus::Working || runtime->pending_approval) {
        return 5;
    }

    registry.ApplyNotification(CodexNotification{
        "turn/completed",
        {{"threadId", "thr_123"}, {"turnId", "turn_1"}},
    });
    runtime = registry.Find("thr_123");
    if (runtime == nullptr || runtime->status != SessionStatus::Completed) {
        return 6;
    }

    registry.ApplyNotification(CodexNotification{
        "turn/error",
        {{"threadId", "thr_123"}, {"message", "failed"}},
    });
    runtime = registry.Find("thr_123");
    if (runtime == nullptr || runtime->status != SessionStatus::Error || runtime->last_error != "failed") {
        return 7;
    }
    return 0;
}

// ============================================================================
// Codex Deck - Implementation du service Archive
// ----------------------------------------------------------------------------
// Ce fichier conserve les archives hors catalogue actif et invalide leur cache
// uniquement apres restauration confirmee par app-server.
// ============================================================================

#include "ArchiveSessionService.h"

// Cree le service sur un client, un modele et un callback non possedes.
ArchiveSessionService::ArchiveSessionService(CodexClient& client, ArchiveViewModel& model, ArchiveRefreshHandler refresh)
    : client_(client), model_(model), refresh_(std::move(refresh)) {
}

// Charge exhaustivement les threads archives si le cache est invalide.
void ArchiveSessionService::LoadArchive(VoidCompletion completion) {
    if (!model_.invalidated) {
        completion({});
        return;
    }
    model_.loading = true;
    ThreadListOptions options{};
    options.archived = true;
    client_.ListThreads(options, [this, completion = std::move(completion)](
        std::expected<std::vector<CodexThreadSummary>, CodexError> result
    ) mutable {
        if (!result) {
            model_.loading = false;
            completion(std::unexpected(result.error()));
            return;
        }
        SetArchiveThreads(model_, std::move(*result));
        completion({});
    });
}

// Restaure un thread puis invalide le cache et demande une sync.
void ArchiveSessionService::RestoreArchivedThread(CodexThreadId thread_id, VoidCompletion completion) {
    client_.UnarchiveThread(std::move(thread_id), [this, completion = std::move(completion)](
        std::expected<void, CodexError> result
    ) mutable {
        if (!result) {
            completion(std::unexpected(result.error()));
            return;
        }
        model_.invalidated = true;
        if (refresh_) {
            refresh_();
        }
        completion({});
    });
}

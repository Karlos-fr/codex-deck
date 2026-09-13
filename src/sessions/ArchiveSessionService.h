// ============================================================================
// Codex Deck - Service des sessions archivees
// ----------------------------------------------------------------------------
// Ce module charge les archives a la demande et restaure un thread avant de
// demander une reconciliation du catalogue actif.
// ============================================================================

#pragma once

#include "../codex/CodexClient.h"
#include "../navigation/ArchiveViewModel.h"

// Callback demandant une sync exhaustive apres restauration.
using ArchiveRefreshHandler = std::move_only_function<void()>;

// Service asynchrone de la vue Archive.
class ArchiveSessionService {
public:
    // Cree le service sur un client, un modele et un callback non possedes.
    ArchiveSessionService(CodexClient& client, ArchiveViewModel& model, ArchiveRefreshHandler refresh);

    // Charge exhaustivement les threads archives si le cache est invalide.
    void LoadArchive(VoidCompletion completion);

    // Restaure un thread puis invalide le cache et demande une sync.
    void RestoreArchivedThread(CodexThreadId thread_id, VoidCompletion completion);

private:
    // Client app-server non possede.
    CodexClient& client_;
    // Modele archive non possede.
    ArchiveViewModel& model_;
    // Callback de reconciliation.
    ArchiveRefreshHandler refresh_;
};

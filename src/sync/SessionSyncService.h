// ============================================================================
// Codex Deck - Service de synchronisation sessions
// ----------------------------------------------------------------------------
// Ce module fusionne le cache SQLite et les threads Codex en snapshots
// immuables publiables par l'UI.
// ============================================================================

#pragma once

#include "../codex/CodexClient.h"
#include "../model/SessionCatalog.h"
#include "../projects/ProjectDetector.h"
#include "../storage/SqliteDatabase.h"

#include <expected>
#include <functional>
#include <memory>
#include <mutex>

// Loader injectable de listes Codex.
using ThreadListLoader = std::move_only_function<std::expected<std::vector<CodexThreadSummary>, CodexError>(ThreadListOptions)>;

// ----------------------------------------------------------------------------
// Synchronise sessions locales et Codex.
// ----------------------------------------------------------------------------
class SessionSyncService {
public:
    // ------------------------------------------------------------------------
    // Cree un service de sync.
    //
    // Parametres :
    // - database : base SQLite non possedee.
    // - catalog : catalogue a publier.
    // - git_probe : probe Git injectable.
    // - loader : fournisseur de threads Codex.
    // ------------------------------------------------------------------------
    SessionSyncService(
        SqliteDatabase& database,
        SessionCatalog& catalog,
        IGitProjectProbe& git_probe,
        ThreadListLoader loader
    );

    // ------------------------------------------------------------------------
    // Charge l'etat cache sans appeler Codex.
    //
    // Retour :
    // - snapshot publie ou erreur de stockage.
    // ------------------------------------------------------------------------
    std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> LoadCachedState();

    // ------------------------------------------------------------------------
    // Rafraichit depuis Codex puis publie une reconciliation.
    //
    // Retour :
    // - snapshot publie ou erreur de stockage.
    // ------------------------------------------------------------------------
    std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> RefreshFromCodex();

    // ------------------------------------------------------------------------
    // Demande un refresh Codex en coalescant les appels concurrents.
    //
    // Retour :
    // - dernier snapshot publie, ou erreur du refresh executeur.
    // ------------------------------------------------------------------------
    std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> RequestRefreshFromCodex();

private:
    // Base SQLite non possedee.
    SqliteDatabase& database_;

    // Catalogue publie non possede.
    SessionCatalog& catalog_;

    // Probe Git non possede.
    IGitProjectProbe& git_probe_;

    // Loader Codex injectable.
    ThreadListLoader loader_;

    // Protege l'etat de coalescing des refresh.
    std::mutex refresh_mutex_;

    // Indique qu'un refresh est deja en cours.
    bool refresh_active_ = false;

    // Indique qu'un autre passage est demande apres le refresh courant.
    bool refresh_requested_again_ = false;
};

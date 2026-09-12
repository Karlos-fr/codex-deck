// ============================================================================
// Codex Deck - Catalogue de sessions
// ----------------------------------------------------------------------------
// Ce module publie des snapshots immuables par valeur pour les vues UI futures.
// Les mutations restent confinees aux services worker.
// ============================================================================

#pragma once

#include "SessionRecord.h"

#include "../projects/ProjectTypes.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

// ----------------------------------------------------------------------------
// Snapshot immutable du catalogue projets/sessions.
// ----------------------------------------------------------------------------
struct SessionCatalogSnapshot {
    // Projets locaux connus.
    std::vector<Project> projects;

    // Sessions fusionnees.
    std::vector<SessionRecord> sessions;

    // Revision monotone de publication.
    std::uint64_t revision = 0;
};

// ----------------------------------------------------------------------------
// Publie et conserve le dernier snapshot.
// ----------------------------------------------------------------------------
class SessionCatalog {
public:
    // ------------------------------------------------------------------------
    // Publie un nouveau snapshot.
    //
    // Parametres :
    // - snapshot : donnees a publier.
    //
    // Retour :
    // - snapshot partage immutable.
    // ------------------------------------------------------------------------
    std::shared_ptr<const SessionCatalogSnapshot> Publish(SessionCatalogSnapshot snapshot);

    // ------------------------------------------------------------------------
    // Retourne le dernier snapshot publie.
    //
    // Retour :
    // - snapshot courant ou nullptr.
    // ------------------------------------------------------------------------
    std::shared_ptr<const SessionCatalogSnapshot> Current() const;

private:
    // Protege la publication et la revision.
    mutable std::mutex mutex_;

    // Revision monotone interne.
    std::uint64_t revision_ = 0;

    // Dernier snapshot publie.
    std::shared_ptr<const SessionCatalogSnapshot> current_;
};

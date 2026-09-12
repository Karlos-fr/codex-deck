// ============================================================================
// Codex Deck - Implementation du catalogue de sessions
// ----------------------------------------------------------------------------
// Ce fichier fournit une publication thread-safe par snapshots immuables.
// ============================================================================

#include "SessionCatalog.h"

// ----------------------------------------------------------------------------
// Publie un nouveau snapshot.
// ----------------------------------------------------------------------------
std::shared_ptr<const SessionCatalogSnapshot> SessionCatalog::Publish(SessionCatalogSnapshot snapshot) {
    std::lock_guard lock(mutex_);
    snapshot.revision = ++revision_;
    current_ = std::make_shared<const SessionCatalogSnapshot>(std::move(snapshot));
    return current_;
}

// ----------------------------------------------------------------------------
// Retourne le dernier snapshot publie.
// ----------------------------------------------------------------------------
std::shared_ptr<const SessionCatalogSnapshot> SessionCatalog::Current() const {
    std::lock_guard lock(mutex_);
    return current_;
}

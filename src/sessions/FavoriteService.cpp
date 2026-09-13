// ============================================================================
// Codex Deck - Implementation du service de favoris
// ----------------------------------------------------------------------------
// Ce fichier garantit le retour au snapshot precedent si SQLite refuse la
// mutation optimiste.
// ============================================================================

#include "FavoriteService.h"

// Cree un service sur un catalogue et un repository non possedes.
FavoriteService::FavoriteService(SessionCatalog& catalog, SessionMetadataRepository& repository)
    : catalog_(catalog), repository_(repository) {
}

// Bascule le favori d'un thread avec rollback en cas d'erreur.
std::expected<bool, StorageError> FavoriteService::ToggleFavorite(const CodexThreadId& thread_id) {
    const auto previous = catalog_.Current();
    if (!previous) {
        return std::unexpected(StorageError{StorageErrorCode::ConstraintFailed, 0, "Catalogue indisponible"});
    }
    SessionCatalogSnapshot optimistic = *previous;
    bool found = false;
    bool favorite = false;
    for (SessionRecord& session : optimistic.sessions) {
        if (session.codex.id == thread_id) {
            session.favorite = !session.favorite;
            favorite = session.favorite;
            found = true;
            break;
        }
    }
    if (!found) {
        return std::unexpected(StorageError{StorageErrorCode::ConstraintFailed, 0, "Session introuvable"});
    }
    catalog_.Publish(std::move(optimistic));
    if (auto persisted = repository_.SetFavorite(thread_id, favorite); !persisted) {
        catalog_.Publish(*previous);
        return std::unexpected(persisted.error());
    }
    return favorite;
}

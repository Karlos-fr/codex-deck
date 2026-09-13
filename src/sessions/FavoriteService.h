// ============================================================================
// Codex Deck - Service de favoris
// ----------------------------------------------------------------------------
// Ce module publie un toggle optimiste puis le persiste avec rollback, sans
// exposer SQLite a la navigation.
// ============================================================================

#pragma once

#include "../model/SessionCatalog.h"
#include "../storage/SessionMetadataRepository.h"

// Gere les favoris locaux des sessions actives.
class FavoriteService {
public:
    // Cree un service sur un catalogue et un repository non possedes.
    FavoriteService(SessionCatalog& catalog, SessionMetadataRepository& repository);

    // Bascule le favori d'un thread avec rollback en cas d'erreur.
    std::expected<bool, StorageError> ToggleFavorite(const CodexThreadId& thread_id);

private:
    // Catalogue publie non possede.
    SessionCatalog& catalog_;
    // Repository local non possede.
    SessionMetadataRepository& repository_;
};

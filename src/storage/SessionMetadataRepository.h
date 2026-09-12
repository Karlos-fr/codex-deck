// ============================================================================
// Codex Deck - Repository metadonnees de session
// ----------------------------------------------------------------------------
// Ce module persiste le cache leger des sessions Codex et leur association
// locale aux projets Codex Deck.
// ============================================================================

#pragma once

#include "SqliteDatabase.h"

#include "../projects/ProjectTypes.h"

#include <expected>
#include <optional>

// ----------------------------------------------------------------------------
// Acces SQLite aux metadonnees de sessions.
// ----------------------------------------------------------------------------
class SessionMetadataRepository {
public:
    // ------------------------------------------------------------------------
    // Cree un repository sur une base ouverte.
    //
    // Parametres :
    // - database : base SQLite non possedee.
    // ------------------------------------------------------------------------
    explicit SessionMetadataRepository(SqliteDatabase& database);

    // ------------------------------------------------------------------------
    // Lit les metadonnees d'un thread.
    //
    // Parametres :
    // - thread_id : identifiant Codex.
    //
    // Retour :
    // - metadonnees optionnelles ou erreur.
    // ------------------------------------------------------------------------
    std::expected<std::optional<SessionMetadata>, StorageError> Get(const CodexThreadId& thread_id);

    // ------------------------------------------------------------------------
    // Met a jour le cache leger d'un thread.
    //
    // Parametres :
    // - thread_id : identifiant Codex.
    // - title : titre de cache.
    // - cwd : repertoire de travail de cache.
    // - last_activity : activite recente.
    // - status : statut connu.
    //
    // Retour :
    // - succes vide ou erreur.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> UpsertCache(
        const CodexThreadId& thread_id,
        std::string_view title,
        const std::filesystem::path& cwd,
        std::int64_t last_activity,
        SessionStatus status
    );

    // ------------------------------------------------------------------------
    // Definit l'association projet d'un thread.
    //
    // Parametres :
    // - thread_id : identifiant Codex.
    // - project_id : projet ou Unassigned.
    // - source : source de l'association.
    //
    // Retour :
    // - succes vide ou erreur.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> SetAssignment(
        const CodexThreadId& thread_id,
        std::optional<ProjectId> project_id,
        AssignmentSource source
    );

    // ------------------------------------------------------------------------
    // Definit le favori d'un thread.
    //
    // Parametres :
    // - thread_id : identifiant Codex.
    // - favorite : nouvel etat favori.
    //
    // Retour :
    // - succes vide ou erreur.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> SetFavorite(const CodexThreadId& thread_id, bool favorite);

    // ------------------------------------------------------------------------
    // Liste toutes les metadonnees locales.
    //
    // Retour :
    // - liste complete ou erreur.
    // ------------------------------------------------------------------------
    std::expected<std::vector<SessionMetadata>, StorageError> ListAll();

private:
    // Base SQLite non possedee.
    SqliteDatabase& database_;
};

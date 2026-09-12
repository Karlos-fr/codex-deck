// ============================================================================
// Codex Deck - Implementation du repository metadonnees
// ----------------------------------------------------------------------------
// Ce fichier utilise des statements prepares pour stocker le cache leger de
// session et preserver les assignations locales.
// ============================================================================

#include "SessionMetadataRepository.h"

#include <sqlite3.h>

#include <chrono>

namespace {

// ----------------------------------------------------------------------------
// Retourne le timestamp courant.
//
// Retour :
// - millisecondes depuis epoch systeme.
// ----------------------------------------------------------------------------
std::int64_t NowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

// ----------------------------------------------------------------------------
// Convertit un statut en entier stocke.
//
// Parametres :
// - status : statut runtime.
//
// Retour :
// - valeur persistante.
// ----------------------------------------------------------------------------
int StatusToInt(SessionStatus status) {
    return static_cast<int>(status);
}

// ----------------------------------------------------------------------------
// Convertit un entier stocke en statut.
//
// Parametres :
// - value : valeur SQLite.
//
// Retour :
// - statut runtime.
// ----------------------------------------------------------------------------
SessionStatus StatusFromInt(int value) {
    switch (value) {
    case static_cast<int>(SessionStatus::Working):
        return SessionStatus::Working;
    case static_cast<int>(SessionStatus::NeedsAttention):
        return SessionStatus::NeedsAttention;
    case static_cast<int>(SessionStatus::Completed):
        return SessionStatus::Completed;
    case static_cast<int>(SessionStatus::Error):
        return SessionStatus::Error;
    case static_cast<int>(SessionStatus::Idle):
    default:
        return SessionStatus::Idle;
    }
}

// ----------------------------------------------------------------------------
// Construit une erreur SQL depuis la base.
//
// Parametres :
// - database : base source.
// - code : code SQLite.
//
// Retour :
// - erreur structuree.
// ----------------------------------------------------------------------------
StorageError SqlError(SqliteDatabase& database, int code) {
    return StorageError{
        code == SQLITE_CONSTRAINT ? StorageErrorCode::ConstraintFailed : StorageErrorCode::SqlFailed,
        code,
        sqlite3_errmsg(database.handle()),
    };
}

// ----------------------------------------------------------------------------
// Finalise automatiquement un statement SQLite.
// ----------------------------------------------------------------------------
class Statement {
public:
    // ------------------------------------------------------------------------
    // Prepare une instruction.
    //
    // Parametres :
    // - database : base source.
    // - sql : instruction SQL.
    // ------------------------------------------------------------------------
    Statement(SqliteDatabase& database, const char* sql)
        : database_(database) {
        result_ = sqlite3_prepare_v2(database_.handle(), sql, -1, &statement_, nullptr);
    }

    // ------------------------------------------------------------------------
    // Finalise l'instruction.
    // ------------------------------------------------------------------------
    ~Statement() {
        if (statement_ != nullptr) {
            sqlite3_finalize(statement_);
        }
    }

    // ------------------------------------------------------------------------
    // Indique si la preparation a reussi.
    //
    // Retour :
    // - true si le statement est utilisable.
    // ------------------------------------------------------------------------
    bool ok() const {
        return result_ == SQLITE_OK;
    }

    // ------------------------------------------------------------------------
    // Retourne l'erreur de preparation.
    //
    // Retour :
    // - erreur SQLite.
    // ------------------------------------------------------------------------
    StorageError error() {
        return SqlError(database_, result_);
    }

    // ------------------------------------------------------------------------
    // Retourne le statement brut.
    //
    // Retour :
    // - pointeur sqlite3_stmt.
    // ------------------------------------------------------------------------
    sqlite3_stmt* get() const {
        return statement_;
    }

private:
    SqliteDatabase& database_;
    sqlite3_stmt* statement_ = nullptr;
    int result_ = SQLITE_OK;
};

// ----------------------------------------------------------------------------
// Lit une chaine SQLite optionnelle.
//
// Parametres :
// - statement : statement positionne sur une ligne.
// - column : index de colonne.
//
// Retour :
// - chaine UTF-8.
// ----------------------------------------------------------------------------
std::string ColumnText(sqlite3_stmt* statement, int column) {
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text == nullptr ? std::string{} : reinterpret_cast<const char*>(text);
}

// ----------------------------------------------------------------------------
// Lit une metadonnee depuis la ligne courante.
//
// Parametres :
// - statement : statement SELECT positionne.
//
// Retour :
// - metadonnees hydratees.
// ----------------------------------------------------------------------------
SessionMetadata ReadMetadata(sqlite3_stmt* statement) {
    SessionMetadata metadata{};
    metadata.thread_id = ColumnText(statement, 0);
    if (sqlite3_column_type(statement, 1) != SQLITE_NULL) {
        metadata.project_id = sqlite3_column_int64(statement, 1);
    }
    metadata.assignment_source = static_cast<AssignmentSource>(sqlite3_column_int(statement, 2));
    metadata.favorite = sqlite3_column_int(statement, 3) != 0;
    metadata.cached_title = ColumnText(statement, 4);
    metadata.cached_cwd = ColumnText(statement, 5);
    metadata.last_activity = sqlite3_column_int64(statement, 6);
    metadata.last_known_status = StatusFromInt(sqlite3_column_int(statement, 7));
    return metadata;
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree un repository sur une base ouverte.
// ----------------------------------------------------------------------------
SessionMetadataRepository::SessionMetadataRepository(SqliteDatabase& database)
    : database_(database) {
}

// ----------------------------------------------------------------------------
// Lit les metadonnees d'un thread.
// ----------------------------------------------------------------------------
std::expected<std::optional<SessionMetadata>, StorageError> SessionMetadataRepository::Get(const CodexThreadId& thread_id) {
    Statement statement(
        database_,
        "SELECT thread_id, project_id, assignment_source, favorite, cached_title, cached_cwd, last_activity, last_known_status "
        "FROM session_metadata WHERE thread_id=?;"
    );
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_text(statement.get(), 1, thread_id.c_str(), -1, SQLITE_TRANSIENT);
    const int step = sqlite3_step(statement.get());
    if (step == SQLITE_DONE) {
        return std::optional<SessionMetadata>{};
    }
    if (step != SQLITE_ROW) {
        return std::unexpected(SqlError(database_, step));
    }
    return std::optional<SessionMetadata>{ReadMetadata(statement.get())};
}

// ----------------------------------------------------------------------------
// Met a jour le cache leger d'un thread.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> SessionMetadataRepository::UpsertCache(
    const CodexThreadId& thread_id,
    std::string_view title,
    const std::filesystem::path& cwd,
    std::int64_t last_activity,
    SessionStatus status
) {
    Statement statement(
        database_,
        "INSERT INTO session_metadata(thread_id, cached_title, cached_cwd, last_activity, last_known_status, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(thread_id) DO UPDATE SET "
        "cached_title=excluded.cached_title, cached_cwd=excluded.cached_cwd, "
        "last_activity=excluded.last_activity, last_known_status=excluded.last_known_status, updated_at=excluded.updated_at;"
    );
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    const std::string cwd_text = cwd.string();
    sqlite3_bind_text(statement.get(), 1, thread_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 2, title.data(), static_cast<int>(title.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 3, cwd_text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 4, last_activity);
    sqlite3_bind_int(statement.get(), 5, StatusToInt(status));
    sqlite3_bind_int64(statement.get(), 6, NowMillis());
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ----------------------------------------------------------------------------
// Definit l'association projet d'un thread.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> SessionMetadataRepository::SetAssignment(
    const CodexThreadId& thread_id,
    std::optional<ProjectId> project_id,
    AssignmentSource source
) {
    Statement statement(
        database_,
        "INSERT INTO session_metadata(thread_id, project_id, assignment_source, updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(thread_id) DO UPDATE SET project_id=excluded.project_id, "
        "assignment_source=excluded.assignment_source, updated_at=excluded.updated_at;"
    );
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_text(statement.get(), 1, thread_id.c_str(), -1, SQLITE_TRANSIENT);
    if (project_id) {
        sqlite3_bind_int64(statement.get(), 2, *project_id);
    } else {
        sqlite3_bind_null(statement.get(), 2);
    }
    sqlite3_bind_int(statement.get(), 3, static_cast<int>(source));
    sqlite3_bind_int64(statement.get(), 4, NowMillis());
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ----------------------------------------------------------------------------
// Definit le favori d'un thread.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> SessionMetadataRepository::SetFavorite(const CodexThreadId& thread_id, bool favorite) {
    Statement statement(
        database_,
        "INSERT INTO session_metadata(thread_id, favorite, updated_at) VALUES(?, ?, ?) "
        "ON CONFLICT(thread_id) DO UPDATE SET favorite=excluded.favorite, updated_at=excluded.updated_at;"
    );
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_text(statement.get(), 1, thread_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement.get(), 2, favorite ? 1 : 0);
    sqlite3_bind_int64(statement.get(), 3, NowMillis());
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ----------------------------------------------------------------------------
// Liste toutes les metadonnees locales.
// ----------------------------------------------------------------------------
std::expected<std::vector<SessionMetadata>, StorageError> SessionMetadataRepository::ListAll() {
    Statement statement(
        database_,
        "SELECT thread_id, project_id, assignment_source, favorite, cached_title, cached_cwd, last_activity, last_known_status "
        "FROM session_metadata ORDER BY last_activity DESC;"
    );
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }

    std::vector<SessionMetadata> result;
    while (true) {
        const int step = sqlite3_step(statement.get());
        if (step == SQLITE_DONE) {
            break;
        }
        if (step != SQLITE_ROW) {
            return std::unexpected(SqlError(database_, step));
        }
        result.push_back(ReadMetadata(statement.get()));
    }
    return result;
}

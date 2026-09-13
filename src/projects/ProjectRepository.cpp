// ============================================================================
// Codex Deck - Implementation du repository projets
// ----------------------------------------------------------------------------
// Ce fichier utilise des statements prepares pour toutes les valeurs utilisateur
// et garde les transactions multi-table locales.
// ============================================================================

#include "ProjectRepository.h"

#include "PathNormalization.h"

#include <sqlite3.h>

#include <chrono>
#include <map>

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

}  // namespace

// ----------------------------------------------------------------------------
// Cree un repository sur une base ouverte.
// ----------------------------------------------------------------------------
ProjectRepository::ProjectRepository(SqliteDatabase& database)
    : database_(database) {
}

// ----------------------------------------------------------------------------
// Cree un projet sans root initial.
// ----------------------------------------------------------------------------
std::expected<Project, StorageError> ProjectRepository::Create(std::string_view name) {
    const std::int64_t now = NowMillis();
    Statement statement(database_, "INSERT INTO projects(name, created_at, updated_at) VALUES(?, ?, ?);");
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_text(statement.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 2, now);
    sqlite3_bind_int64(statement.get(), 3, now);
    const int step = sqlite3_step(statement.get());
    if (step != SQLITE_DONE) {
        return std::unexpected(SqlError(database_, step));
    }

    Project project{};
    project.id = sqlite3_last_insert_rowid(database_.handle());
    project.name = std::string(name);
    project.created_at = now;
    project.updated_at = now;
    return project;
}

// ----------------------------------------------------------------------------
// Liste les projets et leurs roots.
// ----------------------------------------------------------------------------
std::expected<std::vector<Project>, StorageError> ProjectRepository::List() {
    std::map<ProjectId, Project> projects;
    Statement project_statement(database_, "SELECT id, name, git_remote, created_at, updated_at FROM projects ORDER BY id;");
    if (!project_statement.ok()) {
        return std::unexpected(project_statement.error());
    }
    while (true) {
        const int step = sqlite3_step(project_statement.get());
        if (step == SQLITE_DONE) {
            break;
        }
        if (step != SQLITE_ROW) {
            return std::unexpected(SqlError(database_, step));
        }
        Project project{};
        project.id = sqlite3_column_int64(project_statement.get(), 0);
        project.name = reinterpret_cast<const char*>(sqlite3_column_text(project_statement.get(), 1));
        if (sqlite3_column_type(project_statement.get(), 2) != SQLITE_NULL) {
            project.git_remote = reinterpret_cast<const char*>(sqlite3_column_text(project_statement.get(), 2));
        }
        project.created_at = sqlite3_column_int64(project_statement.get(), 3);
        project.updated_at = sqlite3_column_int64(project_statement.get(), 4);
        projects.emplace(project.id, std::move(project));
    }

    Statement root_statement(database_, "SELECT project_id, root_path FROM project_roots ORDER BY project_id, normalized_path;");
    if (!root_statement.ok()) {
        return std::unexpected(root_statement.error());
    }
    while (true) {
        const int step = sqlite3_step(root_statement.get());
        if (step == SQLITE_DONE) {
            break;
        }
        if (step != SQLITE_ROW) {
            return std::unexpected(SqlError(database_, step));
        }
        const ProjectId project_id = sqlite3_column_int64(root_statement.get(), 0);
        const char* root = reinterpret_cast<const char*>(sqlite3_column_text(root_statement.get(), 1));
        if (auto found = projects.find(project_id); found != projects.end() && root != nullptr) {
            found->second.roots.emplace_back(root);
        }
    }

    std::vector<Project> result;
    for (auto& [id, project] : projects) {
        (void)id;
        result.push_back(std::move(project));
    }
    return result;
}

// Renomme un projet logique existant.
std::expected<void, StorageError> ProjectRepository::Rename(ProjectId project_id, std::string_view name) {
    Statement statement(database_, "UPDATE projects SET name=?, updated_at=? WHERE id=?;");
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_text(statement.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 2, NowMillis());
    sqlite3_bind_int64(statement.get(), 3, project_id);
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ----------------------------------------------------------------------------
// Ajoute une racine a un projet.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> ProjectRepository::AddRoot(ProjectId project_id, const std::filesystem::path& root) {
    Statement statement(
        database_,
        "INSERT OR REPLACE INTO project_roots(project_id, root_path, normalized_path) VALUES(?, ?, ?);"
    );
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    const std::string root_text = root.string();
    const std::wstring normalized = NormalizeWindowsPath(root);
    const std::string normalized_text = std::filesystem::path(normalized).string();
    sqlite3_bind_int64(statement.get(), 1, project_id);
    sqlite3_bind_text(statement.get(), 2, root_text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 3, normalized_text.c_str(), -1, SQLITE_TRANSIENT);
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ----------------------------------------------------------------------------
// Definit le remote Git d'un projet.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> ProjectRepository::SetGitRemote(ProjectId project_id, std::string_view remote) {
    Statement statement(database_, "UPDATE projects SET git_remote=?, updated_at=? WHERE id=?;");
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_text(statement.get(), 1, remote.data(), static_cast<int>(remote.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 2, NowMillis());
    sqlite3_bind_int64(statement.get(), 3, project_id);
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ----------------------------------------------------------------------------
// Supprime un projet et ses roots.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> ProjectRepository::Delete(ProjectId project_id) {
    Statement statement(database_, "DELETE FROM projects WHERE id=?;");
    if (!statement.ok()) {
        return std::unexpected(statement.error());
    }
    sqlite3_bind_int64(statement.get(), 1, project_id);
    const int step = sqlite3_step(statement.get());
    return step == SQLITE_DONE ? std::expected<void, StorageError>{} : std::unexpected(SqlError(database_, step));
}

// ============================================================================
// Codex Deck - Implementation des migrations SQLite
// ----------------------------------------------------------------------------
// Ce fichier installe le schema v1 dedie a l'organisation locale et au cache
// leger, sans stocker l'historique complet Codex.
// ============================================================================

#include "SchemaMigrator.h"

namespace {

// Schema initial de Codex Deck.
constexpr char kSchemaV1[] = R"sql(
BEGIN IMMEDIATE;

CREATE TABLE IF NOT EXISTS app_meta(
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS projects(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  git_remote TEXT,
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS project_roots(
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  root_path TEXT NOT NULL,
  normalized_path TEXT NOT NULL,
  PRIMARY KEY(project_id, normalized_path)
);

CREATE INDEX IF NOT EXISTS idx_project_roots_normalized ON project_roots(normalized_path);

CREATE TABLE IF NOT EXISTS session_metadata(
  thread_id TEXT PRIMARY KEY,
  project_id INTEGER REFERENCES projects(id) ON DELETE SET NULL,
  assignment_source INTEGER NOT NULL DEFAULT 0,
  favorite INTEGER NOT NULL DEFAULT 0,
  cached_title TEXT NOT NULL DEFAULT '',
  cached_cwd TEXT NOT NULL DEFAULT '',
  last_activity INTEGER NOT NULL DEFAULT 0,
  last_known_status INTEGER NOT NULL DEFAULT 0,
  updated_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_session_project ON session_metadata(project_id);
CREATE INDEX IF NOT EXISTS idx_session_activity ON session_metadata(last_activity DESC);

CREATE TABLE IF NOT EXISTS workspace_state(
  key TEXT PRIMARY KEY,
  value_json TEXT NOT NULL
);

INSERT INTO app_meta(key, value)
VALUES('schema_version', '1')
ON CONFLICT(key) DO UPDATE SET value=excluded.value;

COMMIT;
)sql";

}  // namespace

// ----------------------------------------------------------------------------
// Migre la base vers la derniere version connue.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> SchemaMigrator::Migrate(SqliteDatabase& database) {
    auto result = database.Execute(kSchemaV1);
    if (!result) {
        [[maybe_unused]] const auto rollback = database.Execute("ROLLBACK;");
        return std::unexpected(StorageError{
            StorageErrorCode::MigrationFailed,
            result.error().sqlite_code,
            result.error().message,
        });
    }
    return {};
}

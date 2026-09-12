// ============================================================================
// Codex Deck - Implementation du wrapper SQLite
// ----------------------------------------------------------------------------
// Ce fichier encapsule l'ouverture, la fermeture et l'execution SQL simple pour
// les repositories locaux.
// ============================================================================

#include "SqliteDatabase.h"

#include <utility>

namespace {

// ----------------------------------------------------------------------------
// Construit une erreur depuis le handle SQLite.
//
// Parametres :
// - code : famille d'erreur.
// - database : handle SQLite.
// - fallback : message de repli.
//
// Retour :
// - erreur structuree.
// ----------------------------------------------------------------------------
StorageError MakeSqliteError(StorageErrorCode code, sqlite3* database, const char* fallback) {
    const int sqlite_code = database == nullptr ? SQLITE_ERROR : sqlite3_errcode(database);
    const char* message = database == nullptr ? fallback : sqlite3_errmsg(database);
    return StorageError{code, sqlite_code, message == nullptr ? fallback : message};
}

}  // namespace

// ----------------------------------------------------------------------------
// Prend possession d'un handle SQLite.
// ----------------------------------------------------------------------------
SqliteDatabase::SqliteDatabase(sqlite3* handle)
    : handle_(handle) {
}

// ----------------------------------------------------------------------------
// Ferme le handle SQLite possede.
// ----------------------------------------------------------------------------
SqliteDatabase::~SqliteDatabase() {
    if (handle_ != nullptr) {
        sqlite3_close(handle_);
        handle_ = nullptr;
    }
}

// ----------------------------------------------------------------------------
// Deplace la possession d'une connexion.
// ----------------------------------------------------------------------------
SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)) {
}

// ----------------------------------------------------------------------------
// Deplace la possession d'une connexion.
// ----------------------------------------------------------------------------
SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept {
    if (this != &other) {
        if (handle_ != nullptr) {
            sqlite3_close(handle_);
        }
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

// ----------------------------------------------------------------------------
// Execute une instruction SQL sans resultats.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> SqliteDatabase::Execute(std::string_view sql) {
    char* error_message = nullptr;
    const int result = sqlite3_exec(handle_, std::string(sql).c_str(), nullptr, nullptr, &error_message);
    if (result != SQLITE_OK) {
        StorageError error{
            result == SQLITE_CONSTRAINT ? StorageErrorCode::ConstraintFailed : StorageErrorCode::SqlFailed,
            result,
            error_message == nullptr ? sqlite3_errmsg(handle_) : error_message,
        };
        sqlite3_free(error_message);
        return std::unexpected(std::move(error));
    }
    return {};
}

// ----------------------------------------------------------------------------
// Retourne le handle SQLite non possede.
// ----------------------------------------------------------------------------
sqlite3* SqliteDatabase::handle() const {
    return handle_;
}

// ----------------------------------------------------------------------------
// Ouvre une base SQLite et active les pragmas de Codex Deck.
// ----------------------------------------------------------------------------
std::expected<SqliteDatabase, StorageError> OpenDatabase(const std::filesystem::path& path) {
    sqlite3* handle = nullptr;
    const int result = sqlite3_open_v2(
        path.string().c_str(),
        &handle,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
        nullptr
    );
    if (result != SQLITE_OK) {
        StorageError error = MakeSqliteError(StorageErrorCode::OpenFailed, handle, "open failed");
        if (handle != nullptr) {
            sqlite3_close(handle);
        }
        return std::unexpected(std::move(error));
    }

    SqliteDatabase database(handle);
    if (auto pragma = database.Execute("PRAGMA foreign_keys=ON;"); !pragma) {
        return std::unexpected(pragma.error());
    }
    if (auto pragma = database.Execute("PRAGMA journal_mode=WAL;"); !pragma) {
        return std::unexpected(pragma.error());
    }
    if (auto pragma = database.Execute("PRAGMA synchronous=NORMAL;"); !pragma) {
        return std::unexpected(pragma.error());
    }
    return database;
}

// ============================================================================
// Codex Deck - Wrapper RAII SQLite
// ----------------------------------------------------------------------------
// Ce module possede un handle sqlite3 move-only et expose seulement les
// operations communes necessaires aux repositories.
// ============================================================================

#pragma once

#include "StorageError.h"

#include <sqlite3.h>

#include <expected>
#include <filesystem>
#include <string_view>

// ----------------------------------------------------------------------------
// Possede une connexion SQLite.
// ----------------------------------------------------------------------------
class SqliteDatabase {
public:
    // ------------------------------------------------------------------------
    // Cree une base vide sans handle.
    // ------------------------------------------------------------------------
    SqliteDatabase() = default;

    // ------------------------------------------------------------------------
    // Prend possession d'un handle SQLite.
    //
    // Parametres :
    // - handle : handle ouvert a fermer en destructeur.
    // ------------------------------------------------------------------------
    explicit SqliteDatabase(sqlite3* handle);

    // ------------------------------------------------------------------------
    // Ferme le handle SQLite possede.
    // ------------------------------------------------------------------------
    ~SqliteDatabase();

    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    // ------------------------------------------------------------------------
    // Deplace la possession d'une connexion.
    //
    // Parametres :
    // - other : base source.
    // ------------------------------------------------------------------------
    SqliteDatabase(SqliteDatabase&& other) noexcept;

    // ------------------------------------------------------------------------
    // Deplace la possession d'une connexion.
    //
    // Parametres :
    // - other : base source.
    //
    // Retour :
    // - reference vers cette base.
    // ------------------------------------------------------------------------
    SqliteDatabase& operator=(SqliteDatabase&& other) noexcept;

    // ------------------------------------------------------------------------
    // Execute une instruction SQL sans resultats.
    //
    // Parametres :
    // - sql : instruction SQL.
    //
    // Retour :
    // - succes vide ou erreur SQLite.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> Execute(std::string_view sql);

    // ------------------------------------------------------------------------
    // Retourne le handle SQLite non possede.
    //
    // Retour :
    // - handle SQLite courant.
    // ------------------------------------------------------------------------
    sqlite3* handle() const;

private:
    // Handle SQLite possede.
    sqlite3* handle_ = nullptr;
};

// ----------------------------------------------------------------------------
// Ouvre une base SQLite et active les pragmas de Codex Deck.
//
// Parametres :
// - path : chemin du fichier SQLite.
//
// Retour :
// - base ouverte ou erreur.
// ----------------------------------------------------------------------------
std::expected<SqliteDatabase, StorageError> OpenDatabase(const std::filesystem::path& path);

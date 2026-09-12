// ============================================================================
// Codex Deck - Migrations SQLite
// ----------------------------------------------------------------------------
// Ce module cree et fait evoluer le schema local leger de Codex Deck.
// ============================================================================

#pragma once

#include "SqliteDatabase.h"

// ----------------------------------------------------------------------------
// Applique les migrations de schema connues.
// ----------------------------------------------------------------------------
class SchemaMigrator {
public:
    // ------------------------------------------------------------------------
    // Migre la base vers la derniere version connue.
    //
    // Parametres :
    // - database : base ouverte.
    //
    // Retour :
    // - succes vide ou erreur de migration.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> Migrate(SqliteDatabase& database);
};

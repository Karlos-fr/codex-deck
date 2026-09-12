// ============================================================================
// Codex Deck - Erreurs de stockage
// ----------------------------------------------------------------------------
// Ce module definit les erreurs structurees produites par SQLite et les
// migrations locales de Codex Deck.
// ============================================================================

#pragma once

#include <string>

// ----------------------------------------------------------------------------
// Classe les erreurs possibles du stockage local.
// ----------------------------------------------------------------------------
enum class StorageErrorCode {
    // L'ouverture de la base a echoue.
    OpenFailed,

    // Une instruction SQL a echoue.
    SqlFailed,

    // Une migration de schema a echoue.
    MigrationFailed,

    // Une contrainte de donnees a ete violee.
    ConstraintFailed,
};

// ----------------------------------------------------------------------------
// Decrit une erreur SQLite ou migration.
// ----------------------------------------------------------------------------
struct StorageError {
    // Famille d'erreur.
    StorageErrorCode code;

    // Code SQLite brut lorsque disponible.
    int sqlite_code = 0;

    // Message technique fourni par SQLite ou le stockage.
    std::string message;
};

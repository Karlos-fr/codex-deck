// ============================================================================
// Codex Deck - Service de preferences
// ----------------------------------------------------------------------------
// Ce module lit et ecrit le document JSON versionne des preferences dans
// workspace_state, sans gerer le scheduling du worker appelant.
// ============================================================================

#pragma once

#include "DeckPreferences.h"

#include "../storage/SqliteDatabase.h"

// Persiste les preferences globales dans SQLite.
class DeckPreferencesService {
public:
    // Cree le service sur une base non possedee.
    explicit DeckPreferencesService(SqliteDatabase& database);

    // Charge les preferences ou retourne les valeurs par defaut si absentes.
    std::expected<DeckPreferences, StorageError> Load();

    // Sauvegarde le document JSON versionne.
    std::expected<void, StorageError> Save(const DeckPreferences& preferences);

private:
    // Base SQLite non possedee.
    SqliteDatabase& database_;
};

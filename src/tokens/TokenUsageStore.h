// ============================================================================
// Codex Glass - Stockage incremental de consommation de tokens
// ----------------------------------------------------------------------------
// Ce fichier declare le cache SQLite des curseurs JSONL et des agregats locaux.
// Il ne conserve ni contenu de conversation ni identifiant d'authentification.
// ============================================================================

#pragma once

#include "CodexSessionScanner.h"

#include <string>

struct sqlite3;

// ----------------------------------------------------------------------------
// Gere les tables SQLite reservees aux scans locaux de tokens.
// ----------------------------------------------------------------------------
class TokenUsageStore {
public:
    // ------------------------------------------------------------------------
    // Cree un store ferme.
    // ------------------------------------------------------------------------
    TokenUsageStore() = default;

    // ------------------------------------------------------------------------
    // Ferme automatiquement la connexion.
    // ------------------------------------------------------------------------
    ~TokenUsageStore();

    TokenUsageStore(const TokenUsageStore&) = delete;
    TokenUsageStore& operator=(const TokenUsageStore&) = delete;

    // ------------------------------------------------------------------------
    // Ouvre la base partagee de Codex Glass et initialise les tables tokens.
    // ------------------------------------------------------------------------
    bool Open(const std::wstring& database_path);

    // ------------------------------------------------------------------------
    // Ferme la connexion courante.
    // ------------------------------------------------------------------------
    void Close();

    // ------------------------------------------------------------------------
    // Charge les curseurs et agregats techniques par fichier.
    // ------------------------------------------------------------------------
    bool LoadScanCache(TokenScanCache& cache);

    // ------------------------------------------------------------------------
    // Enregistre atomiquement le cache et le snapshot horaire et quotidien courant.
    // ------------------------------------------------------------------------
    bool Save(const TokenScanCache& cache, const TokenUsageSnapshot& snapshot);

    // ------------------------------------------------------------------------
    // Charge le dernier snapshot horaire et quotidien valide en solution de repli.
    // ------------------------------------------------------------------------
    bool LoadLatestSnapshot(TokenUsageSnapshot& snapshot);

    // ------------------------------------------------------------------------
    // Retourne la derniere erreur SQLite lisible.
    // ------------------------------------------------------------------------
    const std::wstring& LastError() const;

private:
    // Connexion SQLite possedee par ce store.
    sqlite3* database_ = nullptr;

    // Derniere erreur exposee a l'appelant.
    std::wstring last_error_;
};

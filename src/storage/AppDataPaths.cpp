// ============================================================================
// Codex Deck - Implementation des chemins applicatifs locaux
// ----------------------------------------------------------------------------
// Ce fichier interroge le Shell Windows pour LocalAppData puis cree le dossier
// CodexDeck avant toute ouverture SQLite.
// ============================================================================

#include "AppDataPaths.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

// ----------------------------------------------------------------------------
// Construit le chemin de base depuis une racine LocalAppData injectee.
// ----------------------------------------------------------------------------
std::filesystem::path CodexDeckDatabasePath(const std::filesystem::path& local_app_data_root) {
    return local_app_data_root / L"CodexDeck" / L"codex-deck.db";
}

// ----------------------------------------------------------------------------
// Retourne le chemin de base Codex Deck dans LocalAppData.
// ----------------------------------------------------------------------------
std::expected<std::filesystem::path, StorageError> CodexDeckDatabasePath() {
    PWSTR raw_path = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw_path);
    if (FAILED(result)) {
        return std::unexpected(StorageError{StorageErrorCode::OpenFailed, static_cast<int>(result), "SHGetKnownFolderPath failed"});
    }

    std::filesystem::path local_app_data(raw_path);
    CoTaskMemFree(raw_path);
    const std::filesystem::path database_path = CodexDeckDatabasePath(local_app_data);

    std::error_code error;
    std::filesystem::create_directories(database_path.parent_path(), error);
    if (error) {
        return std::unexpected(StorageError{StorageErrorCode::OpenFailed, static_cast<int>(error.value()), error.message()});
    }
    return database_path;
}

// ============================================================================
// Codex Deck - Tests du stockage SQLite
// ----------------------------------------------------------------------------
// Ce fichier valide l'ouverture RAII de la base et la migration du schema local
// leger utilise par Codex Deck.
// ============================================================================

#include "storage/SchemaMigrator.h"
#include "storage/SqliteDatabase.h"

#include <sqlite3.h>
#include <windows.h>

#include <filesystem>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Retourne un chemin de base temporaire propre au processus.
//
// Retour :
// - chemin de fichier SQLite dans le dossier temporaire.
// ----------------------------------------------------------------------------
std::filesystem::path TestDatabasePath() {
    wchar_t temp_path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp_path);
    std::filesystem::path directory = std::filesystem::path(temp_path) / L"CodexDeckTests";
    std::filesystem::create_directories(directory);
    return directory / (L"storage-" + std::to_wstring(GetCurrentProcessId()) + L".db");
}

// ----------------------------------------------------------------------------
// Lit la version de schema depuis app_meta.
//
// Parametres :
// - database : base ouverte.
//
// Retour :
// - valeur schema_version ou chaine vide.
// ----------------------------------------------------------------------------
std::string ReadSchemaVersion(SqliteDatabase& database) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            database.handle(),
            "SELECT value FROM app_meta WHERE key='schema_version';",
            -1,
            &statement,
            nullptr
        ) != SQLITE_OK) {
        return {};
    }

    std::string value;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(statement, 0);
        if (text != nullptr) {
            value = reinterpret_cast<const char*>(text);
        }
    }
    sqlite3_finalize(statement);
    return value;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie qu'une base neuve migre vers le schema v1.
//
// Retour :
// - zero si la migration cree app_meta et schema_version=1.
// ----------------------------------------------------------------------------
int main() {
    const std::filesystem::path path = TestDatabasePath();
    std::filesystem::remove(path);

    std::string version;
    {
        auto database = OpenDatabase(path);
        if (!database) {
            return 1;
        }

        SchemaMigrator migrator;
        if (!migrator.Migrate(*database)) {
            return 2;
        }

        version = ReadSchemaVersion(*database);
    }

    std::filesystem::remove(path);
    std::filesystem::remove(path.wstring() + L"-wal");
    std::filesystem::remove(path.wstring() + L"-shm");
    return version == "1" ? 0 : 3;
}

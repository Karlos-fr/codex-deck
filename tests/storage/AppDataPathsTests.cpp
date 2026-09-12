// ============================================================================
// Codex Deck - Tests des chemins applicatifs
// ----------------------------------------------------------------------------
// Ce fichier valide la construction du chemin SQLite depuis LocalAppData sans
// dependre du profil Windows courant.
// ============================================================================

#include "storage/AppDataPaths.h"

// ----------------------------------------------------------------------------
// Verifie le chemin attendu de la base locale.
//
// Retour :
// - zero si le chemin CodexDeck est construit correctement.
// ----------------------------------------------------------------------------
int main() {
    const std::filesystem::path local_app_data = L"C:\\Users\\Test\\AppData\\Local";
    const std::filesystem::path expected = L"C:\\Users\\Test\\AppData\\Local\\CodexDeck\\codex-deck.db";
    return CodexDeckDatabasePath(local_app_data) == expected ? 0 : 1;
}

// ============================================================================
// Codex Deck - Tests du catalogue de sessions
// ----------------------------------------------------------------------------
// Ce fichier valide la publication de snapshots immuables avec revision monotone
// pour les futures vues UI.
// ============================================================================

#include "model/SessionCatalog.h"

// ----------------------------------------------------------------------------
// Verifie que chaque publication incremente la revision.
//
// Retour :
// - zero si les snapshots restent stables par valeur.
// ----------------------------------------------------------------------------
int main() {
    SessionCatalog catalog;
    SessionCatalogSnapshot first{};
    Project project{};
    project.id = 1;
    project.name = "SpotifyAmp";
    first.projects.push_back(project);

    auto first_snapshot = catalog.Publish(std::move(first));
    if (!first_snapshot || first_snapshot->revision != 1 || first_snapshot->projects.size() != 1) {
        return 1;
    }

    SessionCatalogSnapshot second = *first_snapshot;
    CodexThreadSummary thread{};
    thread.id = "thr_123";
    SessionRecord record{};
    record.codex = thread;
    second.sessions.push_back(record);
    auto second_snapshot = catalog.Publish(std::move(second));

    if (!second_snapshot || second_snapshot->revision != 2 || second_snapshot->sessions.size() != 1) {
        return 2;
    }
    if (first_snapshot->sessions.size() != 0) {
        return 3;
    }
    return 0;
}

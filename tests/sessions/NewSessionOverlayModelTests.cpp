// ============================================================================
// Codex Deck - Tests du modele nouvelle session
// ----------------------------------------------------------------------------
// Ce fichier valide les valeurs initiales et le suivi conditionnel du workspace.
// ============================================================================

#include "sessions/NewSessionOverlayModel.h"

// Verifie la selection initiale et le respect d'un workspace manuel.
int main() {
    Project spotify{};
    spotify.id = 1;
    spotify.name = "SpotifyAmp";
    spotify.roots.push_back(L"D:\\VibeCoding\\spotifyamp");
    Project deck{};
    deck.id = 2;
    deck.name = "Codex Deck";
    deck.roots.push_back(L"D:\\VibeCoding\\codex-deck");
    const std::vector<Project> projects{spotify, deck};

    auto model = BuildNewSessionOverlayModel(projects, spotify.id);
    if (model.project_id != spotify.id || model.workspace != spotify.roots.front() || model.model) {
        return 1;
    }
    SelectNewSessionProject(model, deck.id);
    if (model.workspace != deck.roots.front()) {
        return 2;
    }
    SetNewSessionWorkspace(model, L"D:\\Scratch");
    SelectNewSessionProject(model, spotify.id);
    if (model.workspace != L"D:\\Scratch") {
        return 3;
    }
    return 0;
}

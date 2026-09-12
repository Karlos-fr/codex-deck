// ============================================================================
// Codex Deck - Tests du modele de Command Palette
// ----------------------------------------------------------------------------
// Ce fichier valide la construction d'entrees de palette depuis un snapshot
// synthetique, sans app-server Codex.
// ============================================================================

#include "navigation/CommandPaletteModel.h"

namespace {

// ----------------------------------------------------------------------------
// Cree un projet synthetique.
//
// Parametres :
// - id : identifiant.
// - name : nom.
//
// Retour :
// - projet.
// ----------------------------------------------------------------------------
Project MakeProject(ProjectId id, std::string name) {
    Project project{};
    project.id = id;
    project.name = std::move(name);
    return project;
}

// ----------------------------------------------------------------------------
// Cree une session synthetique.
//
// Parametres :
// - id : identifiant.
// - title : titre.
// - project_id : projet associe.
//
// Retour :
// - session.
// ----------------------------------------------------------------------------
SessionRecord MakeSession(std::string id, std::string title, std::optional<ProjectId> project_id) {
    SessionRecord session{};
    session.codex.id = std::move(id);
    session.codex.name = std::move(title);
    session.project_id = project_id;
    return session;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie la construction et le tri des entrees.
//
// Retour :
// - zero si les entrees sessions/projets/actions sont utilisables.
// ----------------------------------------------------------------------------
int main() {
    SessionCatalogSnapshot snapshot{};
    snapshot.projects.push_back(MakeProject(1, "SpotifyAmp"));
    snapshot.projects.push_back(MakeProject(2, "OpenRemi"));
    snapshot.sessions.push_back(MakeSession("thr_audio", "Audio parity", 1));
    snapshot.sessions.push_back(MakeSession("thr_notes", "Audio notes", 2));

    const auto entries = BuildCommandPaletteEntries(snapshot, L"spa aud");
    if (entries.empty() || entries.size() > 50) {
        return 1;
    }
    if (entries.front().kind != PaletteEntryKind::Session || entries.front().command.kind != DeckCommandKind::OpenThread) {
        return 2;
    }
    if (entries.front().command.thread_id != "thr_audio") {
        return 3;
    }

    const auto actions = BuildCommandPaletteEntries(snapshot, L"new session");
    if (actions.empty() || actions.front().kind != PaletteEntryKind::Action) {
        return 4;
    }
    return actions.front().command.kind == DeckCommandKind::NewSession ? 0 : 5;
}

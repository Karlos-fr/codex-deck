// ============================================================================
// Codex Deck - Tests du tri d'activite
// ----------------------------------------------------------------------------
// Ce fichier valide l'ordre modele des projets et sessions sans dependance UI.
// ============================================================================

#include "model/ActivityOrdering.h"

namespace {

// ----------------------------------------------------------------------------
// Cree une session synthetique.
//
// Parametres :
// - id : identifiant Codex.
// - project_id : projet associe ou Unassigned.
// - activity : derniere activite.
//
// Retour :
// - record de session.
// ----------------------------------------------------------------------------
SessionRecord Session(std::string id, std::optional<ProjectId> project_id, std::int64_t activity) {
    SessionRecord record{};
    record.codex.id = std::move(id);
    record.codex.updated_at = activity;
    record.project_id = project_id;
    return record;
}

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
Project ProjectOf(ProjectId id, std::string name) {
    Project project{};
    project.id = id;
    project.name = std::move(name);
    return project;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie l'ordre par activite significative.
//
// Retour :
// - zero si la vue ordonnee respecte le contrat.
// ----------------------------------------------------------------------------
int main() {
    SessionCatalogSnapshot snapshot{};
    snapshot.projects.push_back(ProjectOf(1, "Old"));
    snapshot.projects.push_back(ProjectOf(2, "Hot"));
    snapshot.sessions.push_back(Session("old-a", 1, 10));
    snapshot.sessions.push_back(Session("hot-a", 2, 50));
    snapshot.sessions.push_back(Session("hot-b", 2, 40));
    snapshot.sessions.push_back(Session("none", std::nullopt, 60));
    snapshot.sessions.push_back(Session("old-b", 1, 30));

    const OrderedCatalogView ordered = SortProjectsAndSessions(snapshot);
    if (ordered.projects.size() != 2 || ordered.unassigned.size() != 1) {
        return 1;
    }
    if (ordered.projects[0].project.id != 2 || ordered.projects[1].project.id != 1) {
        return 2;
    }
    if (ordered.projects[0].sessions[0].codex.id != "hot-a"
        || ordered.projects[0].sessions[1].codex.id != "hot-b") {
        return 3;
    }
    if (ordered.projects[1].sessions[0].codex.id != "old-b"
        || ordered.projects[1].sessions[1].codex.id != "old-a") {
        return 4;
    }
    if (ordered.unassigned.front().codex.id != "none") {
        return 5;
    }
    if (!ShouldUpdateLastActivity(ActivitySignal::PromptSent)
        || !ShouldUpdateLastActivity(ActivitySignal::ExternalSyncNewerUpdatedAt)) {
        return 6;
    }
    return ShouldUpdateLastActivity(ActivitySignal::StreamingDelta)
        || ShouldUpdateLastActivity(ActivitySignal::StdoutLine)
        || ShouldUpdateLastActivity(ActivitySignal::UiRepaint)
        ? 7
        : 0;
}

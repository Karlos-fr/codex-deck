// ============================================================================
// Codex Deck - Tests du modele d'arbre projets/sessions
// ----------------------------------------------------------------------------
// Ce fichier valide la construction de lignes stables sans rendu ni fenetre.
// ============================================================================

#include "navigation/ProjectTreeModel.h"

#include <string>

namespace {

// ----------------------------------------------------------------------------
// Cree un projet synthetique.
//
// Parametres :
// - id : identifiant.
// - name : nom affiche.
//
// Retour :
// - projet de test.
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
// - id : identifiant du thread.
// - title : titre.
// - project_id : projet associe ou Unassigned.
// - updated_at : activite.
//
// Retour :
// - session de test.
// ----------------------------------------------------------------------------
SessionRecord MakeSession(
    std::string id,
    std::string title,
    std::optional<ProjectId> project_id,
    std::int64_t updated_at
) {
    SessionRecord session{};
    session.codex.id = std::move(id);
    session.codex.name = std::move(title);
    session.codex.updated_at = updated_at;
    session.project_id = project_id;
    return session;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie structure, tri et limitation initiale par projet.
//
// Retour :
// - zero si les lignes respectent le contrat du plan.
// ----------------------------------------------------------------------------
int main() {
    SessionCatalogSnapshot catalog{};
    catalog.projects.push_back(MakeProject(1, "Older"));
    catalog.projects.push_back(MakeProject(2, "Recent"));
    for (int index = 0; index < 10; ++index) {
        catalog.sessions.push_back(MakeSession(
            "p2-" + std::to_string(index),
            "Recent " + std::to_string(index),
            2,
            100 - index
        ));
    }
    catalog.sessions.push_back(MakeSession("p1-only", "Older only", 1, 20));
    catalog.sessions.push_back(MakeSession("u-new", "Loose new", std::nullopt, 200));
    catalog.sessions.push_back(MakeSession("u-old", "Loose old", std::nullopt, 10));

    ProjectTreeState state{};
    state.expanded_projects.insert(1);
    state.expanded_projects.insert(2);

    const auto rows = BuildProjectTreeRows(catalog, state);
    if (rows.size() != 15) {
        return 1;
    }
    if (rows[0].kind != TreeRowKind::Project || rows[0].project_id != 2 || rows[0].stable_id != "project:2") {
        return 2;
    }
    if (rows[1].kind != TreeRowKind::Session || rows[1].thread_id != "p2-0" || rows[1].stable_id != "thread:p2-0") {
        return 3;
    }
    if (rows[8].kind != TreeRowKind::Session || rows[8].thread_id != "p2-7") {
        return 4;
    }
    if (rows[9].kind != TreeRowKind::More || rows[9].hidden_count != 2 || rows[9].stable_id != "more:2") {
        return 5;
    }
    if (rows[10].kind != TreeRowKind::Project || rows[10].project_id != 1) {
        return 6;
    }
    if (rows[12].kind != TreeRowKind::UnassignedHeader || rows[12].stable_id != "unassigned") {
        return 7;
    }
    if (rows[13].kind != TreeRowKind::Session || rows[13].thread_id != "u-new") {
        return 8;
    }
    return rows[14].kind == TreeRowKind::Session && rows[14].thread_id == "u-old" ? 0 : 9;
}

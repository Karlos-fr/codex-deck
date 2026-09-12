// ============================================================================
// Codex Deck - Implementation du modele aplati du Tree
// ----------------------------------------------------------------------------
// Ce fichier convertit la vue ordonnee du modele en lignes compactes pour le
// rendu et l'interaction, sans creer de dependance UI.
// ============================================================================

#include "ProjectTreeModel.h"

#include "../model/ActivityOrdering.h"

#include <algorithm>

namespace {

// ----------------------------------------------------------------------------
// Convertit une chaine UTF-8 ASCII-compatible en texte large.
//
// Parametres :
// - text : texte source.
//
// Retour :
// - texte large utilisable par DirectWrite.
// ----------------------------------------------------------------------------
std::wstring WidenAscii(std::string_view text) {
    std::wstring wide;
    wide.reserve(text.size());
    for (const char character : text) {
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    }
    return wide;
}

// ----------------------------------------------------------------------------
// Cree une ligne projet.
//
// Parametres :
// - project : projet source.
//
// Retour :
// - ligne d'arbre projet.
// ----------------------------------------------------------------------------
TreeRow ProjectRow(const Project& project) {
    TreeRow row{};
    row.kind = TreeRowKind::Project;
    row.stable_id = "project:" + std::to_string(project.id);
    row.primary_text = WidenAscii(project.name);
    row.project_id = project.id;
    return row;
}

// ----------------------------------------------------------------------------
// Cree une ligne session.
//
// Parametres :
// - session : session source.
// - depth : profondeur d'affichage.
//
// Retour :
// - ligne d'arbre session.
// ----------------------------------------------------------------------------
TreeRow SessionRow(const SessionRecord& session, int depth) {
    TreeRow row{};
    row.kind = TreeRowKind::Session;
    row.stable_id = "thread:" + session.codex.id;
    row.depth = depth;
    row.primary_text = WidenAscii(session.codex.name.empty() ? session.codex.id : session.codex.name);
    row.secondary_text = WidenAscii(session.codex.cwd.string());
    row.status = session.status;
    row.project_id = session.project_id;
    row.thread_id = session.codex.id;
    return row;
}

// ----------------------------------------------------------------------------
// Cree une ligne More pour un projet.
//
// Parametres :
// - project_id : projet concerne.
// - hidden_count : nombre de sessions masquees.
//
// Retour :
// - ligne d'arbre More.
// ----------------------------------------------------------------------------
TreeRow MoreRow(ProjectId project_id, std::size_t hidden_count) {
    TreeRow row{};
    row.kind = TreeRowKind::More;
    row.stable_id = "more:" + std::to_string(project_id);
    row.depth = 1;
    row.primary_text = L"... " + std::to_wstring(hidden_count) + L" more";
    row.project_id = project_id;
    row.hidden_count = hidden_count;
    return row;
}

// ----------------------------------------------------------------------------
// Cree l'en-tete fixe Unassigned.
//
// Retour :
// - ligne d'en-tete.
// ----------------------------------------------------------------------------
TreeRow UnassignedHeaderRow() {
    TreeRow row{};
    row.kind = TreeRowKind::UnassignedHeader;
    row.stable_id = "unassigned";
    row.primary_text = L"Unassigned";
    return row;
}

}  // namespace

// ----------------------------------------------------------------------------
// Construit les lignes aplaties de navigation.
// ----------------------------------------------------------------------------
std::vector<TreeRow> BuildProjectTreeRows(
    const SessionCatalogSnapshot& catalog,
    const ProjectTreeState& state,
    std::size_t max_sessions_per_project
) {
    const OrderedCatalogView ordered = SortProjectsAndSessions(catalog);
    std::vector<TreeRow> rows;
    rows.reserve(catalog.projects.size() + catalog.sessions.size() + 2);

    for (const OrderedProjectSessions& project_group : ordered.projects) {
        rows.push_back(ProjectRow(project_group.project));
        if (!state.expanded_projects.contains(project_group.project.id)) {
            continue;
        }

        const std::size_t visible_count = std::min(max_sessions_per_project, project_group.sessions.size());
        for (std::size_t index = 0; index < visible_count; ++index) {
            rows.push_back(SessionRow(project_group.sessions[index], 1));
        }
        if (project_group.sessions.size() > visible_count) {
            rows.push_back(MoreRow(project_group.project.id, project_group.sessions.size() - visible_count));
        }
    }

    rows.push_back(UnassignedHeaderRow());
    if (state.unassigned_expanded) {
        for (const SessionRecord& session : ordered.unassigned) {
            rows.push_back(SessionRow(session, 1));
        }
    }
    return rows;
}

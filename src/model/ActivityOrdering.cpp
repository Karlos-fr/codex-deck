// ============================================================================
// Codex Deck - Implementation du tri d'activite
// ----------------------------------------------------------------------------
// Ce fichier groupe les sessions par projet puis ordonne chaque niveau selon la
// derniere activite significative deja presente dans le modele.
// ============================================================================

#include "ActivityOrdering.h"

#include <algorithm>
#include <map>

namespace {

// ----------------------------------------------------------------------------
// Compare deux sessions par activite decroissante.
//
// Parametres :
// - left : session gauche.
// - right : session droite.
//
// Retour :
// - true si left doit preceder right.
// ----------------------------------------------------------------------------
bool MoreRecentSession(const SessionRecord& left, const SessionRecord& right) {
    if (left.codex.updated_at != right.codex.updated_at) {
        return left.codex.updated_at > right.codex.updated_at;
    }
    return left.codex.id < right.codex.id;
}

// ----------------------------------------------------------------------------
// Retourne l'activite recente d'un groupe.
//
// Parametres :
// - group : groupe projet.
//
// Retour :
// - timestamp de la session la plus recente, ou zero.
// ----------------------------------------------------------------------------
std::int64_t MostRecentActivity(const OrderedProjectSessions& group) {
    return group.sessions.empty() ? 0 : group.sessions.front().codex.updated_at;
}

}  // namespace

// ----------------------------------------------------------------------------
// Trie projets et sessions par activite significative.
// ----------------------------------------------------------------------------
OrderedCatalogView SortProjectsAndSessions(const SessionCatalogSnapshot& snapshot) {
    std::map<ProjectId, OrderedProjectSessions> groups;
    for (const Project& project : snapshot.projects) {
        groups.emplace(project.id, OrderedProjectSessions{project, {}});
    }

    OrderedCatalogView view{};
    for (const SessionRecord& session : snapshot.sessions) {
        if (!session.project_id) {
            view.unassigned.push_back(session);
            continue;
        }
        if (auto found = groups.find(*session.project_id); found != groups.end()) {
            found->second.sessions.push_back(session);
        }
    }

    std::ranges::sort(view.unassigned, MoreRecentSession);
    for (auto& [project_id, group] : groups) {
        (void)project_id;
        if (group.sessions.empty()) {
            continue;
        }
        std::ranges::sort(group.sessions, MoreRecentSession);
        view.projects.push_back(std::move(group));
    }
    std::ranges::sort(view.projects, [](const OrderedProjectSessions& left, const OrderedProjectSessions& right) {
        const std::int64_t left_activity = MostRecentActivity(left);
        const std::int64_t right_activity = MostRecentActivity(right);
        if (left_activity != right_activity) {
            return left_activity > right_activity;
        }
        return left.project.id < right.project.id;
    });
    return view;
}

// ----------------------------------------------------------------------------
// Indique si un evenement doit modifier last_activity.
// ----------------------------------------------------------------------------
bool ShouldUpdateLastActivity(ActivitySignal signal) {
    switch (signal) {
    case ActivitySignal::ThreadCreatedOrResumed:
    case ActivitySignal::PromptSent:
    case ActivitySignal::TurnCompleted:
    case ActivitySignal::ApprovalReceived:
    case ActivitySignal::ExternalSyncNewerUpdatedAt:
        return true;
    case ActivitySignal::StreamingDelta:
    case ActivitySignal::StdoutLine:
    case ActivitySignal::UiRepaint:
    default:
        return false;
    }
}

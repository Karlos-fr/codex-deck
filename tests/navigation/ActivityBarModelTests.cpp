// ============================================================================
// Codex Deck - Tests du modele de barre d'activite
// ----------------------------------------------------------------------------
// Ce fichier valide les compteurs et filtres globaux a partir de sessions
// synthetiques, sans compte Codex ni app-server.
// ============================================================================

#include "navigation/ActivityBarModel.h"

#include <chrono>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Retourne un timestamp proche de maintenant.
//
// Retour :
// - millisecondes depuis epoch systeme.
// ----------------------------------------------------------------------------
std::int64_t NowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

// ----------------------------------------------------------------------------
// Cree une session de test.
//
// Parametres :
// - id : identifiant.
// - status : statut.
// - updated_at : timestamp.
// - favorite : favori local.
// - archived : archive Codex.
//
// Retour :
// - record de session.
// ----------------------------------------------------------------------------
SessionRecord MakeSession(
    std::string id,
    SessionStatus status,
    std::int64_t updated_at,
    bool favorite = false,
    bool archived = false
) {
    SessionRecord session{};
    session.codex.id = std::move(id);
    session.codex.name = session.codex.id;
    session.codex.updated_at = updated_at;
    session.codex.archived = archived;
    session.status = status;
    session.favorite = favorite;
    return session;
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie les compteurs d'activite.
//
// Retour :
// - zero si les compteurs attendus sont exacts.
// ----------------------------------------------------------------------------
int TestCounts() {
    const std::int64_t now = NowMillis();
    const std::int64_t old = now - 48LL * 60LL * 60LL * 1000LL;
    const std::vector<SessionRecord> sessions{
        MakeSession("working-a", SessionStatus::Working, now),
        MakeSession("working-b", SessionStatus::Working, now),
        MakeSession("attention", SessionStatus::NeedsAttention, now),
        MakeSession("completed-today", SessionStatus::Completed, now, true),
        MakeSession("completed-old", SessionStatus::Completed, old),
        MakeSession("archived", SessionStatus::Idle, now, false, true),
    };

    const ActivityCounts counts = BuildActivityCounts(sessions);
    if (counts.working != 2 || counts.needs_attention != 1 || counts.completed_today != 1) {
        return 1;
    }
    if (counts.favorites != 1 || counts.archived != 1 || counts.total != 6) {
        return 2;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Verifie que les filtres ne modifient pas les sessions sources.
//
// Retour :
// - zero si les filtres retournent les sessions attendues.
// ----------------------------------------------------------------------------
int TestFilters() {
    const std::int64_t now = NowMillis();
    const std::vector<SessionRecord> sessions{
        MakeSession("working", SessionStatus::Working, now),
        MakeSession("attention", SessionStatus::NeedsAttention, now),
        MakeSession("completed", SessionStatus::Completed, now),
        MakeSession("favorite", SessionStatus::Idle, now, true),
        MakeSession("archived", SessionStatus::Idle, now, false, true),
        MakeSession("idle", SessionStatus::Idle, now),
    };

    if (FilterSessions(sessions, SessionFilter::Working).size() != 1) {
        return 3;
    }
    if (FilterSessions(sessions, SessionFilter::NeedsAttention).front().codex.id != "attention") {
        return 4;
    }
    if (FilterSessions(sessions, SessionFilter::Favorites).front().codex.id != "favorite") {
        return 5;
    }
    if (FilterSessions(sessions, SessionFilter::Archived).front().codex.id != "archived") {
        return 6;
    }
    return FilterSessions(sessions, SessionFilter::All).size() == sessions.size() ? 0 : 7;
}

// ----------------------------------------------------------------------------
// Execute les tests de barre d'activite.
//
// Retour :
// - zero si tous les scenarios passent.
// ----------------------------------------------------------------------------
int main() {
    if (const int result = TestCounts(); result != 0) {
        return result;
    }
    if (const int result = TestFilters(); result != 0) {
        return result;
    }
    return 0;
}

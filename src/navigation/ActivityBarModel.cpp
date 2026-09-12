// ============================================================================
// Codex Deck - Implementation du modele de barre d'activite
// ----------------------------------------------------------------------------
// Ce fichier derive des compteurs legers depuis les records de session, en
// s'appuyant sur Codex pour l'etat archive et sur le cache local pour favoris.
// ============================================================================

#include "ActivityBarModel.h"

#include <chrono>

namespace {

// Nombre de millisecondes dans un jour civil approximatif.
constexpr std::int64_t kMillisPerDay = 24LL * 60LL * 60LL * 1000LL;

// ----------------------------------------------------------------------------
// Retourne le timestamp courant.
//
// Retour :
// - millisecondes depuis epoch systeme.
// ----------------------------------------------------------------------------
std::int64_t NowMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

// ----------------------------------------------------------------------------
// Indique si un timestamp appartient a la fenetre recente du jour courant.
//
// Parametres :
// - timestamp : timestamp a tester.
//
// Retour :
// - true si le timestamp est dans les dernieres 24 heures.
// ----------------------------------------------------------------------------
bool IsCompletedToday(std::int64_t timestamp) {
    const std::int64_t now = NowMillis();
    return timestamp >= 0 && timestamp <= now && now - timestamp < kMillisPerDay;
}

// ----------------------------------------------------------------------------
// Indique si une session correspond a un filtre.
//
// Parametres :
// - session : session source.
// - filter : filtre demande.
//
// Retour :
// - true si la session doit etre conservee.
// ----------------------------------------------------------------------------
bool MatchesFilter(const SessionRecord& session, SessionFilter filter) {
    switch (filter) {
    case SessionFilter::Working:
        return session.status == SessionStatus::Working;
    case SessionFilter::NeedsAttention:
        return session.status == SessionStatus::NeedsAttention;
    case SessionFilter::CompletedToday:
        return session.status == SessionStatus::Completed && IsCompletedToday(session.codex.updated_at);
    case SessionFilter::Favorites:
        return session.favorite;
    case SessionFilter::Archived:
        return session.codex.archived;
    case SessionFilter::All:
    default:
        return true;
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// Calcule les compteurs globaux.
// ----------------------------------------------------------------------------
ActivityCounts BuildActivityCounts(std::span<const SessionRecord> sessions) {
    ActivityCounts counts{};
    counts.total = sessions.size();
    for (const SessionRecord& session : sessions) {
        if (session.status == SessionStatus::Working) {
            ++counts.working;
        }
        if (session.status == SessionStatus::NeedsAttention) {
            ++counts.needs_attention;
        }
        if (session.status == SessionStatus::Completed && IsCompletedToday(session.codex.updated_at)) {
            ++counts.completed_today;
        }
        if (session.favorite) {
            ++counts.favorites;
        }
        if (session.codex.archived) {
            ++counts.archived;
        }
    }
    return counts;
}

// ----------------------------------------------------------------------------
// Filtre les sessions selon un filtre global.
// ----------------------------------------------------------------------------
std::vector<SessionRecord> FilterSessions(std::span<const SessionRecord> sessions, SessionFilter filter) {
    std::vector<SessionRecord> filtered;
    filtered.reserve(sessions.size());
    for (const SessionRecord& session : sessions) {
        if (MatchesFilter(session, filter)) {
            filtered.push_back(session);
        }
    }
    return filtered;
}

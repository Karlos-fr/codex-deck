// ============================================================================
// Codex Deck - Modele de barre d'activite
// ----------------------------------------------------------------------------
// Ce module calcule les compteurs et filtres globaux a partir des sessions
// fusionnees, sans modifier le catalogue source.
// ============================================================================

#pragma once

#include "../model/SessionRecord.h"

#include <cstddef>
#include <span>
#include <vector>

// ----------------------------------------------------------------------------
// Filtre global applicable au Tree.
// ----------------------------------------------------------------------------
enum class SessionFilter {
    // Toutes les sessions actives.
    All,

    // Sessions en cours de travail.
    Working,

    // Sessions attendant une action.
    NeedsAttention,

    // Sessions terminees aujourd'hui.
    CompletedToday,

    // Sessions favorites locales.
    Favorites,

    // Sessions archivees cote Codex.
    Archived,
};

// ----------------------------------------------------------------------------
// Compteurs globaux d'activite.
// ----------------------------------------------------------------------------
struct ActivityCounts {
    // Nombre total de sessions.
    std::size_t total = 0;

    // Sessions en cours.
    std::size_t working = 0;

    // Sessions attendant une attention.
    std::size_t needs_attention = 0;

    // Sessions terminees aujourd'hui.
    std::size_t completed_today = 0;

    // Sessions favorites.
    std::size_t favorites = 0;

    // Sessions archivees.
    std::size_t archived = 0;
};

// ----------------------------------------------------------------------------
// Calcule les compteurs globaux.
//
// Parametres :
// - sessions : sessions sources.
//
// Retour :
// - compteurs agregees.
// ----------------------------------------------------------------------------
ActivityCounts BuildActivityCounts(std::span<const SessionRecord> sessions);

// ----------------------------------------------------------------------------
// Filtre les sessions selon un filtre global.
//
// Parametres :
// - sessions : sessions sources.
// - filter : filtre demande.
//
// Retour :
// - copie des sessions correspondant au filtre.
// ----------------------------------------------------------------------------
std::vector<SessionRecord> FilterSessions(std::span<const SessionRecord> sessions, SessionFilter filter);

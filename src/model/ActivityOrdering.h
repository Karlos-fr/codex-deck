// ============================================================================
// Codex Deck - Tri d'activite du catalogue
// ----------------------------------------------------------------------------
// Ce module construit une vue ordonnee a partir d'un snapshot immutable sans
// modifier le catalogue source ni connaitre les contraintes d'affichage UI.
// ============================================================================

#pragma once

#include "SessionCatalog.h"

#include <vector>

// ----------------------------------------------------------------------------
// Type d'evenement susceptible d'influencer l'activite d'une session.
// ----------------------------------------------------------------------------
enum class ActivitySignal {
    // Creation ou reprise explicite d'un thread.
    ThreadCreatedOrResumed,

    // Prompt envoye par l'utilisateur.
    PromptSent,

    // Tour Codex termine.
    TurnCompleted,

    // Approbation recue par Codex.
    ApprovalReceived,

    // Synchronisation externe avec updatedAt plus recent.
    ExternalSyncNewerUpdatedAt,

    // Delta de streaming sans signification de tri.
    StreamingDelta,

    // Ligne stdout sans signification de tri.
    StdoutLine,

    // Repaint UI sans changement modele.
    UiRepaint,
};

// ----------------------------------------------------------------------------
// Groupe ordonne des sessions d'un projet.
// ----------------------------------------------------------------------------
struct OrderedProjectSessions {
    // Projet local associe au groupe.
    Project project;

    // Sessions du projet triees par activite decroissante.
    std::vector<SessionRecord> sessions;
};

// ----------------------------------------------------------------------------
// Vue modele ordonnee du catalogue.
// ----------------------------------------------------------------------------
struct OrderedCatalogView {
    // Projets ayant des sessions, tries par activite recente.
    std::vector<OrderedProjectSessions> projects;

    // Sessions sans projet, destinees a la zone Unassigned fixe.
    std::vector<SessionRecord> unassigned;
};

// ----------------------------------------------------------------------------
// Trie projets et sessions par activite significative.
//
// Parametres :
// - snapshot : catalogue source immutable.
//
// Retour :
// - vue ordonnee sans mutation du snapshot source.
// ----------------------------------------------------------------------------
OrderedCatalogView SortProjectsAndSessions(const SessionCatalogSnapshot& snapshot);

// ----------------------------------------------------------------------------
// Indique si un evenement doit modifier last_activity.
//
// Parametres :
// - signal : evenement observe.
//
// Retour :
// - true seulement pour les evenements significatifs du contrat.
// ----------------------------------------------------------------------------
bool ShouldUpdateLastActivity(ActivitySignal signal);

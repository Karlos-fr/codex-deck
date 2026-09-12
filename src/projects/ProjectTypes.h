// ============================================================================
// Codex Deck - Types projets et sessions locales
// ----------------------------------------------------------------------------
// Ce module definit les objets persistables propres a Codex Deck. Il ne contient
// aucun historique complet de conversation Codex.
// ============================================================================

#pragma once

#include "../codex/CodexTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Identifiant interne d'un projet Codex Deck.
using ProjectId = std::int64_t;

// ----------------------------------------------------------------------------
// Source de l'association entre une session Codex et un projet Deck.
// ----------------------------------------------------------------------------
enum class AssignmentSource : int {
    // Association deduite automatiquement.
    Automatic = 0,

    // Association choisie explicitement par l'utilisateur.
    Manual = 1,
};

// ----------------------------------------------------------------------------
// Projet logique local.
// ----------------------------------------------------------------------------
struct Project {
    // Identifiant SQLite du projet.
    ProjectId id = 0;

    // Nom affiche du projet.
    std::string name;

    // Racines de workspace associees.
    std::vector<std::filesystem::path> roots;

    // Remote Git optionnel detecte ou saisi.
    std::optional<std::string> git_remote;

    // Timestamp de creation local.
    std::int64_t created_at = 0;

    // Timestamp de derniere modification locale.
    std::int64_t updated_at = 0;
};

// ----------------------------------------------------------------------------
// Metadonnees locales legeres d'une session Codex.
// ----------------------------------------------------------------------------
struct SessionMetadata {
    // Identifiant Codex du thread.
    CodexThreadId thread_id;

    // Projet associe, absent pour Unassigned.
    std::optional<ProjectId> project_id;

    // Source de l'association.
    AssignmentSource assignment_source = AssignmentSource::Automatic;

    // Marqueur favori local.
    bool favorite = false;

    // Titre de cache affiche au demarrage.
    std::string cached_title;

    // Repertoire de travail de cache.
    std::filesystem::path cached_cwd;

    // Derniere activite connue.
    std::int64_t last_activity = 0;

    // Dernier statut connu.
    SessionStatus last_known_status = SessionStatus::Idle;
};

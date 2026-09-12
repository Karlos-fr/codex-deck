// ============================================================================
// Codex Deck - Runtime de session
// ----------------------------------------------------------------------------
// Ce module definit l'etat volatile d'une session Codex. Ces donnees restent en
// memoire et ne sont pas persistees dans SQLite.
// ============================================================================

#pragma once

#include "../codex/CodexTypes.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Demande d'approbation en attente pour une session.
// ----------------------------------------------------------------------------
struct PendingApproval {
    // Identifiant JSON-RPC a reutiliser pour repondre.
    nlohmann::json request_id;

    // Thread concerne par la demande.
    CodexThreadId thread_id;

    // Tour concerne, lorsqu'il est fourni par Codex.
    std::optional<CodexTurnId> turn_id;

    // Methode app-server d'origine.
    std::string method;

    // Titre court a afficher.
    std::string title;

    // Detail lisible de la demande.
    std::string detail;

    // Decisions wire annoncees par Codex.
    std::vector<std::string> available_decisions;
};

// ----------------------------------------------------------------------------
// Etat runtime partage d'une session.
// ----------------------------------------------------------------------------
struct SessionRuntime {
    // Thread suivi.
    CodexThreadId thread_id;

    // Etat courant partage.
    SessionStatus status = SessionStatus::Idle;

    // Tour courant si un travail est actif.
    std::optional<CodexTurnId> current_turn;

    // Approbation en attente, s'il y en a une.
    std::optional<PendingApproval> pending_approval;

    // Timestamp local de derniere activite.
    std::int64_t latest_activity = 0;

    // Dernier message d'erreur lisible.
    std::string last_error;
};

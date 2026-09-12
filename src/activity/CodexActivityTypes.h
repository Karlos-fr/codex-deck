// ============================================================================
// Codex Glass - Types du suivi d'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier decrit uniquement les metadonnees techniques exposees par le
// moniteur local. Il ne contient aucun contenu de conversation ou d'outil.
// ============================================================================

#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Etat technique courant d'une session Codex locale.
// ----------------------------------------------------------------------------
enum class CodexActivityState {
    Unavailable,
    Idle,
    Thinking,
    ToolRunning,
    WaitingForUser,
    Completed,
    Aborted,
    Error,
};

// ----------------------------------------------------------------------------
// Resume non sensible d'une session encore pertinente pour l'interface.
// ----------------------------------------------------------------------------
struct CodexSessionActivitySnapshot {
    // Identifiant technique stable conserve uniquement en memoire.
    std::wstring session_key;

    // Identifiant du tour courant lorsqu'il est fourni par Codex.
    std::wstring turn_id;

    // Etat technique deduit des marqueurs JSONL.
    CodexActivityState state = CodexActivityState::Thinking;

    // Instant du dernier marqueur technique exploite.
    std::chrono::system_clock::time_point last_event_at{};

    // Nombre d'appels d'outil sans sortie correlee.
    std::size_t open_tool_count = 0;

    // Indique qu'au moins un outil attend explicitement l'utilisateur.
    bool waiting_for_user = false;

    // Indique que le tour a ete deduit sans marqueur task_started.
    bool inferred = false;
};

// ----------------------------------------------------------------------------
// Snapshot multisession publie vers le thread UI.
// ----------------------------------------------------------------------------
struct CodexActivitySnapshot {
    // Sessions actives ou terminees depuis peu.
    std::vector<CodexSessionActivitySnapshot> sessions;

    // Instant auquel le moniteur a produit ce snapshot.
    std::chrono::system_clock::time_point sampled_at{};

    // Indique qu'au moins un repertoire de sessions est accessible.
    bool monitor_available = false;
};

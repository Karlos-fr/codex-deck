// ============================================================================
// Codex Deck - Types de creation de session
// ----------------------------------------------------------------------------
// Ce module definit les requetes et resultats stables du flux de creation sans
// dependre du rendu ou du transport JSON-RPC.
// ============================================================================

#pragma once

#include "../codex/CodexError.h"
#include "../codex/CodexTypes.h"
#include "../projects/ProjectTypes.h"
#include "../storage/StorageError.h"

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>

// Requete de creation d'une session Codex.
struct CreateSessionRequest {
    // Projet explicite, ou association automatique.
    std::optional<ProjectId> project_id;
    // Repertoire de travail du nouveau thread.
    std::filesystem::path cwd;
    // Override modele optionnel.
    std::optional<std::string> model;
    // Premier prompt optionnel a lancer apres creation.
    std::optional<std::string> initial_prompt;
};

// Session creee et publiee dans le catalogue.
struct CreatedSession {
    // Resume retourne par app-server.
    CodexThreadSummary thread;
    // Projet explicitement associe.
    std::optional<ProjectId> project_id;
    // Erreur du prompt initial, sans invalidation du thread cree.
    std::optional<CodexError> initial_prompt_error;
};

// Erreur structuree du flux de creation.
struct SessionCreationError {
    // Erreur Codex eventuelle.
    std::optional<CodexError> codex;
    // Erreur de persistance eventuelle.
    std::optional<StorageError> storage;
};

// Completion asynchrone du flux de creation.
using SessionCreationCompletion = std::move_only_function<void(std::expected<CreatedSession, SessionCreationError>)>;

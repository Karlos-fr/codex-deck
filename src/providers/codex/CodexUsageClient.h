// ============================================================================
// Codex Glass - Client HTTP d'usage Codex
// ----------------------------------------------------------------------------
// Ce fichier declare le transport WinHTTP de l'endpoint Codex. Il ne parse pas
// la reponse et ne connait aucun modele de rendu.
// ============================================================================

#pragma once

#include "CodexAuthReader.h"

#include <atomic>
#include <optional>
#include <string>

// ----------------------------------------------------------------------------
// Realise et annule l'appel distant d'usage Codex.
// ----------------------------------------------------------------------------
class CodexUsageClient {
public:
    // ------------------------------------------------------------------------
    // Recupere le corps JSON brut de l'endpoint d'usage.
    // ------------------------------------------------------------------------
    std::optional<std::string> Fetch(
        const CodexAuthCredentials& credentials,
        std::wstring* error_message
    );

    // ------------------------------------------------------------------------
    // Annule la requete WinHTTP active.
    // ------------------------------------------------------------------------
    void Cancel();

private:
    // Indique qu'une annulation est demandee.
    std::atomic_bool cancellation_requested_ = false;

    // Handle WinHTTP actif conserve sans exposer WinHTTP dans le header.
    std::atomic<void*> active_request_ = nullptr;
};

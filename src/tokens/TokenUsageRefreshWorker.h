// ============================================================================
// Codex Glass - Worker de rafraichissement des tokens locaux
// ----------------------------------------------------------------------------
// Ce fichier declare le scan JSONL et son cache hors thread UI. Le resultat est
// restitue uniquement par message Win32.
// ============================================================================

#pragma once

#include "TokenUsageTypes.h"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

// Message prive signalant la fin d'un scan local de tokens.
constexpr UINT kTokenUsageRefreshCompletedMessage = WM_APP + 21;

// ----------------------------------------------------------------------------
// Possede le thread de scan local et son dernier resultat.
// ----------------------------------------------------------------------------
class TokenUsageRefreshWorker {
public:
    // ------------------------------------------------------------------------
    // Cree un worker utilisant le profil Codex et la base de l'application.
    // ------------------------------------------------------------------------
    TokenUsageRefreshWorker() = default;

    // ------------------------------------------------------------------------
    // Cree un worker injectable pour les tests.
    // ------------------------------------------------------------------------
    TokenUsageRefreshWorker(std::filesystem::path codex_home, std::wstring database_path);

    // ------------------------------------------------------------------------
    // Annule puis joint automatiquement tout scan actif.
    // ------------------------------------------------------------------------
    ~TokenUsageRefreshWorker();

    TokenUsageRefreshWorker(const TokenUsageRefreshWorker&) = delete;
    TokenUsageRefreshWorker& operator=(const TokenUsageRefreshWorker&) = delete;

    // ------------------------------------------------------------------------
    // Lance un scan de trente jours si aucun travail n'est actif.
    // ------------------------------------------------------------------------
    bool Start(HWND hwnd);

    // ------------------------------------------------------------------------
    // Indique si un scan est actuellement possede par le worker.
    // Retour : true tant que le thread doit encore etre joint.
    // ------------------------------------------------------------------------
    bool IsActive() const;

    // ------------------------------------------------------------------------
    // Joint le thread termine et transfere son resultat.
    // ------------------------------------------------------------------------
    std::optional<TokenUsageSnapshot> TakeResult();

    // ------------------------------------------------------------------------
    // Demande l'annulation et joint le thread actif.
    // ------------------------------------------------------------------------
    void Stop();

private:
    // Profil explicite, vide pour utiliser CODEX_HOME.
    std::filesystem::path codex_home_;

    // Base explicite, vide pour utiliser CodexGlass.db.
    std::wstring database_path_;

    // Drapeau d'annulation lu par le scanner entre les lignes.
    std::atomic_bool cancellation_requested_ = false;

    // Protege le resultat partage.
    std::mutex result_mutex_;

    // Dernier snapshot complet en attente.
    std::optional<TokenUsageSnapshot> result_;

    // Thread de scan possede.
    std::thread thread_;
};

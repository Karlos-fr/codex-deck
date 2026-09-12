// ============================================================================
// Codex Glass - Moniteur d'activite locale Codex
// ----------------------------------------------------------------------------
// Ce fichier declare le watcher incremental des sessions JSONL. Le moniteur
// publie des metadonnees techniques vers Win32 sans dependre du scanner Tokens.
// ============================================================================

#pragma once

#include "CodexActivityTypes.h"

#include <windows.h>

#include <filesystem>
#include <memory>
#include <optional>

// Message prive signalant un nouveau snapshot d'activite Codex.
constexpr UINT kCodexActivityChangedMessage = WM_APP + 24;

// ----------------------------------------------------------------------------
// Surveille les journaux locaux et publie leur activite technique agregee.
// ----------------------------------------------------------------------------
class CodexActivityMonitor {
public:
    // ------------------------------------------------------------------------
    // Cree un moniteur utilisant le profil Codex courant.
    // ------------------------------------------------------------------------
    CodexActivityMonitor();

    // ------------------------------------------------------------------------
    // Cree un moniteur pour un profil explicite.
    //
    // Parametres :
    // - codex_home : racine contenant sessions et archived_sessions.
    // ------------------------------------------------------------------------
    explicit CodexActivityMonitor(std::filesystem::path codex_home);

    // ------------------------------------------------------------------------
    // Arrete et joint automatiquement le thread de surveillance.
    // ------------------------------------------------------------------------
    ~CodexActivityMonitor();

    CodexActivityMonitor(const CodexActivityMonitor&) = delete;
    CodexActivityMonitor& operator=(const CodexActivityMonitor&) = delete;

    // ------------------------------------------------------------------------
    // Demarre la surveillance si elle n'est pas deja active.
    //
    // Parametres :
    // - hwnd : fenetre qui recevra kCodexActivityChangedMessage.
    //
    // Retour :
    // - true lorsque le thread est lance.
    // ------------------------------------------------------------------------
    bool Start(HWND hwnd);

    // ------------------------------------------------------------------------
    // Arrete la surveillance et libere ses handles Windows.
    // ------------------------------------------------------------------------
    void Stop();

    // ------------------------------------------------------------------------
    // Indique si le thread de surveillance est possede.
    // ------------------------------------------------------------------------
    bool IsActive() const;

    // ------------------------------------------------------------------------
    // Transfere le dernier snapshot publie vers le thread UI.
    // ------------------------------------------------------------------------
    std::optional<CodexActivitySnapshot> TakeSnapshot();

private:
    class Impl;

    // Implementation privee contenant les curseurs et handles du watcher.
    std::unique_ptr<Impl> impl_;
};

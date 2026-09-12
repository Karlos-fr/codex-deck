// ============================================================================
// Codex Deck - Processus app-server
// ----------------------------------------------------------------------------
// Ce module lance un processus enfant supervise avec stdin/stdout rediriges et
// un Job Object afin d'eviter tout processus orphelin.
// ============================================================================

#pragma once

#include "CodexError.h"
#include "CodexExecutableResolver.h"

#include <windows.h>

#include <expected>

// ----------------------------------------------------------------------------
// Possede un processus app-server et ses pipes parent.
// ----------------------------------------------------------------------------
class CodexProcess {
public:
    // ------------------------------------------------------------------------
    // Cree un processus vide.
    // ------------------------------------------------------------------------
    CodexProcess() = default;

    // ------------------------------------------------------------------------
    // Arrete le processus et libere les handles restants.
    // ------------------------------------------------------------------------
    ~CodexProcess();

    CodexProcess(const CodexProcess&) = delete;
    CodexProcess& operator=(const CodexProcess&) = delete;

    // ------------------------------------------------------------------------
    // Lance le processus enfant selon la specification fournie.
    //
    // Parametres :
    // - spec : application et ligne de commande a lancer.
    //
    // Retour :
    // - succes vide ou erreur Win32 structuree.
    // ------------------------------------------------------------------------
    std::expected<void, CodexError> Start(const CodexLaunchSpec& spec);

    // ------------------------------------------------------------------------
    // Transfere la possession du pipe stdout parent.
    //
    // Retour :
    // - handle a fermer par l'appelant.
    // ------------------------------------------------------------------------
    HANDLE TakeStdoutReadHandle();

    // ------------------------------------------------------------------------
    // Retourne le pipe stdin parent conserve par le processus.
    //
    // Retour :
    // - handle non possede par l'appelant.
    // ------------------------------------------------------------------------
    HANDLE StdinWriteHandle() const;

    // ------------------------------------------------------------------------
    // Indique si le processus enfant est encore actif.
    //
    // Retour :
    // - true si le process n'a pas encore termine.
    // ------------------------------------------------------------------------
    bool IsRunning() const;

    // ------------------------------------------------------------------------
    // Arrete le processus et ferme les handles possedes.
    // ------------------------------------------------------------------------
    void Stop();

private:
    // Handle du processus enfant.
    HANDLE process_ = nullptr;

    // Handle du thread principal enfant.
    HANDLE thread_ = nullptr;

    // Job Object fermant le processus enfant avec le parent.
    HANDLE job_ = nullptr;

    // Extremite parent pour ecrire dans stdin enfant.
    HANDLE stdin_write_ = nullptr;

    // Extremite parent pour lire stdout enfant.
    HANDLE stdout_read_ = nullptr;
};

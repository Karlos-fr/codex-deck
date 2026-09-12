// ============================================================================
// Codex Deck - Erreurs Codex
// ----------------------------------------------------------------------------
// Ce module definit les erreurs structurees partagees par le lancement de
// processus, le transport JSON-RPC et le parsing du protocole app-server.
// ============================================================================

#pragma once

#include <string>

// ----------------------------------------------------------------------------
// Classe les familles d'erreurs produites par l'integration Codex.
// ----------------------------------------------------------------------------
enum class CodexErrorCode {
    // L'executable Codex n'a pas ete trouve.
    ExecutableNotFound,

    // Le processus app-server n'a pas pu demarrer.
    ProcessLaunchFailed,

    // Le processus app-server s'est termine.
    ProcessExited,

    // La lecture d'un pipe a echoue.
    PipeReadFailed,

    // L'ecriture d'un pipe a echoue.
    PipeWriteFailed,

    // Une ligne recue n'est pas un JSON valide.
    InvalidJson,

    // Une enveloppe ou charge utile ne respecte pas le contrat minimal.
    InvalidResponse,

    // Une reponse JSON-RPC contient une erreur distante.
    RpcError,

    // Une requete RPC a depasse son delai.
    RequestTimeout,

    // Le transport n'est plus connecte.
    Disconnected,
};

// ----------------------------------------------------------------------------
// Decrit une erreur Codex avec message humain et codes optionnels.
// ----------------------------------------------------------------------------
struct CodexError {
    // Famille de l'erreur.
    CodexErrorCode code;

    // Message de diagnostic localise pour les logs ou l'UI.
    std::wstring message;

    // Code systeme Windows optionnel.
    int system_code = 0;

    // Code d'erreur JSON-RPC optionnel.
    int rpc_code = 0;
};

// ============================================================================
// Codex Deck - Facade typee app-server
// ----------------------------------------------------------------------------
// Ce module expose les operations Codex V1 au reste de l'application. Il
// transforme les resultats JSON-RPC en types internes stables.
// ============================================================================

#pragma once

#include "CodexProtocol.h"
#include "JsonRpcTransport.h"

#include <expected>
#include <functional>
#include <optional>
#include <vector>

// ----------------------------------------------------------------------------
// Options de liste de threads.
// ----------------------------------------------------------------------------
struct ThreadListOptions {
    // Indique si la liste cible les archives.
    bool archived = false;

    // Filtre optionnel par repertoire de travail.
    std::optional<std::filesystem::path> cwd;
};

// ----------------------------------------------------------------------------
// Options de creation d'un thread.
// ----------------------------------------------------------------------------
struct StartThreadOptions {
    // Repertoire de travail initial.
    std::filesystem::path cwd;

    // Modele optionnel demande pour ce thread.
    std::optional<std::string> model;
};

// ----------------------------------------------------------------------------
// Options de lancement d'un tour.
// ----------------------------------------------------------------------------
struct StartTurnOptions {
    // Thread recevant le prompt.
    CodexThreadId thread_id;

    // Texte utilisateur a envoyer.
    std::string prompt;
};

// Completion pour une operation sans resultat.
using VoidCompletion = std::move_only_function<void(std::expected<void, CodexError>)>;

// Completion pour un resume de thread.
using ThreadSummaryCompletion = std::move_only_function<void(std::expected<CodexThreadSummary, CodexError>)>;

// Completion pour un detail de thread.
using ThreadDetailCompletion = std::move_only_function<void(std::expected<CodexThreadDetail, CodexError>)>;

// Completion pour une liste de threads.
using ThreadListCompletion = std::move_only_function<void(std::expected<std::vector<CodexThreadSummary>, CodexError>)>;

// Completion pour un identifiant de tour.
using TurnCompletion = std::move_only_function<void(std::expected<CodexTurnId, CodexError>)>;

// ----------------------------------------------------------------------------
// Facade asynchrone des methodes Codex app-server.
// ----------------------------------------------------------------------------
class CodexClient {
public:
    // ------------------------------------------------------------------------
    // Cree une facade autour d'un transport deja demarre.
    //
    // Parametres :
    // - transport : transport JSON-RPC non possede.
    // ------------------------------------------------------------------------
    explicit CodexClient(JsonRpcTransport& transport);

    // ------------------------------------------------------------------------
    // Execute le handshake initialize.
    //
    // Parametres :
    // - completion : callback de fin de handshake.
    // ------------------------------------------------------------------------
    void Connect(VoidCompletion completion);

    // ------------------------------------------------------------------------
    // Liste tous les threads correspondant aux options.
    //
    // Parametres :
    // - options : filtres de liste.
    // - completion : callback recevant la liste exhaustive.
    // ------------------------------------------------------------------------
    void ListThreads(ThreadListOptions options, ThreadListCompletion completion);

    // ------------------------------------------------------------------------
    // Lit passivement un thread avec ses tours.
    //
    // Parametres :
    // - thread_id : identifiant du thread.
    // - completion : callback recevant le detail.
    // ------------------------------------------------------------------------
    void ReadThread(CodexThreadId thread_id, ThreadDetailCompletion completion);

    // ------------------------------------------------------------------------
    // Reprend interactivement un thread avec ses tours.
    //
    // Parametres :
    // - thread_id : identifiant du thread.
    // - completion : callback recevant le detail.
    // ------------------------------------------------------------------------
    void ResumeThread(CodexThreadId thread_id, ThreadDetailCompletion completion);

    // ------------------------------------------------------------------------
    // Cree un thread Codex.
    //
    // Parametres :
    // - options : repertoire et modele optionnel.
    // - completion : callback recevant le resume cree.
    // ------------------------------------------------------------------------
    void StartThread(StartThreadOptions options, ThreadSummaryCompletion completion);

    // ------------------------------------------------------------------------
    // Renomme un thread cote Codex.
    //
    // Parametres :
    // - thread_id : identifiant du thread.
    // - name : nouveau nom.
    // - completion : callback de confirmation.
    // ------------------------------------------------------------------------
    void SetThreadName(CodexThreadId thread_id, std::string name, VoidCompletion completion);

    // ------------------------------------------------------------------------
    // Archive un thread cote Codex.
    //
    // Parametres :
    // - thread_id : identifiant du thread.
    // - completion : callback de confirmation.
    // ------------------------------------------------------------------------
    void ArchiveThread(CodexThreadId thread_id, VoidCompletion completion);

    // ------------------------------------------------------------------------
    // Lance un tour dans un thread.
    //
    // Parametres :
    // - options : thread cible et prompt.
    // - completion : callback recevant l'identifiant du tour.
    // ------------------------------------------------------------------------
    void StartTurn(StartTurnOptions options, TurnCompletion completion);

private:
    // Transport JSON-RPC non possede.
    JsonRpcTransport& transport_;
};

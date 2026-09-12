// ============================================================================
// Codex Deck - Transport JSON-RPC
// ----------------------------------------------------------------------------
// Ce module gere les messages JSON-RPC lignes sur stdin/stdout. Il ne connait
// pas les methodes Codex typees, seulement les enveloppes et leur correlation.
// ============================================================================

#pragma once

#include "CodexProtocol.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

// Identifiant numerique des requetes emises par Codex Deck.
using RpcRequestId = std::uint64_t;

// Completion appelee quand une reponse RPC arrive ou que le transport tombe.
using RpcCompletion = std::move_only_function<void(std::expected<nlohmann::json, CodexError>)>;

// ----------------------------------------------------------------------------
// Transporte des enveloppes JSON-RPC sur pipes Win32.
// ----------------------------------------------------------------------------
class JsonRpcTransport {
public:
    // Handler appele pour chaque notification serveur.
    using NotificationHandler = std::move_only_function<void(CodexNotification)>;

    // Handler appele pour chaque requete serveur.
    using ServerRequestHandler = std::move_only_function<void(CodexServerRequest)>;

    // Handler appele lorsque le transport se deconnecte de facon inattendue.
    using DisconnectHandler = std::move_only_function<void(CodexError)>;

    // ------------------------------------------------------------------------
    // Cree un transport inactif.
    // ------------------------------------------------------------------------
    JsonRpcTransport() = default;

    // ------------------------------------------------------------------------
    // Arrete le transport et libere les handles possedes.
    // ------------------------------------------------------------------------
    ~JsonRpcTransport();

    JsonRpcTransport(const JsonRpcTransport&) = delete;
    JsonRpcTransport& operator=(const JsonRpcTransport&) = delete;

    // ------------------------------------------------------------------------
    // Installe le handler de notifications.
    //
    // Parametres :
    // - handler : callback a appeler depuis le thread de lecture.
    // ------------------------------------------------------------------------
    void SetNotificationHandler(NotificationHandler handler);

    // ------------------------------------------------------------------------
    // Installe le handler de requetes serveur.
    //
    // Parametres :
    // - handler : callback a appeler depuis le thread de lecture.
    // ------------------------------------------------------------------------
    void SetServerRequestHandler(ServerRequestHandler handler);

    // ------------------------------------------------------------------------
    // Installe le handler de deconnexion.
    //
    // Parametres :
    // - handler : callback a appeler depuis le thread de lecture.
    // ------------------------------------------------------------------------
    void SetDisconnectHandler(DisconnectHandler handler);

    // ------------------------------------------------------------------------
    // Demarre la lecture asynchrone sur les pipes fournis.
    //
    // Parametres :
    // - stdout_read : pipe lu par le transport.
    // - stdin_write : pipe ecrit par le transport.
    //
    // Retour :
    // - succes vide ou erreur si les handles sont invalides.
    // ------------------------------------------------------------------------
    std::expected<void, CodexError> Start(HANDLE stdout_read, HANDLE stdin_write);

    // ------------------------------------------------------------------------
    // Envoie une requete JSON-RPC.
    //
    // Parametres :
    // - method : methode distante.
    // - params : parametres JSON.
    // - completion : callback de reponse.
    //
    // Retour :
    // - identifiant genere pour la requete.
    // ------------------------------------------------------------------------
    RpcRequestId Request(std::string method, nlohmann::json params, RpcCompletion completion);

    // ------------------------------------------------------------------------
    // Repond a une requete serveur.
    //
    // Parametres :
    // - id : identifiant JSON-RPC recu du serveur.
    // - result : resultat a renvoyer.
    //
    // Retour :
    // - succes vide ou erreur d'ecriture.
    // ------------------------------------------------------------------------
    std::expected<void, CodexError> SendResponse(const nlohmann::json& id, nlohmann::json result);

    // ------------------------------------------------------------------------
    // Arrete le transport et complete les requetes pendantes.
    // ------------------------------------------------------------------------
    void Stop();

private:
    // ------------------------------------------------------------------------
    // Boucle de lecture executee sur worker.
    // ------------------------------------------------------------------------
    void ReadLoop();

    // ------------------------------------------------------------------------
    // Traite une ligne JSON-RPC recue.
    //
    // Parametres :
    // - line : ligne UTF-8 sans saut final.
    // ------------------------------------------------------------------------
    void ProcessLine(const std::string& line);

    // ------------------------------------------------------------------------
    // Ecrit une enveloppe JSON-RPC complete.
    //
    // Parametres :
    // - payload : objet JSON a serialiser.
    //
    // Retour :
    // - succes vide ou erreur d'ecriture.
    // ------------------------------------------------------------------------
    std::expected<void, CodexError> WriteJsonLine(const nlohmann::json& payload);

    // ------------------------------------------------------------------------
    // Signale une deconnexion et vide les completions pendantes.
    //
    // Parametres :
    // - error : erreur a propager.
    // ------------------------------------------------------------------------
    void Disconnect(CodexError error);

    // Pipe stdout lu par le worker.
    HANDLE stdout_read_ = nullptr;

    // Pipe stdin ecrit par les appels publics.
    HANDLE stdin_write_ = nullptr;

    // Thread de lecture des lignes JSON.
    std::jthread reader_;

    // Indique un arret explicite.
    std::atomic_bool stopping_ = false;

    // Prochain identifiant RPC a emettre.
    std::atomic<RpcRequestId> next_id_ = 1;

    // Protege les ecritures sur stdin.
    std::mutex write_mutex_;

    // Protege la table des completions.
    std::mutex pending_mutex_;

    // Requetes en attente indexees par id.
    std::map<RpcRequestId, RpcCompletion> pending_;

    // Protege les handlers remplaceables.
    std::mutex handler_mutex_;

    // Handler de notification courant.
    NotificationHandler notification_handler_;

    // Handler de requete serveur courant.
    ServerRequestHandler server_request_handler_;

    // Handler de deconnexion courant.
    DisconnectHandler disconnect_handler_;
};

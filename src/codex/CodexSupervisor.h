// ============================================================================
// Codex Deck - Superviseur app-server
// ----------------------------------------------------------------------------
// Ce module lance, connecte et relance l'unique app-server Codex sur un worker,
// sans bloquer le thread UI.
// ============================================================================

#pragma once

#include "CodexClient.h"
#include "CodexExecutableResolver.h"

#include <atomic>
#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <thread>

// ----------------------------------------------------------------------------
// Etat global de connexion au serveur Codex.
// ----------------------------------------------------------------------------
enum class CodexConnectionState {
    // Resolution et lancement initial en cours.
    Starting,

    // Handshake app-server reussi.
    Connected,

    // Relance apres deconnexion ou crash.
    Reconnecting,

    // Codex est actuellement indisponible.
    Unavailable,
};

// ----------------------------------------------------------------------------
// Supervise le cycle de vie du processus app-server.
// ----------------------------------------------------------------------------
class CodexSupervisor {
public:
    // Factory de specification de lancement.
    using LaunchSpecFactory = std::move_only_function<std::expected<CodexLaunchSpec, CodexError>()>;

    // Handler de changement d'etat de connexion.
    using ConnectionStateHandler = std::move_only_function<void(CodexConnectionState)>;

    // Handler de demande de resynchronisation.
    using ResyncRequiredHandler = std::move_only_function<void(CodexClient&)>;

    // Handler de client connecte.
    using ConnectedClientHandler = std::move_only_function<void(CodexClient&)>;

    // ------------------------------------------------------------------------
    // Cree un superviseur inactif.
    // ------------------------------------------------------------------------
    CodexSupervisor() = default;

    // ------------------------------------------------------------------------
    // Arrete le superviseur.
    // ------------------------------------------------------------------------
    ~CodexSupervisor();

    CodexSupervisor(const CodexSupervisor&) = delete;
    CodexSupervisor& operator=(const CodexSupervisor&) = delete;

    // ------------------------------------------------------------------------
    // Installe le handler d'etat.
    //
    // Parametres :
    // - handler : callback appele depuis le worker.
    // ------------------------------------------------------------------------
    void SetConnectionStateHandler(ConnectionStateHandler handler);

    // ------------------------------------------------------------------------
    // Installe le handler de resynchronisation.
    //
    // Parametres :
    // - handler : callback appele apres reconnexion.
    // ------------------------------------------------------------------------
    void SetResyncRequiredHandler(ResyncRequiredHandler handler);

    // ------------------------------------------------------------------------
    // Installe le handler de client connecte.
    //
    // Parametres :
    // - handler : callback appele apres chaque connexion reussie.
    // ------------------------------------------------------------------------
    void SetConnectedClientHandler(ConnectedClientHandler handler);

    // ------------------------------------------------------------------------
    // Demarre la supervision avec une factory injectable.
    //
    // Parametres :
    // - factory : producteur de spec de lancement.
    // ------------------------------------------------------------------------
    void Start(LaunchSpecFactory factory);

    // ------------------------------------------------------------------------
    // Demarre la supervision avec le resolver Codex standard.
    // ------------------------------------------------------------------------
    void Start();

    // ------------------------------------------------------------------------
    // Arrete la supervision et le processus courant.
    // ------------------------------------------------------------------------
    void Stop();

private:
    // ------------------------------------------------------------------------
    // Boucle de supervision worker.
    //
    // Parametres :
    // - factory : producteur de spec de lancement.
    // ------------------------------------------------------------------------
    void Run(LaunchSpecFactory factory);

    // ------------------------------------------------------------------------
    // Publie un changement d'etat.
    //
    // Parametres :
    // - state : nouvel etat.
    // ------------------------------------------------------------------------
    void PublishState(CodexConnectionState state);

    // ------------------------------------------------------------------------
    // Publie une demande de resynchronisation.
    //
    // Parametres :
    // - client : client connecte disponible sur le worker.
    // ------------------------------------------------------------------------
    void PublishResync(CodexClient& client);

    // ------------------------------------------------------------------------
    // Publie le client connecte courant.
    //
    // Parametres :
    // - client : client connecte disponible sur le worker.
    // ------------------------------------------------------------------------
    void PublishConnectedClient(CodexClient& client);

    // Thread de supervision.
    std::jthread worker_;

    // Indique un arret explicite.
    std::atomic_bool stopping_ = false;

    // Protege les callbacks remplaceables.
    std::mutex handler_mutex_;

    // Callback d'etat courant.
    ConnectionStateHandler state_handler_;

    // Callback de resynchronisation courant.
    ResyncRequiredHandler resync_handler_;

    // Callback de client connecte courant.
    ConnectedClientHandler connected_client_handler_;
};

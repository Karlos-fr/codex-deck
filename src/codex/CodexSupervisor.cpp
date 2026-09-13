// ============================================================================
// Codex Deck - Implementation du superviseur app-server
// ----------------------------------------------------------------------------
// Ce fichier gere le backoff de reconnexion et le handshake Codex sur un worker.
// La fenetre peut donc s'afficher avant toute connexion app-server.
// ============================================================================

#include "CodexSupervisor.h"

#include "CodexProcess.h"
#include "JsonRpcTransport.h"

#include <windows.h>

#include <array>
#include <chrono>

namespace {

// Delai maximal de reconnexion.
constexpr auto kMaximumReconnectDelay = std::chrono::seconds(5);

// ----------------------------------------------------------------------------
// Duplique un handle pour le transmettre au transport.
//
// Parametres :
// - handle : handle source.
//
// Retour :
// - handle duplique ou nullptr.
// ----------------------------------------------------------------------------
HANDLE DuplicateForTransport(HANDLE handle) {
    HANDLE duplicate = nullptr;
    DuplicateHandle(
        GetCurrentProcess(),
        handle,
        GetCurrentProcess(),
        &duplicate,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS
    );
    return duplicate;
}

// ----------------------------------------------------------------------------
// Retourne le delai de reconnexion pour une tentative.
//
// Parametres :
// - attempt : index de tentative.
//
// Retour :
// - delai borne a cinq secondes.
// ----------------------------------------------------------------------------
std::chrono::milliseconds BackoffDelay(int attempt) {
    constexpr std::array<std::chrono::milliseconds, 4> delays{
        std::chrono::milliseconds(250),
        std::chrono::milliseconds(500),
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(2000),
    };
    if (attempt < static_cast<int>(delays.size())) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(delays[attempt]);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(kMaximumReconnectDelay);
}

// ----------------------------------------------------------------------------
// Attend un backoff tout en respectant une demande d'arret.
//
// Parametres :
// - delay : duree maximale a attendre.
// - stopping : indicateur d'arret du superviseur.
// ----------------------------------------------------------------------------
void InterruptibleBackoff(std::chrono::milliseconds delay, const std::atomic_bool& stopping) {
    constexpr auto kSleepSlice = std::chrono::milliseconds(50);
    auto remaining = delay;
    while (!stopping && remaining > std::chrono::milliseconds::zero()) {
        const auto slice = remaining < kSleepSlice ? remaining : kSleepSlice;
        std::this_thread::sleep_for(slice);
        remaining -= slice;
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// Arrete le superviseur.
// ----------------------------------------------------------------------------
CodexSupervisor::~CodexSupervisor() {
    Stop();
}

// ----------------------------------------------------------------------------
// Installe le handler d'etat.
// ----------------------------------------------------------------------------
void CodexSupervisor::SetConnectionStateHandler(ConnectionStateHandler handler) {
    std::lock_guard lock(handler_mutex_);
    state_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Installe le handler de resynchronisation.
// ----------------------------------------------------------------------------
void CodexSupervisor::SetResyncRequiredHandler(ResyncRequiredHandler handler) {
    std::lock_guard lock(handler_mutex_);
    resync_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Installe le handler de client connecte.
// ----------------------------------------------------------------------------
void CodexSupervisor::SetConnectedClientHandler(ConnectedClientHandler handler) {
    std::lock_guard lock(handler_mutex_);
    connected_client_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Demarre la supervision avec une factory injectable.
// ----------------------------------------------------------------------------
void CodexSupervisor::Start(LaunchSpecFactory factory) {
    Stop();
    stopping_ = false;
    worker_ = std::jthread([this, factory = std::move(factory)]() mutable {
        Run(std::move(factory));
    });
}

// ----------------------------------------------------------------------------
// Demarre la supervision avec le resolver Codex standard.
// ----------------------------------------------------------------------------
void CodexSupervisor::Start() {
    Start([]() -> std::expected<CodexLaunchSpec, CodexError> {
        return ResolveCodexExecutable();
    });
}

// ----------------------------------------------------------------------------
// Arrete la supervision et le processus courant.
// ----------------------------------------------------------------------------
void CodexSupervisor::Stop() {
    stopping_ = true;
    if (worker_.joinable()) {
        worker_.request_stop();
        worker_.join();
    }
    std::lock_guard lock(client_tasks_mutex_);
    client_tasks_.clear();
}

// Place une operation dans la file du client connecte.
void CodexSupervisor::Submit(ClientTask task) {
    if (!task || stopping_) {
        return;
    }
    std::lock_guard lock(client_tasks_mutex_);
    client_tasks_.push_back(std::move(task));
}

// ----------------------------------------------------------------------------
// Boucle de supervision worker.
// ----------------------------------------------------------------------------
void CodexSupervisor::Run(LaunchSpecFactory factory) {
    bool connected_once = false;
    int reconnect_attempt = 0;

    while (!stopping_) {
        if (!connected_once) {
            PublishState(CodexConnectionState::Starting);
        }
        const auto spec = factory();
        if (!spec) {
            PublishState(CodexConnectionState::Unavailable);
            InterruptibleBackoff(BackoffDelay(reconnect_attempt++), stopping_);
            continue;
        }

        CodexProcess process;
        if (!process.Start(*spec)) {
            PublishState(CodexConnectionState::Unavailable);
            InterruptibleBackoff(BackoffDelay(reconnect_attempt++), stopping_);
            continue;
        }

        JsonRpcTransport transport;
        std::mutex mutex;
        std::condition_variable condition;
        bool disconnected = false;
        bool connect_done = false;
        bool connect_ok = false;

        transport.SetDisconnectHandler([&](CodexError) {
            std::lock_guard lock(mutex);
            disconnected = true;
            condition.notify_all();
        });

        HANDLE stdin_write = DuplicateForTransport(process.StdinWriteHandle());
        if (!transport.Start(process.TakeStdoutReadHandle(), stdin_write)) {
            process.Stop();
            PublishState(CodexConnectionState::Unavailable);
            InterruptibleBackoff(BackoffDelay(reconnect_attempt++), stopping_);
            continue;
        }

        CodexClient client(transport);
        client.Connect([&](std::expected<void, CodexError> result) {
            std::lock_guard lock(mutex);
            connect_ok = result.has_value();
            connect_done = true;
            condition.notify_all();
        });

        {
            std::unique_lock lock(mutex);
            condition.wait_for(lock, std::chrono::seconds(5), [&]() {
                return stopping_ || connect_done || disconnected;
            });
        }

        if (stopping_) {
            process.Stop();
            transport.Stop();
            break;
        }

        if (!connect_ok) {
            transport.Stop();
            process.Stop();
            PublishState(CodexConnectionState::Unavailable);
            InterruptibleBackoff(BackoffDelay(reconnect_attempt++), stopping_);
            continue;
        }

        PublishState(CodexConnectionState::Connected);
        PublishConnectedClient(client);
        if (connected_once) {
            PublishResync(client);
        }
        connected_once = true;
        reconnect_attempt = 0;

        while (!stopping_) {
            DrainClientTasks(client);
            {
                std::unique_lock lock(mutex);
                condition.wait_for(lock, std::chrono::milliseconds(100), [&]() {
                    return stopping_ || disconnected;
                });
                if (disconnected) {
                    break;
                }
            }
            if (!process.IsRunning()) {
                break;
            }
        }

        process.Stop();
        transport.Stop();
        if (!stopping_) {
            PublishState(CodexConnectionState::Reconnecting);
            InterruptibleBackoff(BackoffDelay(reconnect_attempt++), stopping_);
        }
    }
}

// Execute les taches accumulees avec le client connecte courant.
void CodexSupervisor::DrainClientTasks(CodexClient& client) {
    std::deque<ClientTask> tasks;
    {
        std::lock_guard lock(client_tasks_mutex_);
        tasks.swap(client_tasks_);
    }
    for (ClientTask& task : tasks) {
        task(client);
    }
}

// ----------------------------------------------------------------------------
// Publie un changement d'etat.
// ----------------------------------------------------------------------------
void CodexSupervisor::PublishState(CodexConnectionState state) {
    std::lock_guard lock(handler_mutex_);
    if (state_handler_) {
        state_handler_(state);
    }
}

// ----------------------------------------------------------------------------
// Publie une demande de resynchronisation.
// ----------------------------------------------------------------------------
void CodexSupervisor::PublishResync(CodexClient& client) {
    std::lock_guard lock(handler_mutex_);
    if (resync_handler_) {
        resync_handler_(client);
    }
}

// ----------------------------------------------------------------------------
// Publie le client connecte courant.
// ----------------------------------------------------------------------------
void CodexSupervisor::PublishConnectedClient(CodexClient& client) {
    std::lock_guard lock(handler_mutex_);
    if (connected_client_handler_) {
        connected_client_handler_(client);
    }
}

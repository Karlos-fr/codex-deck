// ============================================================================
// Codex Deck - Tests du superviseur Codex
// ----------------------------------------------------------------------------
// Ce fichier valide la reconnexion apres crash du faux app-server et la demande
// de resynchronisation apres retour en etat connecte.
// ============================================================================

#include "codex/CodexSupervisor.h"

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <vector>

// ----------------------------------------------------------------------------
// Verifie la sequence de reconnexion apres un crash post-initialize.
//
// Retour :
// - zero si le superviseur reconnecte et demande une resynchronisation.
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }

    const std::filesystem::path fake_server(argv[1]);
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<CodexConnectionState> states;
    int resync_count = 0;
    int launch_count = 0;

    CodexSupervisor supervisor;
    supervisor.SetConnectionStateHandler([&](CodexConnectionState state) {
        std::lock_guard lock(mutex);
        states.push_back(state);
        condition.notify_all();
    });
    supervisor.SetResyncRequiredHandler([&](CodexClient&) {
        std::lock_guard lock(mutex);
        ++resync_count;
        condition.notify_all();
    });

    supervisor.Start([&]() -> std::expected<CodexLaunchSpec, CodexError> {
        ++launch_count;
        CodexLaunchSpec spec{};
        spec.application_path = fake_server;
        spec.command_line = L"\"" + fake_server.wstring() + L"\" app-server";
        if (launch_count == 1) {
            spec.command_line += L" --exit-after-initialize";
        }
        return spec;
    });

    {
        std::unique_lock lock(mutex);
        condition.wait_for(lock, std::chrono::seconds(8), [&]() {
            return states.size() >= 4 && resync_count == 1;
        });
    }
    supervisor.Stop();

    if (states.size() < 4) {
        return 2;
    }
    if (states[0] != CodexConnectionState::Starting) {
        return 3;
    }
    if (states[1] != CodexConnectionState::Connected) {
        return 4;
    }
    if (states[2] != CodexConnectionState::Reconnecting) {
        return 5;
    }
    if (states[3] != CodexConnectionState::Connected) {
        return 6;
    }
    if (resync_count != 1) {
        return 7;
    }
    return 0;
}

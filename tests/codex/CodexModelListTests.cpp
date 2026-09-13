// ============================================================================
// Codex Deck - Tests du catalogue de modeles Codex
// ----------------------------------------------------------------------------
// Ce fichier valide la pagination et le parsing tolerant de model/list contre
// le faux app-server, sans utiliser de compte Codex reel.
// ============================================================================

#include "codex/CodexClient.h"
#include "codex/CodexProcess.h"
#include "codex/JsonRpcTransport.h"

#include <windows.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>

namespace {

// Duplique un handle de pipe pour le transport de test.
HANDLE DuplicateForTransport(HANDLE handle) {
    HANDLE duplicate = nullptr;
    DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS);
    return duplicate;
}

// Attend la fin d'une operation asynchrone bornee.
bool WaitDone(std::mutex& mutex, std::condition_variable& condition, bool& done) {
    std::unique_lock lock(mutex);
    return condition.wait_for(lock, std::chrono::seconds(3), [&done] { return done; });
}

}  // namespace

// Verifie que model/list concatene les pages dans l'ordre serveur.
int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }
    CodexLaunchSpec spec{};
    spec.application_path = std::filesystem::path(argv[1]);
    spec.command_line = L"\"" + spec.application_path.wstring() + L"\" app-server";
    CodexProcess process;
    if (!process.Start(spec)) {
        return 2;
    }
    JsonRpcTransport transport;
    HANDLE stdin_write = DuplicateForTransport(process.StdinWriteHandle());
    if (!transport.Start(process.TakeStdoutReadHandle(), stdin_write)) {
        return 3;
    }
    CodexClient client(transport);
    std::mutex mutex;
    std::condition_variable condition;
    bool done = false;
    bool ok = false;
    client.Connect([&](std::expected<void, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok) {
        return 4;
    }
    done = false;
    std::vector<CodexModelInfo> models;
    client.ListModels([&](std::expected<std::vector<CodexModelInfo>, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        if (result) {
            models = std::move(*result);
        }
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || models.size() != 2) {
        return 5;
    }
    if (models[0].id != "gpt-5-codex" || !models[0].is_default
        || models[0].supported_efforts != std::vector<std::string>{"medium", "high"}) {
        return 6;
    }
    if (models[1].id != "gpt-5-mini" || models[1].display_name != "GPT-5 mini") {
        return 7;
    }
    transport.Stop();
    process.Stop();
    return 0;
}

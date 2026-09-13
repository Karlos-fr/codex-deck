// ============================================================================
// Codex Deck - Tests des options de liste de threads
// ----------------------------------------------------------------------------
// Ce fichier valide l'encodage d'un probe recent et son arret a max_items
// contre le faux app-server.
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

// Verifie le contrat wire du probe recent borne a cent threads.
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
    if (!transport.Start(process.TakeStdoutReadHandle(), DuplicateForTransport(process.StdinWriteHandle()))) {
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
    std::vector<CodexThreadSummary> threads;
    ThreadListOptions options{};
    options.max_items = 100;
    options.use_state_db_only = true;
    client.ListThreads(options, [&](std::expected<std::vector<CodexThreadSummary>, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        if (result) {
            threads = std::move(*result);
        }
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || threads.size() != 100) {
        return 5;
    }
    if (threads.front().id != "probe_0" || threads.back().id != "probe_99") {
        return 6;
    }
    transport.Stop();
    process.Stop();
    return 0;
}

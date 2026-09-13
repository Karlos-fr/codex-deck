// ============================================================================
// Codex Deck - Tests du service Archive
// ----------------------------------------------------------------------------
// Ce fichier valide chargement paresseux, ordre et restauration avec refresh.
// ============================================================================

#include "sessions/ArchiveSessionService.h"

#include "codex/CodexProcess.h"

#include <windows.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>

namespace {

// Duplique un handle de pipe pour le transport.
HANDLE DuplicateForTransport(HANDLE handle) {
    HANDLE duplicate = nullptr;
    DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS);
    return duplicate;
}

// Attend une completion asynchrone bornee.
bool WaitDone(std::mutex& mutex, std::condition_variable& condition, bool& done) {
    std::unique_lock lock(mutex);
    return condition.wait_for(lock, std::chrono::seconds(3), [&done] { return done; });
}

}  // namespace

// Verifie le chargement archive puis thread/unarchive.
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
    ArchiveViewModel model{};
    bool refreshed = false;
    ArchiveSessionService service(client, model, [&] { refreshed = true; });
    done = false;
    service.LoadArchive([&](std::expected<void, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || model.threads.size() != 1 || model.invalidated) {
        return 5;
    }
    done = false;
    service.RestoreArchivedThread(model.threads.front().id, [&](std::expected<void, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || !refreshed || !model.invalidated) {
        return 6;
    }
    transport.Stop();
    process.Stop();
    return 0;
}

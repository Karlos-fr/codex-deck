// ============================================================================
// Codex Deck - Tests de la facade Codex typee
// ----------------------------------------------------------------------------
// Ce fichier valide le handshake et les operations V1 contre FakeAppServer via
// le processus supervise et le transport JSON-RPC reel.
// ============================================================================

#include "codex/CodexClient.h"
#include "codex/CodexProcess.h"
#include "codex/JsonRpcTransport.h"

#include <windows.h>

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>

namespace {

// ----------------------------------------------------------------------------
// Duplique un handle pour le confier au transport.
//
// Parametres :
// - handle : handle source appartenant au process.
//
// Retour :
// - handle duplique a fermer par le transport.
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
// Attend qu'une completion asynchrone soit recue.
//
// Parametres :
// - mutex : mutex partage.
// - condition : condition signalee par la completion.
// - done : drapeau observe.
//
// Retour :
// - true si la completion arrive a temps.
// ----------------------------------------------------------------------------
bool WaitDone(std::mutex& mutex, std::condition_variable& condition, bool& done) {
    std::unique_lock lock(mutex);
    return condition.wait_for(lock, std::chrono::seconds(3), [&]() {
        return done;
    });
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie les operations typees principales du client.
//
// Retour :
// - zero si la facade respecte le contrat minimal.
// ----------------------------------------------------------------------------
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
    std::vector<CodexThreadSummary> threads;
    client.ListThreads(ThreadListOptions{}, [&](std::expected<std::vector<CodexThreadSummary>, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        if (result) {
            threads = *result;
        }
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || threads.size() != 1 || threads[0].id != "thr_123") {
        return 5;
    }

    done = false;
    CodexThreadSummary created{};
    client.StartThread(StartThreadOptions{std::filesystem::path("D:\\VibeCoding\\codex-deck"), std::nullopt}, [&](std::expected<CodexThreadSummary, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        if (result) {
            created = *result;
        }
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || created.id != "thr_new") {
        return 6;
    }

    done = false;
    client.SetThreadName(created.id, "Renamed", [&](std::expected<void, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok) {
        return 7;
    }

    done = false;
    CodexThreadDetail detail{};
    client.ReadThread(created.id, [&](std::expected<CodexThreadDetail, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        if (result) {
            detail = *result;
        }
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || detail.summary.id != created.id) {
        return 8;
    }

    done = false;
    CodexTurnId turn_id;
    client.StartTurn(StartTurnOptions{created.id, "hello"}, [&](std::expected<CodexTurnId, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        if (result) {
            turn_id = *result;
        }
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok || turn_id != "turn_1") {
        return 9;
    }

    done = false;
    client.ArchiveThread(created.id, [&](std::expected<void, CodexError> result) {
        std::lock_guard lock(mutex);
        ok = result.has_value();
        done = true;
        condition.notify_all();
    });
    if (!WaitDone(mutex, condition, done) || !ok) {
        return 10;
    }

    transport.Stop();
    process.Stop();
    return 0;
}

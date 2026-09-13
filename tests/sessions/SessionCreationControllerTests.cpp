// ============================================================================
// Codex Deck - Tests du controller de creation de session
// ----------------------------------------------------------------------------
// Ce fichier valide creation, association manuelle et conservation du thread
// lorsque le prompt initial echoue.
// ============================================================================

#include "sessions/SessionCreationController.h"

#include "codex/CodexProcess.h"
#include "projects/ProjectRepository.h"
#include "storage/SchemaMigrator.h"

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

// Attend la completion du controller.
bool WaitDone(std::mutex& mutex, std::condition_variable& condition, bool& done) {
    std::unique_lock lock(mutex);
    return condition.wait_for(lock, std::chrono::seconds(3), [&done] { return done; });
}

}  // namespace

// Verifie le cycle de creation avec projet et prompt initial en erreur.
int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }
    CodexLaunchSpec spec{};
    spec.application_path = std::filesystem::path(argv[1]);
    spec.command_line = L"\"" + spec.application_path.wstring() + L"\" app-server --fail-turn-start";
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

    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const auto database_path = std::filesystem::path(temp) / L"CodexDeckTests"
        / (L"session-create-" + std::to_wstring(GetCurrentProcessId()) + L".db");
    std::filesystem::create_directories(database_path.parent_path());
    std::filesystem::remove(database_path);
    auto database = OpenDatabase(database_path);
    SchemaMigrator migrator;
    if (!database || !migrator.Migrate(*database)) {
        return 5;
    }
    ProjectRepository projects(*database);
    auto project = projects.Create("SpotifyAmp");
    if (!project || !projects.AddRoot(project->id, L"D:\\VibeCoding\\spotifyamp")) {
        return 6;
    }
    SessionCatalog catalog;
    catalog.Publish(SessionCatalogSnapshot{});
    SessionMetadataRepository metadata(*database);
    ProjectAssignmentService assignments(catalog, metadata);
    SessionCreationController controller(client, catalog, assignments);

    done = false;
    std::optional<CreatedSession> created;
    controller.CreateSession(
        CreateSessionRequest{project->id, L"D:\\VibeCoding\\spotifyamp", std::nullopt, std::string("hello")},
        [&](std::expected<CreatedSession, SessionCreationError> result) {
            std::lock_guard lock(mutex);
            ok = result.has_value();
            if (result) {
                created = std::move(*result);
            }
            done = true;
            condition.notify_all();
        }
    );
    if (!WaitDone(mutex, condition, done) || !ok || !created || !created->initial_prompt_error) {
        return 7;
    }
    const auto snapshot = catalog.Current();
    const auto persisted = metadata.Get("thr_new");
    if (!snapshot || snapshot->sessions.size() != 1 || snapshot->sessions.front().codex.cwd != L"D:\\VibeCoding\\spotifyamp") {
        return 8;
    }
    if (snapshot->sessions.front().project_id != project->id || !persisted || !*persisted) {
        return 9;
    }
    if ((*persisted)->project_id != project->id || (*persisted)->assignment_source != AssignmentSource::Manual) {
        return 10;
    }
    transport.Stop();
    process.Stop();
    return 0;
}

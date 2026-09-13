// ============================================================================
// Codex Deck - Tests de synchronisation sessions
// ----------------------------------------------------------------------------
// Ce fichier valide le demarrage cache-first et la reconciliation Codex/local
// sans lancer de vrai app-server.
// ============================================================================

#include "projects/ProjectRepository.h"
#include "storage/SchemaMigrator.h"
#include "storage/SessionMetadataRepository.h"
#include "storage/SqliteDatabase.h"
#include "sync/SessionSyncService.h"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <future>
#include <thread>

namespace {

// ----------------------------------------------------------------------------
// Cree une base temporaire migree.
//
// Parametres :
// - suffix : suffixe de fichier.
//
// Retour :
// - connexion SQLite prete.
// ----------------------------------------------------------------------------
SqliteDatabase CreateTestDatabase(const wchar_t* suffix) {
    wchar_t temp_path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp_path);
    const std::filesystem::path path = std::filesystem::path(temp_path)
        / L"CodexDeckTests"
        / (std::wstring(L"sync-") + std::to_wstring(GetCurrentProcessId()) + suffix + L".db");
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::remove(path);
    auto database = OpenDatabase(path);
    SchemaMigrator migrator;
    [[maybe_unused]] const auto migrated = migrator.Migrate(*database);
    return std::move(*database);
}

// ----------------------------------------------------------------------------
// Cree un thread Codex synthetique.
//
// Parametres :
// - id : identifiant.
// - title : titre.
// - cwd : repertoire.
// - updated : timestamp d'activite.
//
// Retour :
// - resume Codex.
// ----------------------------------------------------------------------------
CodexThreadSummary Thread(std::string id, std::string title, std::filesystem::path cwd, std::int64_t updated) {
    CodexThreadSummary thread{};
    thread.id = std::move(id);
    thread.name = std::move(title);
    thread.cwd = std::move(cwd);
    thread.updated_at = updated;
    return thread;
}

// ----------------------------------------------------------------------------
// Probe Git sans resultat.
// ----------------------------------------------------------------------------
class EmptyGitProbe final : public IGitProjectProbe {
public:
    std::expected<std::optional<GitProjectIdentity>, ProjectDetectionError> Inspect(const std::filesystem::path& cwd) override {
        (void)cwd;
        return std::optional<GitProjectIdentity>{};
    }
};

}  // namespace

// ----------------------------------------------------------------------------
// Verifie le chargement cache-first.
//
// Retour :
// - zero si le snapshot cache est publie sans loader Codex.
// ----------------------------------------------------------------------------
int TestLoadCachedState() {
    auto database = CreateTestDatabase(L"-cache");
    SessionMetadataRepository metadata(database);
    if (auto inserted_a = metadata.UpsertCache("thr_a", "A", "D:\\A", 10, SessionStatus::Idle); !inserted_a) {
        return 4;
    }
    if (auto inserted_b = metadata.UpsertCache("thr_b", "B", "D:\\B", 20, SessionStatus::Completed); !inserted_b) {
        return 5;
    }

    bool codex_called = false;
    EmptyGitProbe probe;
    SessionCatalog catalog;
    SessionSyncService service(database, catalog, probe, [&](ThreadListOptions) {
        codex_called = true;
        return std::expected<std::vector<CodexThreadSummary>, CodexError>{std::vector<CodexThreadSummary>{}};
    });

    auto snapshot = service.LoadCachedState();
    if (!snapshot || !*snapshot || (*snapshot)->sessions.size() != 2) {
        return 1;
    }
    if (codex_called) {
        return 2;
    }
    for (const SessionRecord& record : (*snapshot)->sessions) {
        if (record.present_in_codex) {
            return 3;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Verifie la reconciliation Codex/local.
//
// Retour :
// - zero si les sessions sont fusionnees selon le contrat.
// ----------------------------------------------------------------------------
int TestRefreshFromCodex() {
    auto database = CreateTestDatabase(L"-refresh");
    ProjectRepository projects(database);
    auto manual_project = projects.Create("Manual");
    if (!manual_project) {
        return 10;
    }

    SessionMetadataRepository metadata(database);
    if (auto inserted_a = metadata.UpsertCache("thr_a", "Old A", "D:\\old", 100, SessionStatus::Idle); !inserted_a) {
        return 13;
    }
    if (auto assigned_a = metadata.SetAssignment("thr_a", manual_project->id, AssignmentSource::Manual); !assigned_a) {
        return 14;
    }
    if (auto inserted_b = metadata.UpsertCache("thr_b", "B", "D:\\b", 90, SessionStatus::Idle); !inserted_b) {
        return 15;
    }

    EmptyGitProbe probe;
    SessionCatalog catalog;
    SessionSyncService service(database, catalog, probe, [&](ThreadListOptions) {
        return std::expected<std::vector<CodexThreadSummary>, CodexError>{std::vector<CodexThreadSummary>{
            Thread("thr_a", "New A", "D:\\new", 200),
            Thread("thr_c", "C", "D:\\c", 300),
        }};
    });

    auto snapshot = service.RefreshFromCodex();
    if (!snapshot || !*snapshot || (*snapshot)->sessions.size() != 3) {
        return 11;
    }

    bool saw_a = false;
    bool saw_b = false;
    bool saw_c = false;
    for (const SessionRecord& record : (*snapshot)->sessions) {
        if (record.codex.id == "thr_a") {
            saw_a = record.codex.name == "New A"
                && record.codex.updated_at == 200
                && record.project_id == manual_project->id
                && record.assignment_source == AssignmentSource::Manual
                && record.present_in_codex;
        } else if (record.codex.id == "thr_b") {
            saw_b = !record.present_in_codex;
        } else if (record.codex.id == "thr_c") {
            saw_c = record.present_in_codex && !record.project_id;
        }
    }
    return saw_a && saw_b && saw_c ? 0 : 12;
}

// ----------------------------------------------------------------------------
// Verifie le coalescing des demandes de resynchronisation.
//
// Retour :
// - zero si une deuxieme demande concurrente declenche un seul passage suivant.
// ----------------------------------------------------------------------------
int TestRefreshRequestsAreCoalesced() {
    auto database = CreateTestDatabase(L"-coalesce");
    EmptyGitProbe probe;
    SessionCatalog catalog;
    std::atomic_int calls = 0;
    std::promise<void> first_started;
    std::promise<void> release_first;
    std::shared_future<void> release = release_first.get_future().share();

    SessionSyncService service(database, catalog, probe, [&](ThreadListOptions) {
        const int call = ++calls;
        if (call == 1) {
            first_started.set_value();
            release.wait();
        }
        return std::expected<std::vector<CodexThreadSummary>, CodexError>{std::vector<CodexThreadSummary>{
            Thread("thr_sync", "Sync", "D:\\sync", call),
        }};
    });

    std::jthread first([&] {
        [[maybe_unused]] const auto refreshed = service.RequestRefreshFromCodex();
    });

    first_started.get_future().wait();
    [[maybe_unused]] const auto second = service.RequestRefreshFromCodex();
    release_first.set_value();
    first.join();

    auto snapshot = catalog.Current();
    if (!snapshot || snapshot->sessions.size() != 1) {
        return 16;
    }
    return calls == 2 && snapshot->sessions.front().codex.updated_at == 2 ? 0 : 17;
}

// ----------------------------------------------------------------------------
// Verifie qu'un probe externe declenche une seule sync et conserve le manuel.
//
// Retour :
// - zero si creation et renommage externes sont reconcilies correctement.
// ----------------------------------------------------------------------------
int TestExternalProbeRefreshesChangedCatalog() {
    auto database = CreateTestDatabase(L"-external-probe");
    ProjectRepository projects(database);
    auto manual_project = projects.Create("Manual");
    if (!manual_project) {
        return 18;
    }
    SessionMetadataRepository metadata(database);
    if (auto cached = metadata.UpsertCache("A", "Alpha", "D:\\manual", 2, SessionStatus::Idle); !cached) {
        return 19;
    }
    if (auto assigned = metadata.SetAssignment("A", manual_project->id, AssignmentSource::Manual); !assigned) {
        return 20;
    }

    int probe_calls = 0;
    int full_calls = 0;
    EmptyGitProbe git_probe;
    SessionCatalog catalog;
    SessionSyncService service(database, catalog, git_probe, [&](ThreadListOptions options) {
        const bool changed = options.max_items ? ++probe_calls >= 2 : ++full_calls >= 2;
        std::vector<CodexThreadSummary> threads{
            Thread("A", changed ? "Alpha renamed" : "Alpha", "D:\\external", 2),
            Thread("B", "Beta", "D:\\b", 1),
        };
        if (changed) {
            threads.insert(threads.begin(), Thread("C", "Charlie", "D:\\c", 3));
        }
        return std::expected<std::vector<CodexThreadSummary>, CodexError>{std::move(threads)};
    });

    if (auto initial = service.RefreshFromCodex(); !initial) {
        return 21;
    }
    auto stable = service.ProbeExternalChanges();
    auto changed = service.ProbeExternalChanges();
    if (!stable || *stable || !changed || !*changed || full_calls != 2) {
        return 22;
    }
    const auto snapshot = catalog.Current();
    bool saw_a = false;
    bool saw_c = false;
    for (const SessionRecord& session : snapshot->sessions) {
        if (session.codex.id == "A") {
            saw_a = session.codex.name == "Alpha renamed"
                && session.project_id == manual_project->id
                && session.assignment_source == AssignmentSource::Manual;
        } else if (session.codex.id == "C") {
            saw_c = true;
        }
    }
    return saw_a && saw_c ? 0 : 23;
}

// ----------------------------------------------------------------------------
// Execute les tests de synchronisation.
//
// Retour :
// - zero si tous les scenarios passent.
// ----------------------------------------------------------------------------
int main() {
    if (const int result = TestLoadCachedState(); result != 0) {
        return result;
    }
    if (const int result = TestRefreshFromCodex(); result != 0) {
        return result;
    }
    if (const int result = TestRefreshRequestsAreCoalesced(); result != 0) {
        return result;
    }
    if (const int result = TestExternalProbeRefreshesChangedCatalog(); result != 0) {
        return result;
    }
    return 0;
}

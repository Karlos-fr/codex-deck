// ============================================================================
// Codex Deck - Implementation de la synchronisation sessions
// ----------------------------------------------------------------------------
// Ce fichier charge un snapshot cache-first puis fusionne les listes Codex avec
// les metadonnees locales et l'auto-detection projet.
// ============================================================================

#include "SessionSyncService.h"

#include "../projects/ProjectDiscoveryService.h"
#include "../projects/ProjectRepository.h"
#include "../storage/SessionMetadataRepository.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace {

// ----------------------------------------------------------------------------
// Convertit une erreur Codex en erreur stockage generique pour ce service.
//
// Parametres :
// - error : erreur Codex source.
//
// Retour :
// - erreur StorageError.
// ----------------------------------------------------------------------------
StorageError CodexAsStorageError(const CodexError& error) {
    return StorageError{StorageErrorCode::SqlFailed, error.system_code, "Codex sync failed"};
}

// ----------------------------------------------------------------------------
// Construit un resume Codex depuis une metadonnee de cache.
//
// Parametres :
// - metadata : metadonnees locales.
//
// Retour :
// - resume equivalent pour snapshot cache.
// ----------------------------------------------------------------------------
CodexThreadSummary CachedThread(const SessionMetadata& metadata) {
    CodexThreadSummary thread{};
    thread.id = metadata.thread_id;
    thread.name = metadata.cached_title;
    thread.cwd = metadata.cached_cwd;
    thread.updated_at = metadata.last_activity;
    return thread;
}

// ----------------------------------------------------------------------------
// Construit un record depuis les metadonnees locales.
//
// Parametres :
// - metadata : metadonnees locales.
//
// Retour :
// - session non presente dans Codex.
// ----------------------------------------------------------------------------
SessionRecord CachedRecord(const SessionMetadata& metadata) {
    SessionRecord record{};
    record.codex = CachedThread(metadata);
    record.project_id = metadata.project_id;
    record.assignment_source = metadata.assignment_source;
    record.favorite = metadata.favorite;
    record.status = metadata.last_known_status;
    record.present_in_codex = false;
    return record;
}

// ----------------------------------------------------------------------------
// Fusionne un thread Codex avec ses metadonnees.
//
// Parametres :
// - thread : thread Codex.
// - metadata : metadonnees optionnelles.
// - project_id : projet detecte.
//
// Retour :
// - record present dans Codex.
// ----------------------------------------------------------------------------
SessionRecord MergedRecord(
    const CodexThreadSummary& thread,
    const std::optional<SessionMetadata>& metadata,
    std::optional<ProjectId> project_id
) {
    SessionRecord record{};
    record.codex = thread;
    record.project_id = project_id;
    if (metadata) {
        record.assignment_source = metadata->assignment_source;
        record.favorite = metadata->favorite;
        record.status = metadata->last_known_status;
    }
    record.present_in_codex = true;
    return record;
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree un service de sync.
// ----------------------------------------------------------------------------
SessionSyncService::SessionSyncService(
    SqliteDatabase& database,
    SessionCatalog& catalog,
    IGitProjectProbe& git_probe,
    ThreadListLoader loader
)
    : database_(database),
      catalog_(catalog),
      git_probe_(git_probe),
      loader_(std::move(loader)) {
}

// ----------------------------------------------------------------------------
// Charge l'etat cache sans appeler Codex.
// ----------------------------------------------------------------------------
std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> SessionSyncService::LoadCachedState() {
    ProjectRepository project_repository(database_);
    SessionMetadataRepository metadata_repository(database_);

    auto projects = project_repository.List();
    if (!projects) {
        return std::unexpected(projects.error());
    }
    auto metadata = metadata_repository.ListAll();
    if (!metadata) {
        return std::unexpected(metadata.error());
    }

    SessionCatalogSnapshot snapshot{};
    snapshot.projects = std::move(*projects);
    for (const SessionMetadata& item : *metadata) {
        snapshot.sessions.push_back(CachedRecord(item));
    }
    return catalog_.Publish(std::move(snapshot));
}

// ----------------------------------------------------------------------------
// Rafraichit depuis Codex puis publie une reconciliation.
// ----------------------------------------------------------------------------
std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> SessionSyncService::RefreshFromCodex() {
    auto codex_threads = loader_(ThreadListOptions{});
    if (!codex_threads) {
        return std::unexpected(CodexAsStorageError(codex_threads.error()));
    }

    ProjectRepository project_repository(database_);
    SessionMetadataRepository metadata_repository(database_);
    auto projects = project_repository.List();
    if (!projects) {
        return std::unexpected(projects.error());
    }
    projects = DiscoverGitProjects(*codex_threads, std::move(*projects), project_repository, git_probe_);
    if (!projects) {
        return std::unexpected(projects.error());
    }
    auto all_metadata = metadata_repository.ListAll();
    if (!all_metadata) {
        return std::unexpected(all_metadata.error());
    }

    std::map<CodexThreadId, SessionMetadata> metadata_by_thread;
    for (SessionMetadata& metadata : *all_metadata) {
        metadata_by_thread.emplace(metadata.thread_id, std::move(metadata));
    }

    std::set<CodexThreadId> present;
    SessionCatalogSnapshot snapshot{};
    snapshot.projects = *projects;
    for (const CodexThreadSummary& thread : *codex_threads) {
        present.insert(thread.id);
        std::optional<SessionMetadata> metadata;
        if (const auto found = metadata_by_thread.find(thread.id); found != metadata_by_thread.end()) {
            metadata = found->second;
        }

        const std::optional<ProjectId> project_id = DetectProject(thread, *projects, metadata, git_probe_);
        if (auto cached = metadata_repository.UpsertCache(thread.id, thread.name, thread.cwd, thread.updated_at, SessionStatus::Idle); !cached) {
            return std::unexpected(cached.error());
        }
        if (!metadata || metadata->assignment_source != AssignmentSource::Manual) {
            if (auto assigned = metadata_repository.SetAssignment(thread.id, project_id, AssignmentSource::Automatic); !assigned) {
                return std::unexpected(assigned.error());
            }
        }
        snapshot.sessions.push_back(MergedRecord(thread, metadata, project_id));
    }

    for (const auto& [thread_id, metadata] : metadata_by_thread) {
        if (!present.contains(thread_id)) {
            snapshot.sessions.push_back(CachedRecord(metadata));
        }
    }
    auto published = catalog_.Publish(std::move(snapshot));
    std::vector<CodexThreadSummary> recent;
    for (const SessionRecord& session : published->sessions) {
        if (session.present_in_codex && !session.codex.archived) {
            recent.push_back(session.codex);
        }
    }
    std::ranges::stable_sort(recent, [](const CodexThreadSummary& left, const CodexThreadSummary& right) {
        return left.updated_at > right.updated_at;
    });
    if (recent.size() > 100) {
        recent.resize(100);
    }
    recent_fingerprint_ = RecentSessionFingerprint(recent);
    return published;
}

// ----------------------------------------------------------------------------
// Demande un refresh Codex en coalescant les appels concurrents.
// ----------------------------------------------------------------------------
std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> SessionSyncService::RequestRefreshFromCodex() {
    {
        std::lock_guard lock(refresh_mutex_);
        if (refresh_active_) {
            refresh_requested_again_ = true;
            return catalog_.Current();
        }
        refresh_active_ = true;
    }

    std::expected<std::shared_ptr<const SessionCatalogSnapshot>, StorageError> last_snapshot = catalog_.Current();
    while (true) {
        last_snapshot = RefreshFromCodex();

        std::lock_guard lock(refresh_mutex_);
        if (!last_snapshot || !refresh_requested_again_) {
            refresh_active_ = false;
            refresh_requested_again_ = false;
            return last_snapshot;
        }
        refresh_requested_again_ = false;
    }
}

// Compare le probe recent et declenche une sync complete si necessaire.
std::expected<bool, StorageError> SessionSyncService::ProbeExternalChanges() {
    ThreadListOptions options{};
    options.max_items = 100;
    options.use_state_db_only = true;
    auto threads = loader_(options);
    if (!threads) {
        return std::unexpected(CodexAsStorageError(threads.error()));
    }
    const std::uint64_t fingerprint = RecentSessionFingerprint(*threads);
    if (!recent_fingerprint_) {
        recent_fingerprint_ = fingerprint;
        return false;
    }
    if (*recent_fingerprint_ == fingerprint) {
        return false;
    }
    if (auto refreshed = RequestRefreshFromCodex(); !refreshed) {
        return std::unexpected(refreshed.error());
    }
    return true;
}

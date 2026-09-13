// ============================================================================
// Codex Deck - Implementation de la decouverte de projets Git
// ----------------------------------------------------------------------------
// Ce fichier deduplique les cwd et racines Git avant de persister les projets.
// Une sonde Git indisponible reste non fatale pour la synchronisation Codex.
// ============================================================================

#include "ProjectDiscoveryService.h"

#include "PathNormalization.h"

#include <map>
#include <set>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Produit un nom UTF-8 lisible depuis le dernier segment d'une racine Git.
//
// Parametres :
// - root : racine du depot.
//
// Retour :
// - dernier segment UTF-8, ou chemin complet si ce segment est vide.
// ----------------------------------------------------------------------------
std::string ProjectNameFromRoot(const std::filesystem::path& root) {
    const std::filesystem::path leaf = root.filename().empty() ? root : root.filename();
    const std::u8string utf8 = leaf.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

// ----------------------------------------------------------------------------
// Ajoute une racine a un projet existant et a son index de detection.
//
// Parametres :
// - project : projet a completer.
// - root : racine Git detectee.
// - repository : stockage cible.
// - roots : index racine vers projet a actualiser.
// - project_index : position du projet dans la liste.
//
// Retour :
// - succes vide ou erreur de persistance.
// ----------------------------------------------------------------------------
std::expected<void, StorageError> AttachRoot(
    Project& project,
    const std::filesystem::path& root,
    ProjectRepository& repository,
    std::map<std::wstring, std::size_t>& roots,
    std::size_t project_index
) {
    if (auto added = repository.AddRoot(project.id, root); !added) {
        return std::unexpected(added.error());
    }
    project.roots.push_back(root);
    roots.emplace(NormalizeWindowsPath(root), project_index);
    return {};
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree ou complete les projets correspondant aux depots Git des threads.
// ----------------------------------------------------------------------------
std::expected<std::vector<Project>, StorageError> DiscoverGitProjects(
    const std::vector<CodexThreadSummary>& threads,
    std::vector<Project> projects,
    ProjectRepository& repository,
    IGitProjectProbe& git_probe
) {
    std::map<std::wstring, std::size_t> roots;
    std::map<std::string, std::size_t> remotes;
    for (std::size_t index = 0; index < projects.size(); ++index) {
        for (const std::filesystem::path& root : projects[index].roots) {
            roots.emplace(NormalizeWindowsPath(root), index);
        }
        if (projects[index].git_remote) {
            remotes.emplace(*projects[index].git_remote, index);
        }
    }

    std::set<std::wstring> inspected_workspaces;
    for (const CodexThreadSummary& thread : threads) {
        if (thread.cwd.empty()) {
            continue;
        }
        const std::wstring normalized_cwd = NormalizeWindowsPath(thread.cwd);
        if (!inspected_workspaces.insert(normalized_cwd).second) {
            continue;
        }

        const auto inspected = git_probe.Inspect(thread.cwd);
        if (!inspected || !*inspected || (*inspected)->root.empty()) {
            continue;
        }
        const GitProjectIdentity& identity = **inspected;
        const std::wstring normalized_root = NormalizeWindowsPath(identity.root);
        if (roots.contains(normalized_root)) {
            continue;
        }

        if (identity.origin_remote) {
            if (const auto same_remote = remotes.find(*identity.origin_remote); same_remote != remotes.end()) {
                if (auto attached = AttachRoot(
                        projects[same_remote->second],
                        identity.root,
                        repository,
                        roots,
                        same_remote->second
                    ); !attached) {
                    return std::unexpected(attached.error());
                }
                continue;
            }
        }

        auto created = repository.Create(ProjectNameFromRoot(identity.root));
        if (!created) {
            return std::unexpected(created.error());
        }
        if (auto added = repository.AddRoot(created->id, identity.root); !added) {
            [[maybe_unused]] const auto rollback = repository.Delete(created->id);
            return std::unexpected(added.error());
        }
        created->roots.push_back(identity.root);
        if (identity.origin_remote) {
            if (auto stored = repository.SetGitRemote(created->id, *identity.origin_remote); !stored) {
                [[maybe_unused]] const auto rollback = repository.Delete(created->id);
                return std::unexpected(stored.error());
            }
            created->git_remote = identity.origin_remote;
        }

        const std::size_t created_index = projects.size();
        roots.emplace(normalized_root, created_index);
        if (created->git_remote) {
            remotes.emplace(*created->git_remote, created_index);
        }
        projects.push_back(std::move(*created));
    }
    return projects;
}

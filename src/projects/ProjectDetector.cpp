// ============================================================================
// Codex Deck - Implementation de la detection projet
// ----------------------------------------------------------------------------
// Ce fichier applique les priorites : manuel, cwd sous root le plus long, remote
// Git identique, puis Unassigned.
// ============================================================================

#include "ProjectDetector.h"

#include "PathNormalization.h"

namespace {

// ----------------------------------------------------------------------------
// Indique si un cwd normalise est sous une racine normalisee.
//
// Parametres :
// - cwd : chemin normalise du thread.
// - root : racine normalisee du projet.
//
// Retour :
// - true si root contient cwd.
// ----------------------------------------------------------------------------
bool IsUnderRoot(const std::wstring& cwd, const std::wstring& root) {
    if (cwd == root) {
        return true;
    }
    if (cwd.size() <= root.size() || cwd.rfind(root, 0) != 0) {
        return false;
    }
    return root.ends_with(L"\\") || cwd[root.size()] == L'\\';
}

}  // namespace

// ----------------------------------------------------------------------------
// Detecte le projet d'un thread Codex.
// ----------------------------------------------------------------------------
std::optional<ProjectId> DetectProject(
    const CodexThreadSummary& thread,
    const std::vector<Project>& projects,
    const std::optional<SessionMetadata>& existing_metadata,
    IGitProjectProbe& git_probe
) {
    if (existing_metadata && existing_metadata->assignment_source == AssignmentSource::Manual) {
        return existing_metadata->project_id;
    }

    const std::wstring cwd = NormalizeWindowsPath(thread.cwd);
    std::optional<ProjectId> best_project;
    std::size_t best_length = 0;
    for (const Project& project : projects) {
        for (const std::filesystem::path& root_path : project.roots) {
            const std::wstring root = NormalizeWindowsPath(root_path);
            if (IsUnderRoot(cwd, root) && root.size() > best_length) {
                best_project = project.id;
                best_length = root.size();
            }
        }
    }
    if (best_project) {
        return best_project;
    }

    const auto identity = git_probe.Inspect(thread.cwd);
    if (!identity || !*identity || !(*identity)->origin_remote) {
        return std::nullopt;
    }
    for (const Project& project : projects) {
        if (project.git_remote && *project.git_remote == *(*identity)->origin_remote) {
            return project.id;
        }
    }
    return std::nullopt;
}

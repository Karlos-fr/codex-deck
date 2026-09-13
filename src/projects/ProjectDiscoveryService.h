// ============================================================================
// Codex Deck - Decouverte de projets Git
// ----------------------------------------------------------------------------
// Ce module materialise les depots Git rencontres dans les cwd Codex en projets
// locaux. Il ne modifie jamais les associations manuelles des sessions.
// ============================================================================

#pragma once

#include "GitProjectProbe.h"
#include "ProjectRepository.h"
#include "ProjectTypes.h"

#include "../codex/CodexTypes.h"

#include <expected>
#include <vector>

// ----------------------------------------------------------------------------
// Cree ou complete les projets correspondant aux depots Git des threads.
//
// Parametres :
// - threads : resumes Codex a examiner.
// - projects : projets locaux deja connus.
// - repository : stockage utilise pour les projets decouverts.
// - git_probe : sonde Git injectable.
//
// Retour :
// - liste actualisee des projets ou erreur de persistance.
// ----------------------------------------------------------------------------
std::expected<std::vector<Project>, StorageError> DiscoverGitProjects(
    const std::vector<CodexThreadSummary>& threads,
    std::vector<Project> projects,
    ProjectRepository& repository,
    IGitProjectProbe& git_probe
);

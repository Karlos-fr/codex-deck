// ============================================================================
// Codex Deck - Detection de projet logique
// ----------------------------------------------------------------------------
// Ce module associe un thread Codex a un projet local depuis son cwd et, si
// necessaire, depuis l'identite Git injectee.
// ============================================================================

#pragma once

#include "GitProjectProbe.h"
#include "ProjectTypes.h"

#include "../codex/CodexTypes.h"

#include <optional>
#include <vector>

// ----------------------------------------------------------------------------
// Detecte le projet d'un thread Codex.
//
// Parametres :
// - thread : thread Codex a associer.
// - projects : projets locaux connus.
// - existing_metadata : metadonnees locales existantes.
// - git_probe : probe Git injectable.
//
// Retour :
// - projet detecte ou std::nullopt pour Unassigned.
// ----------------------------------------------------------------------------
std::optional<ProjectId> DetectProject(
    const CodexThreadSummary& thread,
    const std::vector<Project>& projects,
    const std::optional<SessionMetadata>& existing_metadata,
    IGitProjectProbe& git_probe
);

// ============================================================================
// Codex Deck - Resolution de l'executable Codex
// ----------------------------------------------------------------------------
// Ce module choisit l'executable a lancer pour `codex app-server` sans demarrer
// de processus. Il se limite a la resolution de chemin et de ligne de commande.
// ============================================================================

#pragma once

#include "CodexError.h"

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

// ----------------------------------------------------------------------------
// Decrit la commande native a passer a CreateProcess.
// ----------------------------------------------------------------------------
struct CodexLaunchSpec {
    // Application native transmise a CreateProcessW.
    std::filesystem::path application_path;

    // Ligne de commande complete mutable pour CreateProcessW.
    std::wstring command_line;
};

// Callback injectable pour chercher un executable par nom.
using PathLookup = std::function<std::optional<std::filesystem::path>(std::wstring_view)>;

// ----------------------------------------------------------------------------
// Resolut l'executable Codex avec une recherche injectable.
//
// Parametres :
// - override_path : chemin explicite prioritaire.
// - lookup : fonction de recherche pour codex.exe puis codex.cmd.
//
// Retour :
// - specification de lancement ou erreur si rien n'est trouve.
// ----------------------------------------------------------------------------
std::expected<CodexLaunchSpec, CodexError> ResolveCodexExecutable(
    const std::optional<std::filesystem::path>& override_path,
    const PathLookup& lookup
);

// ----------------------------------------------------------------------------
// Resolut l'executable Codex avec SearchPathW.
//
// Parametres :
// - override_path : chemin explicite prioritaire.
//
// Retour :
// - specification de lancement ou erreur si rien n'est trouve.
// ----------------------------------------------------------------------------
std::expected<CodexLaunchSpec, CodexError> ResolveCodexExecutable(
    const std::optional<std::filesystem::path>& override_path = std::nullopt
);

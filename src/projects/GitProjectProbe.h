// ============================================================================
// Codex Deck - Probe d'identite Git
// ----------------------------------------------------------------------------
// Ce module interroge Git uniquement pour identifier un workspace. Il ne fournit
// pas de moteur Git generaliste.
// ============================================================================

#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

// ----------------------------------------------------------------------------
// Erreur non fatale de detection projet.
// ----------------------------------------------------------------------------
struct ProjectDetectionError {
    // Message technique court.
    std::string message;
};

// ----------------------------------------------------------------------------
// Identite Git d'un workspace.
// ----------------------------------------------------------------------------
struct GitProjectIdentity {
    // Racine Git detectee.
    std::filesystem::path root;

    // Remote origin optionnel.
    std::optional<std::string> origin_remote;
};

// ----------------------------------------------------------------------------
// Interface injectable de probe Git.
// ----------------------------------------------------------------------------
class IGitProjectProbe {
public:
    // ------------------------------------------------------------------------
    // Libere l'interface de probe.
    // ------------------------------------------------------------------------
    virtual ~IGitProjectProbe() = default;

    // ------------------------------------------------------------------------
    // Inspecte un cwd pour trouver une identite Git.
    //
    // Parametres :
    // - cwd : repertoire de travail du thread.
    //
    // Retour :
    // - identite optionnelle ou erreur technique.
    // ------------------------------------------------------------------------
    virtual std::expected<std::optional<GitProjectIdentity>, ProjectDetectionError> Inspect(const std::filesystem::path& cwd) = 0;
};

// ----------------------------------------------------------------------------
// Probe Git reel base sur la commande git.
// ----------------------------------------------------------------------------
class GitProjectProbe final : public IGitProjectProbe {
public:
    // ------------------------------------------------------------------------
    // Inspecte un cwd via git rev-parse puis remote get-url.
    //
    // Parametres :
    // - cwd : repertoire de travail du thread.
    //
    // Retour :
    // - identite optionnelle, std::nullopt si Git est absent ou hors repo.
    // ------------------------------------------------------------------------
    std::expected<std::optional<GitProjectIdentity>, ProjectDetectionError> Inspect(const std::filesystem::path& cwd) override;
};

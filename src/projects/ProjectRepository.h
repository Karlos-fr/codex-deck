// ============================================================================
// Codex Deck - Repository projets
// ----------------------------------------------------------------------------
// Ce module persiste les projets logiques et leurs racines dans SQLite.
// ============================================================================

#pragma once

#include "ProjectTypes.h"

#include "../storage/SqliteDatabase.h"

#include <expected>

// ----------------------------------------------------------------------------
// Acces SQLite aux projets.
// ----------------------------------------------------------------------------
class ProjectRepository {
public:
    // ------------------------------------------------------------------------
    // Cree un repository sur une base ouverte.
    //
    // Parametres :
    // - database : base SQLite non possedee.
    // ------------------------------------------------------------------------
    explicit ProjectRepository(SqliteDatabase& database);

    // ------------------------------------------------------------------------
    // Cree un projet sans root initial.
    //
    // Parametres :
    // - name : nom affiche.
    //
    // Retour :
    // - projet cree ou erreur.
    // ------------------------------------------------------------------------
    std::expected<Project, StorageError> Create(std::string_view name);

    // ------------------------------------------------------------------------
    // Liste les projets et leurs roots.
    //
    // Retour :
    // - projets tries par identifiant ou erreur.
    // ------------------------------------------------------------------------
    std::expected<std::vector<Project>, StorageError> List();

    // ------------------------------------------------------------------------
    // Ajoute une racine a un projet.
    //
    // Parametres :
    // - project_id : projet cible.
    // - root : chemin racine.
    //
    // Retour :
    // - succes vide ou erreur.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> AddRoot(ProjectId project_id, const std::filesystem::path& root);

    // ------------------------------------------------------------------------
    // Definit le remote Git d'un projet.
    //
    // Parametres :
    // - project_id : projet cible.
    // - remote : remote a stocker.
    //
    // Retour :
    // - succes vide ou erreur.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> SetGitRemote(ProjectId project_id, std::string_view remote);

    // ------------------------------------------------------------------------
    // Supprime un projet et ses roots.
    //
    // Parametres :
    // - project_id : projet cible.
    //
    // Retour :
    // - succes vide ou erreur.
    // ------------------------------------------------------------------------
    std::expected<void, StorageError> Delete(ProjectId project_id);

private:
    // Base SQLite non possedee.
    SqliteDatabase& database_;
};

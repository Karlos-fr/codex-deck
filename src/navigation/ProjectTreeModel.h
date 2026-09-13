// ============================================================================
// Codex Deck - Modele aplati du Tree
// ----------------------------------------------------------------------------
// Ce module transforme le catalogue projets/sessions en lignes stables
// independantes du rendu Direct2D et de l'input Win32.
// ============================================================================

#pragma once

#include "../model/SessionCatalog.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ----------------------------------------------------------------------------
// Type visuel et interactif d'une ligne d'arbre.
// ----------------------------------------------------------------------------
enum class TreeRowKind {
    // Ligne de projet logique.
    Project,

    // Ligne de session Codex.
    Session,

    // Ligne ouvrant les sessions masquees d'un projet.
    More,

    // En-tete fixe des sessions non assignees.
    UnassignedHeader,

    // Entree d'acces aux archives.
    ArchiveEntry,
};

// ----------------------------------------------------------------------------
// Ligne aplatie stable du Tree.
// ----------------------------------------------------------------------------
struct TreeRow {
    // Nature de la ligne.
    TreeRowKind kind = TreeRowKind::Session;

    // Identifiant stable pour cache, selection et hit testing.
    std::string stable_id;

    // Profondeur d'indentation logique.
    int depth = 0;

    // Texte principal affiche.
    std::wstring primary_text;

    // Texte secondaire affiche.
    std::wstring secondary_text;

    // Statut associe aux sessions.
    SessionStatus status = SessionStatus::Idle;

    // Projet associe lorsque pertinent.
    std::optional<ProjectId> project_id;

    // Thread associe lorsque pertinent.
    std::optional<CodexThreadId> thread_id;

    // Nombre de sessions masquees pour une ligne More.
    std::size_t hidden_count = 0;

    // Indique si une section depliable est ouverte.
    bool expanded = false;
};

// ----------------------------------------------------------------------------
// Etat de presentation de l'arbre.
// ----------------------------------------------------------------------------
struct ProjectTreeState {
    // Projets actuellement deployes.
    std::unordered_set<ProjectId> expanded_projects;

    // Indique si la section Unassigned est deployee.
    bool unassigned_expanded = true;

    // Session selectionnee dans le Tree.
    std::optional<CodexThreadId> selected_thread;

    // Nombre de sessions visibles demande par projet.
    std::unordered_map<ProjectId, std::size_t> visible_sessions_by_project;
};

// ----------------------------------------------------------------------------
// Construit les lignes aplaties de navigation.
//
// Parametres :
// - catalog : snapshot source.
// - state : etat de presentation.
// - max_sessions_per_project : limite initiale par projet.
//
// Retour :
// - lignes stables triees et limitees.
// ----------------------------------------------------------------------------
std::vector<TreeRow> BuildProjectTreeRows(
    const SessionCatalogSnapshot& catalog,
    const ProjectTreeState& state,
    std::size_t max_sessions_per_project = 8
);

// ============================================================================
// Codex Deck - Modele de Command Palette
// ----------------------------------------------------------------------------
// Ce module construit des entrees de navigation et d'action depuis un snapshot,
// sans lancer les commandes ni bloquer l'UI.
// ============================================================================

#pragma once

#include "../app/DeckCommand.h"
#include "../model/SessionCatalog.h"

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Famille d'entree de palette.
// ----------------------------------------------------------------------------
enum class PaletteEntryKind {
    // Entree de session.
    Session,

    // Entree de projet.
    Project,

    // Entree d'action globale.
    Action,
};

// ----------------------------------------------------------------------------
// Mode fonctionnel de la palette partagee.
// ----------------------------------------------------------------------------
enum class CommandPaletteMode {
    // Recherche limitee aux sessions et projets.
    Search,

    // Palette globale incluant les actions applicatives.
    Commands,
};

// ----------------------------------------------------------------------------
// Entree scoree de palette.
// ----------------------------------------------------------------------------
struct PaletteEntry {
    // Famille de l'entree.
    PaletteEntryKind kind = PaletteEntryKind::Action;

    // Identifiant stable.
    std::string id;

    // Titre affiche.
    std::wstring title;

    // Sous-titre ou groupe affiche.
    std::wstring subtitle;

    // Score fuzzy.
    int score = 0;

    // Commande a emettre a la validation.
    DeckCommand command;
};

// ----------------------------------------------------------------------------
// Construit les entrees de palette correspondant a une requete.
//
// Parametres :
// - snapshot : catalogue source.
// - query : requete deja normalisee.
// - max_results : nombre maximal d'entrees retournees.
//
// Retour :
// - entrees triees par score puis groupe.
// ----------------------------------------------------------------------------
std::vector<PaletteEntry> BuildCommandPaletteEntries(
    const SessionCatalogSnapshot& snapshot,
    std::wstring_view query,
    CommandPaletteMode mode = CommandPaletteMode::Commands,
    std::size_t max_results = 50
);

// ============================================================================
// Codex Deck - Modele de la vue Archive
// ----------------------------------------------------------------------------
// Ce module trie et conserve separement les threads archives sans les injecter
// dans l'arbre principal.
// ============================================================================

#pragma once

#include "../codex/CodexTypes.h"

// Cache de la vue d'archives chargee a la demande.
struct ArchiveViewModel {
    // Threads archives tries par activite decroissante.
    std::vector<CodexThreadSummary> threads;
    // Indique qu'un chargement est en cours.
    bool loading = false;
    // Indique que le cache doit etre recharge.
    bool invalidated = true;
};

// Remplace et trie le contenu archive par activite recente.
void SetArchiveThreads(ArchiveViewModel& model, std::vector<CodexThreadSummary> threads);

// ============================================================================
// Codex Deck - Implementation du modele Archive
// ----------------------------------------------------------------------------
// Ce fichier maintient un ordre stable recent sans modifier le catalogue actif.
// ============================================================================

#include "ArchiveViewModel.h"

#include <algorithm>

// Remplace et trie le contenu archive par activite recente.
void SetArchiveThreads(ArchiveViewModel& model, std::vector<CodexThreadSummary> threads) {
    std::ranges::stable_sort(threads, [](const CodexThreadSummary& left, const CodexThreadSummary& right) {
        return left.updated_at > right.updated_at;
    });
    model.threads = std::move(threads);
    model.loading = false;
    model.invalidated = false;
}

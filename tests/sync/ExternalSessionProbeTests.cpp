// ============================================================================
// Codex Deck - Tests du fingerprint de sessions externes
// ----------------------------------------------------------------------------
// Ce fichier valide qu'une creation ou un renommage externe demande une seule
// sync complete et que les options du probe restent bornees.
// ============================================================================

#include "sync/ExternalSessionProbe.h"

// Cree un thread synthetique pour les probes.
CodexThreadSummary Thread(std::string id, std::string name, std::int64_t updated_at) {
    CodexThreadSummary thread{};
    thread.id = std::move(id);
    thread.name = std::move(name);
    thread.cwd = L"D:\\Work";
    thread.updated_at = updated_at;
    return thread;
}

// Verifie baseline, stabilite et detection d'un changement externe.
int main() {
    std::size_t call = 0;
    int refreshes = 0;
    ExternalSessionProbe probe(
        [&call](ThreadListOptions options) -> std::expected<std::vector<CodexThreadSummary>, CodexError> {
            if (options.max_items != 100 || !options.use_state_db_only
                || options.sort_key != "recency_at" || options.sort_direction != "desc") {
                return std::unexpected(CodexError{CodexErrorCode::InvalidResponse, L"Options invalides"});
            }
            ++call;
            if (call < 3) {
                return std::vector<CodexThreadSummary>{Thread("A", "Alpha", 2), Thread("B", "Beta", 1)};
            }
            return std::vector<CodexThreadSummary>{Thread("C", "Charlie", 3), Thread("A", "Alpha renamed", 2), Thread("B", "Beta", 1)};
        },
        [&refreshes] { ++refreshes; }
    );
    const auto first = probe.Probe();
    const auto second = probe.Probe();
    const auto changed = probe.Probe();
    if (!first || *first || !second || *second || !changed || !*changed || refreshes != 1) {
        return 1;
    }
    const auto stable = probe.Probe();
    return stable && !*stable && refreshes == 1 ? 0 : 2;
}

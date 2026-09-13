// ============================================================================
// Codex Deck - Probe des changements de sessions externes
// ----------------------------------------------------------------------------
// Ce module compare un fingerprint recent et ne demande une sync exhaustive
// que lorsqu'un autre client a modifie le catalogue Codex.
// ============================================================================

#pragma once

#include "../codex/CodexClient.h"
#include "../model/SessionCatalog.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

// Calcule le fingerprint stable des threads dans leur ordre courant.
std::uint64_t RecentSessionFingerprint(std::span<const CodexThreadSummary> threads);

// Callback de refresh exhaustif utilise par le probe.
using FullSessionRefresh = std::move_only_function<void()>;

// Loader injectable du premier lot de threads recents.
using RecentThreadLoader = std::move_only_function<std::expected<std::vector<CodexThreadSummary>, CodexError>(ThreadListOptions)>;

// Compare les probes recents successifs.
class ExternalSessionProbe {
public:
    // Cree un probe depuis un loader et une action de refresh.
    ExternalSessionProbe(RecentThreadLoader loader, FullSessionRefresh refresh);

    // Execute un probe borne et retourne si une sync a ete demandee.
    std::expected<bool, CodexError> Probe();

    // Recale le fingerprint apres une sync exhaustive reussie.
    void ResetFromSnapshot(const SessionCatalogSnapshot& snapshot);

private:
    // Loader recent injectable.
    RecentThreadLoader loader_;
    // Action de sync exhaustive.
    FullSessionRefresh refresh_;
    // Dernier fingerprint accepte.
    std::optional<std::uint64_t> fingerprint_;
};

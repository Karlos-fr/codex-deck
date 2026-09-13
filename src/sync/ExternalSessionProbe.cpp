// ============================================================================
// Codex Deck - Implementation du probe de sessions externes
// ----------------------------------------------------------------------------
// Ce fichier utilise FNV-1a sur les champs stables sans lire les fichiers
// internes de Codex.
// ============================================================================

#include "ExternalSessionProbe.h"

#include <algorithm>

namespace {

// Offset initial FNV-1a 64 bits.
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
// Multiplicateur FNV-1a 64 bits.
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
// Nombre maximal de sessions incluses dans un fingerprint local.
constexpr std::size_t kRecentLimit = 100;

// Ajoute une chaine au hash FNV courant.
void HashText(std::uint64_t& hash, std::string_view text) {
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    hash ^= 0xFFU;
    hash *= kFnvPrime;
}

}  // namespace

// Calcule le fingerprint stable des threads dans leur ordre courant.
std::uint64_t RecentSessionFingerprint(std::span<const CodexThreadSummary> threads) {
    std::uint64_t hash = kFnvOffset;
    for (const CodexThreadSummary& thread : threads) {
        HashText(hash, thread.id);
        HashText(hash, thread.name);
        HashText(hash, thread.cwd.string());
        HashText(hash, std::to_string(thread.updated_at));
    }
    return hash;
}

// Cree un probe depuis un loader et une action de refresh.
ExternalSessionProbe::ExternalSessionProbe(RecentThreadLoader loader, FullSessionRefresh refresh)
    : loader_(std::move(loader)), refresh_(std::move(refresh)) {
}

// Execute un probe borne et retourne si une sync a ete demandee.
std::expected<bool, CodexError> ExternalSessionProbe::Probe() {
    ThreadListOptions options{};
    options.max_items = kRecentLimit;
    options.use_state_db_only = true;
    auto threads = loader_(options);
    if (!threads) {
        return std::unexpected(threads.error());
    }
    const std::uint64_t current = RecentSessionFingerprint(*threads);
    if (!fingerprint_) {
        fingerprint_ = current;
        return false;
    }
    if (*fingerprint_ == current) {
        return false;
    }
    fingerprint_ = current;
    if (refresh_) {
        refresh_();
    }
    return true;
}

// Recale le fingerprint apres une sync exhaustive reussie.
void ExternalSessionProbe::ResetFromSnapshot(const SessionCatalogSnapshot& snapshot) {
    std::vector<CodexThreadSummary> active;
    active.reserve(snapshot.sessions.size());
    for (const SessionRecord& session : snapshot.sessions) {
        if (!session.codex.archived && session.present_in_codex) {
            active.push_back(session.codex);
        }
    }
    std::ranges::stable_sort(active, [](const CodexThreadSummary& left, const CodexThreadSummary& right) {
        return left.updated_at > right.updated_at;
    });
    if (active.size() > kRecentLimit) {
        active.resize(kRecentLimit);
    }
    fingerprint_ = RecentSessionFingerprint(active);
}

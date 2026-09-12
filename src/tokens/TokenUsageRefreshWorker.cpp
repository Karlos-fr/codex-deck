// ============================================================================
// Codex Glass - Worker de rafraichissement des tokens locaux
// ----------------------------------------------------------------------------
// Ce fichier execute le scan et les transactions SQLite hors du thread UI,
// avec restauration du dernier snapshot valide lors d'une erreur temporaire.
// ============================================================================

#include "TokenUsageRefreshWorker.h"

#include "CodexSessionScanner.h"
#include "TokenUsageStore.h"
#include "../usage/UsageHistoryStore.h"

#include <chrono>
#include <exception>
#include <utility>

// ----------------------------------------------------------------------------
// Cree un worker injectable pour les tests.
// ----------------------------------------------------------------------------
TokenUsageRefreshWorker::TokenUsageRefreshWorker(
    std::filesystem::path codex_home,
    std::wstring database_path
) : codex_home_(std::move(codex_home)), database_path_(std::move(database_path)) {}

// ----------------------------------------------------------------------------
// Annule puis joint automatiquement tout scan actif.
// ----------------------------------------------------------------------------
TokenUsageRefreshWorker::~TokenUsageRefreshWorker() {
    Stop();
}

// ----------------------------------------------------------------------------
// Lance un scan de la couverture heatmap si aucun travail n'est actif.
// ----------------------------------------------------------------------------
bool TokenUsageRefreshWorker::Start(HWND hwnd) {
    if (thread_.joinable()) {
        return false;
    }
    cancellation_requested_.store(false);
    {
        const std::lock_guard lock(result_mutex_);
        result_.reset();
    }
    try {
        const std::filesystem::path codex_home = codex_home_;
        const std::wstring database_path = database_path_.empty()
            ? GetUsageHistoryDatabasePath()
            : database_path_;
        thread_ = std::thread([this, hwnd, codex_home, database_path]() {
            TokenUsageSnapshot snapshot{};
            TokenUsageStore store;
            try {
                TokenScanCache cache;
                if (store.Open(database_path)) {
                    store.LoadScanCache(cache);
                }
                const CodexSessionScanner scanner = codex_home.empty()
                    ? CodexSessionScanner()
                    : CodexSessionScanner(codex_home);
                snapshot = scanner.Scan(
                    std::chrono::system_clock::now(),
                    kTokenHeatmapCoverageDays,
                    &cancellation_requested_,
                    &cache
                );
                if (!cancellation_requested_.load() && store.Open(database_path)) {
                    store.Save(cache, snapshot);
                }
            } catch (const std::exception&) {
                if (store.Open(database_path)) {
                    store.LoadLatestSnapshot(snapshot);
                }
                snapshot.error_message = L"Scan local temporairement indisponible";
                snapshot.freshness = TokenUsageFreshness::Error;
            } catch (...) {
                if (store.Open(database_path)) {
                    store.LoadLatestSnapshot(snapshot);
                }
                snapshot.error_message = L"Erreur inconnue du scan local";
                snapshot.freshness = TokenUsageFreshness::Error;
            }
            if (!cancellation_requested_.load()) {
                const std::lock_guard lock(result_mutex_);
                result_ = std::move(snapshot);
                if (hwnd != nullptr) {
                    PostMessageW(hwnd, kTokenUsageRefreshCompletedMessage, 0, 0);
                }
            }
        });
        return true;
    } catch (...) {
        return false;
    }
}

// ----------------------------------------------------------------------------
// Indique si un scan est actuellement possede par le worker.
//
// Retour : true tant que le thread doit encore etre joint.
// ----------------------------------------------------------------------------
bool TokenUsageRefreshWorker::IsActive() const {
    return thread_.joinable();
}

// ----------------------------------------------------------------------------
// Joint le thread termine et transfere son resultat.
// ----------------------------------------------------------------------------
std::optional<TokenUsageSnapshot> TokenUsageRefreshWorker::TakeResult() {
    if (thread_.joinable()) {
        thread_.join();
    }
    const std::lock_guard lock(result_mutex_);
    std::optional<TokenUsageSnapshot> snapshot = std::move(result_);
    result_.reset();
    return snapshot;
}

// ----------------------------------------------------------------------------
// Demande l'annulation et joint le thread actif.
// ----------------------------------------------------------------------------
void TokenUsageRefreshWorker::Stop() {
    cancellation_requested_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    const std::lock_guard lock(result_mutex_);
    result_.reset();
}

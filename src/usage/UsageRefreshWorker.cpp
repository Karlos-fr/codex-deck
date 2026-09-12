// ============================================================================
// Codex Glass - Implementation du worker de rafraichissement d'usage
// ----------------------------------------------------------------------------
// Ce fichier execute le reseau et l'ecriture SQLite hors du thread UI. Chaque
// worker possede ses ressources afin de ne partager aucun handle entre threads.
// ============================================================================

#include "UsageRefreshWorker.h"

#include "UsageHistoryStore.h"
#include "UsageProviderFactory.h"

#include <exception>

// ----------------------------------------------------------------------------
// Cree un worker avec une fabrique injectable et un stockage configurable.
// ----------------------------------------------------------------------------
UsageRefreshWorker::UsageRefreshWorker(
    std::function<std::shared_ptr<IUsageProvider>()> provider_factory,
    bool record_history
) : provider_factory_(std::move(provider_factory)), record_history_(record_history) {}

// ----------------------------------------------------------------------------
// Attend la fin du traitement avant de detruire les ressources partagees.
// ----------------------------------------------------------------------------
UsageRefreshWorker::~UsageRefreshWorker() {
    Stop();
}

// ----------------------------------------------------------------------------
// Lance une recuperation et une historisation hors du thread UI.
//
// Parametres :
// - hwnd : fenetre qui recevra kUsageRefreshCompletedMessage.
//
// Retour :
// - true si le worker a ete lance.
// - false si un traitement est actif ou si le thread n'a pas pu etre cree.
// ----------------------------------------------------------------------------
bool UsageRefreshWorker::Start(HWND hwnd) {
    if (thread_.joinable()) {
        return false;
    }

    {
        const std::lock_guard lock(result_mutex_);
        result_.reset();
    }

    try {
        provider_ = provider_factory_ ? provider_factory_() : CreateUsageProvider();
        if (!provider_) {
            return false;
        }
        const std::shared_ptr<IUsageProvider> provider = provider_;
        const bool record_history = record_history_;
        thread_ = std::thread([this, hwnd, provider, record_history]() {
            UsageRefreshResult completed_result{};

            try {
                completed_result.snapshot = provider->FetchUsage();

                if (record_history) {
                    UsageHistoryStore history_store;
                    if (history_store.Open(GetUsageHistoryDatabasePath())) {
                        history_store.RecordSnapshot(completed_result.snapshot);
                    }
                }
            } catch (const std::exception&) {
                completed_result.failure = UsageRefreshFailure::StandardException;
            } catch (...) {
                completed_result.failure = UsageRefreshFailure::UnknownException;
            }

            {
                const std::lock_guard lock(result_mutex_);
                result_ = std::move(completed_result);
            }
            PostMessageW(hwnd, kUsageRefreshCompletedMessage, 0, 0);
        });
        return true;
    } catch (const std::exception&) {
        provider_.reset();
        return false;
    } catch (...) {
        provider_.reset();
        return false;
    }
}

// ----------------------------------------------------------------------------
// Attend un worker termine et transfere son resultat au thread UI.
//
// Retour :
// - resultat disponible, ou rien si aucune reponse n'a ete produite.
// ----------------------------------------------------------------------------
std::optional<UsageRefreshResult> UsageRefreshWorker::TakeResult() {
    if (thread_.joinable()) {
        thread_.join();
    }
    provider_.reset();

    const std::lock_guard lock(result_mutex_);
    std::optional<UsageRefreshResult> completed_result = std::move(result_);
    result_.reset();
    return completed_result;
}

// ----------------------------------------------------------------------------
// Attend la fin du traitement actif et libere le thread possede.
// ----------------------------------------------------------------------------
void UsageRefreshWorker::Stop() {
    if (provider_) {
        provider_->Cancel();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    provider_.reset();

    const std::lock_guard lock(result_mutex_);
    result_.reset();
}

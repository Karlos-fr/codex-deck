// ============================================================================
// Codex Glass - Worker de rafraichissement d'usage
// ----------------------------------------------------------------------------
// Ce fichier declare le travail asynchrone de recuperation et d'historisation.
// Il ne modifie jamais l'etat UI, transmis uniquement par message Win32.
// ============================================================================

#pragma once

#include "UsageSnapshot.h"

#include <windows.h>

#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <thread>

class IUsageProvider;

// Message prive qui restitue un resultat de rafraichissement au thread UI.
constexpr UINT kUsageRefreshCompletedMessage = WM_APP + 20;

// ----------------------------------------------------------------------------
// Nature d'une exception interceptee par le worker de rafraichissement.
// ----------------------------------------------------------------------------
enum class UsageRefreshFailure {
    None,
    StandardException,
    UnknownException,
};

// ----------------------------------------------------------------------------
// Resultat restitue au thread UI apres un rafraichissement d'usage.
// ----------------------------------------------------------------------------
struct UsageRefreshResult {
    // Snapshot retourne par le fournisseur lorsque l'appel aboutit.
    UsageSnapshot snapshot{};

    // Type d'echec exceptionnel rencontre pendant le travail asynchrone.
    UsageRefreshFailure failure = UsageRefreshFailure::None;
};

// ----------------------------------------------------------------------------
// Possede le thread de rafraichissement et son resultat jusqu'a sa restitution.
// ----------------------------------------------------------------------------
class UsageRefreshWorker {
public:
    // ------------------------------------------------------------------------
    // Cree un worker sans traitement actif.
    // ------------------------------------------------------------------------
    UsageRefreshWorker() = default;

    // ------------------------------------------------------------------------
    // Cree un worker avec une fabrique injectable et un stockage configurable.
    //
    // Parametres :
    // - provider_factory : fabrique utilisee a chaque lancement.
    // - record_history : autorise l'historisation SQLite du snapshot.
    // ------------------------------------------------------------------------
    UsageRefreshWorker(
        std::function<std::shared_ptr<IUsageProvider>()> provider_factory,
        bool record_history
    );

    // ------------------------------------------------------------------------
    // Attend la fin du traitement avant de detruire ses ressources partagees.
    // ------------------------------------------------------------------------
    ~UsageRefreshWorker();

    // ------------------------------------------------------------------------
    // Interdit la copie car le worker possede un thread unique.
    // ------------------------------------------------------------------------
    UsageRefreshWorker(const UsageRefreshWorker&) = delete;

    // ------------------------------------------------------------------------
    // Interdit l'affectation par copie car le worker possede un thread unique.
    // ------------------------------------------------------------------------
    UsageRefreshWorker& operator=(const UsageRefreshWorker&) = delete;

    // ------------------------------------------------------------------------
    // Lance une recuperation et une historisation hors du thread UI.
    //
    // Parametres :
    // - hwnd : fenetre qui recevra kUsageRefreshCompletedMessage.
    //
    // Retour :
    // - true si le worker a ete lance.
    // - false si un traitement est actif ou si le thread n'a pas pu etre cree.
    // ------------------------------------------------------------------------
    bool Start(HWND hwnd);

    // ------------------------------------------------------------------------
    // Attend un worker termine et transfere son resultat au thread UI.
    //
    // Retour :
    // - resultat disponible, ou rien si aucune reponse n'a ete produite.
    // ------------------------------------------------------------------------
    std::optional<UsageRefreshResult> TakeResult();

    // ------------------------------------------------------------------------
    // Attend la fin du traitement actif et libere le thread possede.
    // ------------------------------------------------------------------------
    void Stop();

private:
    // Fabrique optionnelle injectee, principalement utile aux tests et futurs providers.
    std::function<std::shared_ptr<IUsageProvider>()> provider_factory_{};

    // Indique si le worker doit historiser le snapshot distant.
    bool record_history_ = true;

    // Protege le resultat partage entre le worker et le thread UI.
    std::mutex result_mutex_;

    // Resultat complet en attente de restitution au thread UI.
    std::optional<UsageRefreshResult> result_;

    // Thread possede pendant toute la duree du rafraichissement.
    std::thread thread_;

    // Fournisseur generique partage avec Stop pour annuler son I/O active.
    std::shared_ptr<IUsageProvider> provider_;
};

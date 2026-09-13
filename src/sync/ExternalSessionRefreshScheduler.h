// ============================================================================
// Codex Deck - Scheduler de refresh des sessions externes
// ----------------------------------------------------------------------------
// Ce module decide quand lancer un probe sans executer de RPC ni lire l'horloge
// globale, afin de rester entierement deterministe en test.
// ============================================================================

#pragma once

#include <chrono>

// Planifie et coalesce les probes de sessions externes.
class ExternalSessionRefreshScheduler {
public:
    // Type de temps monotone injecte par l'appelant.
    using TimePoint = std::chrono::steady_clock::time_point;

    // Cree le scheduler avec son etat initial de foreground.
    explicit ExternalSessionRefreshScheduler(bool foreground = true);

    // Change l'etat d'activation et demande un probe au retour foreground.
    void SetForeground(bool foreground, TimePoint now);

    // Demande un probe des que possible.
    void RequestImmediateProbe();

    // Commence un probe si une echeance est atteinte et aucun autre actif.
    bool TryBeginProbe(TimePoint now);

    // Termine le probe actif et conserve une demande coalescee eventuelle.
    void CompleteProbe(TimePoint now);

private:
    // Retourne l'intervalle correspondant a l'etat courant.
    std::chrono::seconds CurrentInterval() const;

    // Indique si l'application est au premier plan.
    bool foreground_ = true;
    // Indique qu'un probe est actuellement actif.
    bool probe_active_ = false;
    // Indique qu'une nouvelle demande attend la fin du probe actif.
    bool immediate_requested_ = false;
    // Prochaine echeance periodique.
    TimePoint next_probe_{};
    // Indique que la premiere echeance a ete initialisee.
    bool initialized_ = false;
};

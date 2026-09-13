// ============================================================================
// Codex Deck - Implementation du scheduler de refresh externe
// ----------------------------------------------------------------------------
// Ce fichier applique les intervalles foreground/background et garantit qu'un
// seul probe est actif a la fois.
// ============================================================================

#include "ExternalSessionRefreshScheduler.h"

namespace {

// Intervalle de probe au premier plan.
constexpr auto kForegroundInterval = std::chrono::seconds(15);
// Intervalle de probe en arriere-plan.
constexpr auto kBackgroundInterval = std::chrono::seconds(60);

}  // namespace

// Cree le scheduler avec son etat initial de foreground.
ExternalSessionRefreshScheduler::ExternalSessionRefreshScheduler(bool foreground)
    : foreground_(foreground) {
}

// Change l'etat d'activation et demande un probe au retour foreground.
void ExternalSessionRefreshScheduler::SetForeground(bool foreground, TimePoint now) {
    const bool activated = !foreground_ && foreground;
    foreground_ = foreground;
    if (activated) {
        immediate_requested_ = true;
    }
    next_probe_ = now + CurrentInterval();
    initialized_ = true;
}

// Demande un probe des que possible.
void ExternalSessionRefreshScheduler::RequestImmediateProbe() {
    immediate_requested_ = true;
}

// Commence un probe si une echeance est atteinte et aucun autre actif.
bool ExternalSessionRefreshScheduler::TryBeginProbe(TimePoint now) {
    if (!initialized_) {
        next_probe_ = now + CurrentInterval();
        initialized_ = true;
    }
    if (probe_active_) {
        if (now >= next_probe_) {
            immediate_requested_ = true;
        }
        return false;
    }
    if (!immediate_requested_ && now < next_probe_) {
        return false;
    }
    probe_active_ = true;
    immediate_requested_ = false;
    next_probe_ = now + CurrentInterval();
    return true;
}

// Termine le probe actif et conserve une demande coalescee eventuelle.
void ExternalSessionRefreshScheduler::CompleteProbe(TimePoint now) {
    probe_active_ = false;
    if (!immediate_requested_) {
        next_probe_ = now + CurrentInterval();
    }
}

// Retourne l'intervalle correspondant a l'etat courant.
std::chrono::seconds ExternalSessionRefreshScheduler::CurrentInterval() const {
    return foreground_ ? kForegroundInterval : kBackgroundInterval;
}

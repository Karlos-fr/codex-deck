// ============================================================================
// Codex Deck - Tests du scheduler de refresh externe
// ----------------------------------------------------------------------------
// Ce fichier valide intervalles, activation immediate et coalescing sans timer.
// ============================================================================

#include "sync/ExternalSessionRefreshScheduler.h"

// Verifie les echeances foreground/background et l'absence de chevauchement.
int main() {
    using namespace std::chrono_literals;
    const ExternalSessionRefreshScheduler::TimePoint start{};
    ExternalSessionRefreshScheduler scheduler(true);
    if (scheduler.TryBeginProbe(start) || scheduler.TryBeginProbe(start + 14s)) {
        return 1;
    }
    if (!scheduler.TryBeginProbe(start + 15s) || scheduler.TryBeginProbe(start + 30s)) {
        return 2;
    }
    scheduler.CompleteProbe(start + 15s);
    if (!scheduler.TryBeginProbe(start + 30s)) {
        return 3;
    }
    scheduler.CompleteProbe(start + 30s);
    scheduler.SetForeground(false, start + 31s);
    if (scheduler.TryBeginProbe(start + 90s) || !scheduler.TryBeginProbe(start + 91s)) {
        return 4;
    }
    scheduler.RequestImmediateProbe();
    if (scheduler.TryBeginProbe(start + 92s)) {
        return 5;
    }
    scheduler.CompleteProbe(start + 92s);
    if (!scheduler.TryBeginProbe(start + 92s)) {
        return 6;
    }
    scheduler.CompleteProbe(start + 92s);
    scheduler.SetForeground(true, start + 100s);
    return scheduler.TryBeginProbe(start + 100s) ? 0 : 7;
}

// ============================================================================
// Codex Glass - Controleur de declenchement de vibration
// ----------------------------------------------------------------------------
// Ce fichier declare la logique metier qui decide si un nouveau releve d'usage
// doit declencher une vibration du widget.
// ============================================================================

#pragma once

#include "../usage/UsageSnapshot.h"
#include "../motion/WidgetMotionEffectsSettings.h"

#include <chrono>
#include <optional>

// ----------------------------------------------------------------------------
// Decide quand la vibration du widget doit etre declenchee.
// ----------------------------------------------------------------------------
class WidgetVibrationController {
public:
    // ------------------------------------------------------------------------
    // Observe un releve avec les reglages communs des Effets Motion.
    // ------------------------------------------------------------------------
    bool ObserveSnapshot(
        const WidgetMotionEffectsSettings& settings,
        const UsageSnapshot& snapshot,
        std::chrono::system_clock::time_point now
    );

    // ------------------------------------------------------------------------
    // Oublie l'historique local du controleur.
    // ------------------------------------------------------------------------
    void Reset();

private:
    std::optional<UsageSnapshot> last_snapshot_;
    std::optional<std::chrono::system_clock::time_point> last_triggered_at_;
};

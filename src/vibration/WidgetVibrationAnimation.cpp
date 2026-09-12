// ============================================================================
// Codex Glass - Implementation de la timeline de vibration
// ----------------------------------------------------------------------------
// Ce fichier calcule une progression et une intensite amortie, sans dependance
// Win32 ni effet visuel direct.
// ============================================================================

#include "WidgetVibrationAnimation.h"

#include <algorithm>
#include <cmath>

namespace {

// Duree par defaut de la vibration.
constexpr auto kDefaultVibrationDuration = std::chrono::milliseconds{200};

// Duree minimale acceptee pour eviter une timeline nulle.
constexpr auto kMinimumVibrationDuration = std::chrono::milliseconds{1};

// Exposant du fondu de sortie de l'intensite.
constexpr double kDampingPower = 1.65;

// Nombre d'oscillations virtuelles utilisees par l'intensite partagee.
constexpr double kOscillationCount = 3.0;

// Approximation locale de pi pour garder le module autonome.
constexpr double kPi = 3.14159265358979323846;

// ----------------------------------------------------------------------------
// Convertit une duree en secondes decimales.
//
// Parametres :
// - duration : duree a convertir.
//
// Retour :
// - duree exprimee en secondes.
// ----------------------------------------------------------------------------
double Seconds(WidgetVibrationAnimation::Duration duration) {
    return std::chrono::duration<double>(duration).count();
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree une timeline avec sa duree par defaut.
// ----------------------------------------------------------------------------
WidgetVibrationAnimation::WidgetVibrationAnimation()
    : WidgetVibrationAnimation(kDefaultVibrationDuration) {
}

// ----------------------------------------------------------------------------
// Cree une timeline avec une duree explicite.
//
// Parametres :
// - duration : duree totale de l'animation.
// ----------------------------------------------------------------------------
WidgetVibrationAnimation::WidgetVibrationAnimation(Duration duration)
    : duration_(std::max(duration, std::chrono::duration_cast<Duration>(kMinimumVibrationDuration))) {
}

// ----------------------------------------------------------------------------
// Demarre ou redemarre l'animation.
//
// Parametres :
// - now : instant de depart.
// ----------------------------------------------------------------------------
void WidgetVibrationAnimation::Start(TimePoint now) {
    started_at_ = now;
    started_ = true;
}

// ----------------------------------------------------------------------------
// Demarre ou redemarre l'animation avec une duree explicite.
//
// Parametres :
// - now : instant de depart.
// - duration : duree totale de l'animation.
// ----------------------------------------------------------------------------
void WidgetVibrationAnimation::Start(TimePoint now, Duration duration) {
    duration_ = std::max(duration, std::chrono::duration_cast<Duration>(kMinimumVibrationDuration));
    Start(now);
}

// ----------------------------------------------------------------------------
// Indique si l'animation est encore active.
//
// Parametres :
// - now : instant courant.
//
// Retour :
// - true si la timeline est demarree et non terminee.
// - false sinon.
// ----------------------------------------------------------------------------
bool WidgetVibrationAnimation::IsActive(TimePoint now) const {
    return started_ && now >= started_at_ && now - started_at_ < duration_;
}

// ----------------------------------------------------------------------------
// Retourne la progression normalisee de la timeline.
//
// Parametres :
// - now : instant courant.
//
// Retour :
// - progression bornee entre 0 et 1.
// ----------------------------------------------------------------------------
double WidgetVibrationAnimation::Progress(TimePoint now) const {
    if (!started_ || now <= started_at_) {
        return 0.0;
    }

    return std::clamp(Seconds(now - started_at_) / Seconds(duration_), 0.0, 1.0);
}

// ----------------------------------------------------------------------------
// Retourne la phase d'oscillation a vitesse stable.
//
// Parametres :
// - now : instant courant.
//
// Retour :
// - progression equivalente basee sur la duree par defaut.
// ----------------------------------------------------------------------------
double WidgetVibrationAnimation::OscillationPhase(TimePoint now) const {
    if (!started_ || now <= started_at_) {
        return 0.0;
    }

    return Seconds(now - started_at_) / Seconds(std::chrono::duration_cast<Duration>(kDefaultVibrationDuration));
}

// ----------------------------------------------------------------------------
// Retourne l'intensite amortie de l'animation.
//
// Parametres :
// - now : instant courant.
//
// Retour :
// - intensite bornee entre 0 et 1, egale a 0 quand l'animation est terminee.
// ----------------------------------------------------------------------------
double WidgetVibrationAnimation::Intensity(TimePoint now) const {
    if (!IsActive(now)) {
        return 0.0;
    }

    const double progress = Progress(now);
    const double damping = std::pow(1.0 - progress, kDampingPower);
    const double oscillation = std::abs(std::sin(OscillationPhase(now) * kOscillationCount * kPi));
    return std::clamp(damping * oscillation, 0.0, 1.0);
}

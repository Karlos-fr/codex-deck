// ============================================================================
// Codex Glass - Timeline de vibration du widget
// ----------------------------------------------------------------------------
// Ce fichier declare une animation courte et partagee par les futurs moteurs de
// vibration du widget.
// ============================================================================

#pragma once

#include <chrono>

// ----------------------------------------------------------------------------
// Calcule une timeline de vibration amortie et bornee.
// ----------------------------------------------------------------------------
class WidgetVibrationAnimation {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    // ------------------------------------------------------------------------
    // Cree une timeline avec sa duree par defaut.
    // ------------------------------------------------------------------------
    WidgetVibrationAnimation();

    // ------------------------------------------------------------------------
    // Cree une timeline avec une duree explicite.
    //
    // Parametres :
    // - duration : duree totale de l'animation.
    // ------------------------------------------------------------------------
    explicit WidgetVibrationAnimation(Duration duration);

    // ------------------------------------------------------------------------
    // Demarre ou redemarre l'animation.
    //
    // Parametres :
    // - now : instant de depart.
    // ------------------------------------------------------------------------
    void Start(TimePoint now);

    // ------------------------------------------------------------------------
    // Demarre ou redemarre l'animation avec une duree explicite.
    //
    // Parametres :
    // - now : instant de depart.
    // - duration : duree totale de l'animation.
    // ------------------------------------------------------------------------
    void Start(TimePoint now, Duration duration);

    // ------------------------------------------------------------------------
    // Indique si l'animation est encore active.
    //
    // Parametres :
    // - now : instant courant.
    //
    // Retour :
    // - true si la timeline est demarree et non terminee.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool IsActive(TimePoint now) const;

    // ------------------------------------------------------------------------
    // Retourne la progression normalisee de la timeline.
    //
    // Parametres :
    // - now : instant courant.
    //
    // Retour :
    // - progression bornee entre 0 et 1.
    // ------------------------------------------------------------------------
    double Progress(TimePoint now) const;

    // ------------------------------------------------------------------------
    // Retourne la phase d'oscillation a vitesse stable.
    //
    // Parametres :
    // - now : instant courant.
    //
    // Retour :
    // - progression equivalente basee sur la duree par defaut.
    // ------------------------------------------------------------------------
    double OscillationPhase(TimePoint now) const;

    // ------------------------------------------------------------------------
    // Retourne l'intensite amortie de l'animation.
    //
    // Parametres :
    // - now : instant courant.
    //
    // Retour :
    // - intensite bornee entre 0 et 1, egale a 0 quand l'animation est terminee.
    // ------------------------------------------------------------------------
    double Intensity(TimePoint now) const;

private:
    Duration duration_{};
    TimePoint started_at_{};
    bool started_ = false;
};

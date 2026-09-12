// ============================================================================
// Codex Glass - Moteur de micro-deplacement du widget
// ----------------------------------------------------------------------------
// Ce fichier declare l'effet physique de vibration. Il applique uniquement un
// petit decalage temporaire de fenetre et sait revenir au rectangle stable.
// ============================================================================

#pragma once

#include "../motion/WidgetMotionEffectsSettings.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Applique un micro-deplacement horizontal temporaire a une fenetre.
// ----------------------------------------------------------------------------
class WidgetVibrationMotion {
public:
    // ------------------------------------------------------------------------
    // Capture le rectangle stable avec les axes et la frequence configurables.
    // ------------------------------------------------------------------------
    void Start(const RECT& window_rect, const WidgetMotionVibrationSettings& settings);

    // ------------------------------------------------------------------------
    // Applique l'offset correspondant a la progression courante.
    //
    // Parametres :
    // - hwnd : fenetre a deplacer.
    // - progress : progression normalisee entre 0 et 1.
    // - intensity : intensite amortie entre 0 et 1.
    //
    // Retour :
    // - true si le deplacement a ete applique ou n'etait pas necessaire.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool Apply(HWND hwnd, double progress, double intensity);

    // ------------------------------------------------------------------------
    // Replace la fenetre sur son rectangle stable.
    //
    // Parametres :
    // - hwnd : fenetre a replacer.
    //
    // Retour :
    // - true si le retour a ete applique ou n'etait pas necessaire.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool Finish(HWND hwnd);

    // ------------------------------------------------------------------------
    // Indique si un offset temporaire est actuellement applique.
    //
    // Retour :
    // - true si la fenetre est decalee par le moteur.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool HasTemporaryOffset() const;

    // ------------------------------------------------------------------------
    // Retourne le rectangle stable deduit de la position courante.
    //
    // Parametres :
    // - hwnd : fenetre dont la position courante doit etre observee.
    //
    // Retour :
    // - rectangle stable sans offset temporaire.
    // ------------------------------------------------------------------------
    RECT StableRect(HWND hwnd) const;

private:
    RECT stable_rect_{};
    double intensity_scale_ = 1.0;
    double frequency_scale_ = 1.0;
    int current_offset_x_ = 0;
    int current_offset_y_ = 0;
    bool horizontal_enabled_ = true;
    bool vertical_enabled_ = false;
    bool started_ = false;
};

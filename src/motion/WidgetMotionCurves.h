// ============================================================================
// Codex Glass - Courbes des Effets Motion
// ----------------------------------------------------------------------------
// Ce fichier expose les enveloppes temporelles pures de Vibration et Pulse.
// Il ne depend ni de Win32 ni du renderer.
// ============================================================================

#pragma once

#include "WidgetMotionEffectsSettings.h"

// Regroupe l'offset physique bidimensionnel d'une frame de Vibration.
struct WidgetMotionOffset {
    // Decalage horizontal en pixels.
    int x = 0;

    // Decalage vertical en pixels.
    int y = 0;
};

// Calcule l'extinction de Vibration.
// Parametres : progression normalisee et amortissement en pourcentage.
// Retour : enveloppe bornee entre zero et un.
double WidgetMotionVibrationEnvelope(double progress, int damping_percent);

// Calcule l'enveloppe de Pulse.
// Parametres : progression, repetitions et douceur en pourcentage.
// Retour : intensite bornee entre zero et un.
double WidgetMotionPulseEnvelope(
    double progress,
    WidgetMotionPulseRepetitions repetitions,
    int softness_percent
);

// Calcule la progression locale de la pulsation courante.
// Parametres : progression globale et nombre de repetitions.
// Retour : progression bornee entre zero et un pour le cycle courant.
double WidgetMotionPulseCycleProgress(
    double progress,
    WidgetMotionPulseRepetitions repetitions
);

// Calcule l'offset physique borne de Vibration pour une frame.
// Parametres : phase non bornee, enveloppe et reglages de Vibration.
// Retour : decalage entier horizontal et vertical.
WidgetMotionOffset EvaluateWidgetMotionOffset(
    double oscillation_progress,
    double envelope,
    const WidgetMotionVibrationSettings& settings
);

// ============================================================================
// Codex Glass - Courbes des Effets Motion
// ----------------------------------------------------------------------------
// Ce fichier implemente les enveloppes temporelles pures et bornees utilisees
// par l'orchestrateur et le rendu des Effets Motion.
// ============================================================================

#include "WidgetMotionCurves.h"

#include <algorithm>
#include <cmath>

namespace {

// Valeur locale de pi utilisee pour les oscillations de Vibration.
constexpr double kPi = 3.14159265358979323846;

// Amplitude maximale du deplacement physique en pixels.
constexpr double kMaximumMotionAmplitudePx = 3.0;

// Nombre de cycles de reference de Vibration.
constexpr double kMotionOscillationCount = 3.0;

}  // namespace

// Calcule l'extinction de Vibration a partir d'une progression et d'un amortissement.
double WidgetMotionVibrationEnvelope(double progress, int damping_percent) {
    const double safe_progress = std::clamp(progress, 0.0, 1.0);
    const double damping = static_cast<double>(std::clamp(damping_percent, 0, 100)) / 100.0;
    return std::pow(1.0 - safe_progress, 0.15 + (2.4 * damping));
}

// ----------------------------------------------------------------------------
// Calcule l'enveloppe asymetrique de Pulse.
//
// Parametres :
// - progress : progression globale de l'animation.
// - repetitions : nombre de pulsations dans la timeline.
// - softness_percent : douceur de l'attaque et de l'extinction.
//
// Retour :
// - intensite bornee, avec une attenuation legere apres chaque repetition.
// ----------------------------------------------------------------------------
double WidgetMotionPulseEnvelope(
    double progress,
    WidgetMotionPulseRepetitions repetitions,
    int softness_percent
) {
    if (progress < 0.0 || progress >= 1.0) {
        return 0.0;
    }
    const int repetition_count = static_cast<int>(repetitions);
    const double scaled_progress = progress * static_cast<double>(repetition_count);
    const int cycle_index = std::min(
        static_cast<int>(std::floor(scaled_progress)),
        repetition_count - 1
    );
    const double cycle_progress = scaled_progress - std::floor(scaled_progress);
    const double softness = static_cast<double>(std::clamp(softness_percent, 0, 100)) / 100.0;
    const double attack_end = 0.14 + (0.12 * softness);
    double envelope = 0.0;
    if (cycle_progress < attack_end) {
        const double attack = cycle_progress / attack_end;
        envelope = attack * attack * (3.0 - (2.0 * attack));
    } else {
        const double decay = (cycle_progress - attack_end) / (1.0 - attack_end);
        envelope = std::pow(1.0 - decay, 1.7 - softness);
    }
    const double repetition_attenuation = std::pow(0.86, static_cast<double>(cycle_index));
    return std::clamp(envelope * repetition_attenuation, 0.0, 1.0);
}

// ----------------------------------------------------------------------------
// Calcule la progression locale de la pulsation courante.
//
// Parametres :
// - progress : progression globale de l'animation.
// - repetitions : nombre de pulsations dans la timeline.
//
// Retour :
// - progression du cycle courant entre zero et un.
// ----------------------------------------------------------------------------
double WidgetMotionPulseCycleProgress(
    double progress,
    WidgetMotionPulseRepetitions repetitions
) {
    const double safe_progress = std::clamp(progress, 0.0, 1.0);
    if (safe_progress >= 1.0) {
        return 1.0;
    }
    const double scaled_progress = safe_progress * static_cast<double>(repetitions);
    return scaled_progress - std::floor(scaled_progress);
}

// Calcule l'offset physique borne selon la phase, l'enveloppe et les reglages fournis.
WidgetMotionOffset EvaluateWidgetMotionOffset(
    double oscillation_progress,
    double envelope,
    const WidgetMotionVibrationSettings& settings
) {
    const double progress = std::max(0.0, oscillation_progress);
    const double safe_envelope = std::clamp(envelope, 0.0, 1.0);
    const double intensity = static_cast<double>(std::clamp(settings.intensity_percent, 25, 300)) / 100.0;
    const double frequency = static_cast<double>(std::clamp(settings.frequency_percent, 25, 300)) / 100.0;
    const double phase = progress * kMotionOscillationCount * frequency * 2.0 * kPi;
    const auto sample = [&](double phase_offset) {
        return static_cast<int>(std::lround(
            std::sin(phase + phase_offset) * safe_envelope * intensity * kMaximumMotionAmplitudePx
        ));
    };
    return WidgetMotionOffset{
        settings.horizontal_enabled ? sample(0.0) : 0,
        settings.vertical_enabled ? sample(kPi * 0.5) : 0,
    };
}

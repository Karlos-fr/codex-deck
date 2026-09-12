// ============================================================================
// Codex Glass - Reglages des Effets Motion
// ----------------------------------------------------------------------------
// Ce fichier declare le modele cumulable des effets Vibration, Pulse et Wave.
// La persistance INI et la migration legacy restent isolees du rendu et du menu.
// ============================================================================

#pragma once

#include "../vibration/WidgetVibrationSettings.h"

#include <string>

// ----------------------------------------------------------------------------
// Liste les directions de propagation proposees pour Wave.
// ----------------------------------------------------------------------------
enum class WidgetMotionWaveDirection {
    Left,
    Right,
    Up,
    Down,
    Radial,
};

// ----------------------------------------------------------------------------
// Liste les nombres de pulsations proposes pour Pulse.
// ----------------------------------------------------------------------------
enum class WidgetMotionPulseRepetitions {
    One = 1,
    Two = 2,
    Three = 3,
};

// ----------------------------------------------------------------------------
// Regroupe les reglages propres a l'effet Vibration.
// ----------------------------------------------------------------------------
struct WidgetMotionVibrationSettings {
    // Active l'effet Vibration dans la composition Motion.
    bool enabled = true;

    // Intensite du deplacement physique en pourcentage.
    int intensity_percent = 100;

    // Duree totale de l'effet en millisecondes.
    int duration_ms = 200;

    // Active les oscillations horizontales.
    bool horizontal_enabled = true;

    // Active les oscillations verticales.
    bool vertical_enabled = false;

    // Frequence relative des oscillations en pourcentage.
    int frequency_percent = 100;

    // Force de refraction Glass independante du mouvement physique.
    int refraction_percent = 100;

    // Progressivite de l'extinction entre 0 et 100 pourcent.
    int damping_percent = 100;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages propres a l'effet Pulse.
// ----------------------------------------------------------------------------
struct WidgetMotionPulseSettings {
    // Active l'effet Pulse dans la composition Motion.
    bool enabled = false;

    // Intensite visuelle du pulse en pourcentage.
    int intensity_percent = 100;

    // Duree totale de l'effet en millisecondes.
    int duration_ms = 600;

    // Nombre de pulsations executees pendant la duree totale.
    WidgetMotionPulseRepetitions repetitions = WidgetMotionPulseRepetitions::One;

    // Douceur de l'attaque et du retour entre 0 et 100 pourcent.
    int softness_percent = 70;

    // Propagation du contour vers l'interieur entre 0 et 100 pourcent.
    int extent_percent = 35;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages propres a l'effet Wave.
// ----------------------------------------------------------------------------
struct WidgetMotionWaveSettings {
    // Active l'effet Wave dans la composition Motion.
    bool enabled = false;

    // Amplitude du champ de vague en pourcentage.
    int intensity_percent = 100;

    // Duree totale de l'effet en millisecondes.
    int duration_ms = 1200;

    // Direction de propagation du champ de vague.
    WidgetMotionWaveDirection direction = WidgetMotionWaveDirection::Right;

    // Vitesse relative de propagation en pourcentage.
    int speed_percent = 100;

    // Largeur relative entre les cretes en pourcentage.
    int wavelength_percent = 100;

    // Nombre de cretes successives dans le train d'ondes.
    int wave_count = 2;

    // Extinction spatiale et temporelle entre 0 et 100 pourcent.
    int damping_percent = 70;

    // Force de deformation optique en pourcentage.
    int refraction_percent = 100;

    // Ondulation transversale du front de vague en pourcentage.
    int front_undulation_percent = 45;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages persistants de toute la famille Effets Motion.
// ----------------------------------------------------------------------------
struct WidgetMotionEffectsSettings {
    // Active globalement le declenchement des Effets Motion.
    bool enabled = false;

    // Declenche les effets lors d'une baisse du quota restant.
    bool trigger_on_usage_drop = true;

    // Declenche les effets lorsque le quota restant repasse a cent pour cent.
    bool trigger_on_quota_reset = true;

    // Baisse minimale de quota necessaire pour declencher les effets.
    int usage_drop_threshold_percent = 1;

    // Delai minimal entre deux declenchements en secondes.
    int minimum_interval_seconds = 60;

    // Affiche les commandes de test internes dans le menu contextuel.
    bool debug_menu_enabled = true;

    // Reglages de l'effet Vibration.
    WidgetMotionVibrationSettings vibration{};

    // Reglages de l'effet Pulse.
    WidgetMotionPulseSettings pulse{};

    // Reglages de l'effet Wave.
    WidgetMotionWaveSettings wave{};
};

// ----------------------------------------------------------------------------
// Restaure les parametres Vibration par defaut en conservant son activation.
//
// Parametres :
// - current : reglages dont seul l'etat active doit etre conserve.
//
// Retour :
// - reglages Vibration par defaut avec le meme etat active.
// ----------------------------------------------------------------------------
WidgetMotionVibrationSettings ResetWidgetMotionVibrationSettings(
    const WidgetMotionVibrationSettings& current
);

// ----------------------------------------------------------------------------
// Genere des reglages Vibration aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
WidgetMotionVibrationSettings RandomizeWidgetMotionVibrationSettings(
    const WidgetMotionVibrationSettings& current
);

// ----------------------------------------------------------------------------
// Restaure les parametres Pulse par defaut en conservant son activation.
//
// Parametres :
// - current : reglages dont seul l'etat active doit etre conserve.
//
// Retour :
// - reglages Pulse par defaut avec le meme etat active.
// ----------------------------------------------------------------------------
WidgetMotionPulseSettings ResetWidgetMotionPulseSettings(
    const WidgetMotionPulseSettings& current
);

// ----------------------------------------------------------------------------
// Genere des reglages Pulse aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
WidgetMotionPulseSettings RandomizeWidgetMotionPulseSettings(
    const WidgetMotionPulseSettings& current
);

// ----------------------------------------------------------------------------
// Restaure les parametres Wave par defaut en conservant son activation.
//
// Parametres :
// - current : reglages dont seul l'etat active doit etre conserve.
//
// Retour :
// - reglages Wave par defaut avec le meme etat active.
// ----------------------------------------------------------------------------
WidgetMotionWaveSettings ResetWidgetMotionWaveSettings(
    const WidgetMotionWaveSettings& current
);

// ----------------------------------------------------------------------------
// Genere des reglages Wave aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
WidgetMotionWaveSettings RandomizeWidgetMotionWaveSettings(
    const WidgetMotionWaveSettings& current
);

// ----------------------------------------------------------------------------
// Valide et borne tous les reglages des Effets Motion.
//
// Parametres :
// - settings : reglages a normaliser.
//
// Retour :
// - copie corrigee et utilisable par le menu et les moteurs.
// ----------------------------------------------------------------------------
WidgetMotionEffectsSettings NormalizeWidgetMotionEffectsSettings(
    WidgetMotionEffectsSettings settings
);

// ----------------------------------------------------------------------------
// Convertit les anciens reglages exclusifs vers le modele cumulable.
//
// Parametres :
// - legacy : anciens reglages de vibration deja normalises ou bruts.
//
// Retour :
// - nouvelle configuration conservant le style ancien selectionne.
// ----------------------------------------------------------------------------
WidgetMotionEffectsSettings MigrateLegacyWidgetVibrationSettings(
    const WidgetVibrationSettings& legacy
);

// ----------------------------------------------------------------------------
// Charge les Effets Motion depuis un fichier INI ou migre les valeurs legacy.
//
// Parametres :
// - path : chemin du fichier INI.
// - legacy : valeurs anciennes utilisees en absence du nouveau schema.
//
// Retour :
// - reglages normalises issus du nouveau schema ou de la migration.
// ----------------------------------------------------------------------------
WidgetMotionEffectsSettings LoadWidgetMotionEffectsSettings(
    const std::wstring& path,
    const WidgetVibrationSettings& legacy
);

// ----------------------------------------------------------------------------
// Sauvegarde les Effets Motion et leur version de schema dans un fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - settings : reglages a persister.
//
// Retour :
// - true si toutes les ecritures ont reussi.
// ----------------------------------------------------------------------------
bool SaveWidgetMotionEffectsSettings(
    const std::wstring& path,
    const WidgetMotionEffectsSettings& settings
);

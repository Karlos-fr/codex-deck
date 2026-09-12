// ============================================================================
// Codex Glass - Reglages de vibration du widget
// ----------------------------------------------------------------------------
// Ce fichier declare les options persistantes de la vibration du widget, sans
// declencher ni rendre l'animation.
// ============================================================================

#pragma once

#include <string>

// ----------------------------------------------------------------------------
// Liste les styles de vibration proposes par l'application.
// ----------------------------------------------------------------------------
enum class WidgetVibrationStyle {
    None,
    Glass,
    Pulse,
    Wave,
};

// ----------------------------------------------------------------------------
// Regroupe les reglages persistants de vibration du widget.
// ----------------------------------------------------------------------------
struct WidgetVibrationSettings {
    // Active le declenchement global de la vibration.
    bool enabled = false;

    // Active le micro-deplacement physique de la fenetre.
    bool motion_enabled = true;

    // Active la variation visuelle du rendu GlassEffect.
    bool glass_effect_enabled = true;

    // Baisse minimale de quota restant necessaire pour declencher la vibration.
    int usage_drop_threshold_percent = 5;

    // Delai minimal entre deux vibrations consecutives.
    int minimum_interval_seconds = 60;

    // Intensite globale appliquee aux moteurs de vibration.
    int intensity_percent = 100;

    // Duree totale de la vibration en millisecondes.
    int duration_ms = 200;

    // Style visuel utilise par les moteurs de vibration.
    WidgetVibrationStyle style = WidgetVibrationStyle::Glass;
};

// ----------------------------------------------------------------------------
// Retourne le style de vibration utilise quand aucun reglage valide n'existe.
//
// Retour :
// - style de vibration par defaut.
// ----------------------------------------------------------------------------
WidgetVibrationStyle DefaultWidgetVibrationStyle();

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en style de vibration.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - style correspondant, ou style par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
WidgetVibrationStyle WidgetVibrationStyleFromString(const std::wstring& value);

// ----------------------------------------------------------------------------
// Convertit un style de vibration en chaine persistante.
//
// Parametres :
// - style : style a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring WidgetVibrationStyleToString(WidgetVibrationStyle style);

// ----------------------------------------------------------------------------
// Valide et borne les reglages de vibration.
//
// Parametres :
// - settings : reglages a normaliser.
//
// Retour :
// - reglages corriges avec bornes robustes.
// ----------------------------------------------------------------------------
WidgetVibrationSettings NormalizeWidgetVibrationSettings(WidgetVibrationSettings settings);

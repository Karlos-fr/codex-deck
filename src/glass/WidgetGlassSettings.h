// ============================================================================
// Codex Glass - Reglages de l'effet glass
// ----------------------------------------------------------------------------
// Ce fichier declare les modes glass selectionnables et leurs helpers de
// conversion/persistance, sans appliquer d'effet visuel.
// ============================================================================

#pragma once

#include <string>

// ----------------------------------------------------------------------------
// Liste les modes d'effet glass proposes par l'application.
// ----------------------------------------------------------------------------
enum class GlassEffectMode {
    Off,
    Enabled,
};

// ----------------------------------------------------------------------------
// Liste les profils de rendu GlassEffect proposes par l'application.
// ----------------------------------------------------------------------------
enum class GlassEffectPreset {
    Clear,
    Frosted,
    Subtle,
    Strong,
    Custom,
};

// ----------------------------------------------------------------------------
// Regroupe les reglages persistants de l'apparence permanente du verre.
//
// Le rayon des coins reste interne au renderer : il depend de la geometrie de
// la fenetre et ne constitue pas un reglage utilisateur.
// ----------------------------------------------------------------------------
struct GlassEffectAppearanceSettings {
    // Intensite de diffusion du fond capture en pourcentage.
    int diffusion_percent = 100;

    // Intensite de la teinte du theme en pourcentage.
    int tint_percent = 100;

    // Intensite du grain visuel en pourcentage.
    int grain_percent = 100;

    // Force de refraction des bords en pourcentage.
    int edge_refraction_percent = 100;

    // Largeur de la zone de refraction des bords en pourcentage.
    int edge_width_percent = 100;

    // Intensite de l'aberration chromatique en pourcentage.
    int chromatic_aberration_percent = 100;

    // Force de refraction autour des indicateurs en pourcentage.
    int indicator_refraction_percent = 100;

    // Largeur de refraction autour des indicateurs en pourcentage.
    int indicator_width_percent = 100;

    // Indique si les composants graphiques possedent leur lentille locale statique.
    bool element_glass_enabled = true;

    // Douceur de la transition optique des lentilles locales en pourcentage.
    int element_softness_percent = 100;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages persistants de Calm Water.
// ----------------------------------------------------------------------------
struct GlassEffectCalmWaterSettings {
    // Indique si Calm Water contribue au champ de deformation.
    bool enabled = false;

    // Intensite de deformation en pourcentage.
    int intensity_percent = 100;

    // Vitesse de propagation en pourcentage.
    int speed_percent = 100;

    // Longueur d'onde en pourcentage.
    int wavelength_percent = 100;

    // Intensite des perturbations secondaires en pourcentage.
    int noise_percent = 100;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages persistants de Liquid.
// ----------------------------------------------------------------------------
struct GlassEffectLiquidSettings {
    // Indique si Liquid contribue au champ de deformation.
    bool enabled = false;

    // Intensite de deformation en pourcentage.
    int intensity_percent = 100;

    // Vitesse de propagation en pourcentage.
    int speed_percent = 100;

    // Longueur d'onde en pourcentage.
    int wavelength_percent = 100;

    // Fluidite du mouvement en pourcentage.
    int fluidity_percent = 100;

    // Intensite des perturbations secondaires en pourcentage.
    int noise_percent = 100;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages persistants de Rain.
// ----------------------------------------------------------------------------
struct GlassEffectRainSettings {
    // Indique si Rain contribue au champ de deformation.
    bool enabled = false;

    // Intensite de deformation en pourcentage.
    int intensity_percent = 100;

    // Vitesse de succession des impacts en pourcentage.
    int speed_percent = 100;

    // Densite des impacts en pourcentage.
    int density_percent = 100;

    // Taille maximale des anneaux en pourcentage.
    int ring_size_percent = 100;

    // Duree et douceur du fondu des impacts en pourcentage.
    int fade_percent = 100;
};

// ----------------------------------------------------------------------------
// Regroupe l'apparence et les trois animations cumulables GlassEffect.
// ----------------------------------------------------------------------------
struct GlassEffectSettings {
    // Version courante du schema persistant GlassEffect.
    int schema_version = 1;

    // Preset selectionne ou etat personnalise derive des valeurs.
    GlassEffectPreset preset = GlassEffectPreset::Frosted;

    // Apparence permanente du verre.
    GlassEffectAppearanceSettings appearance{};

    // Reglages propres a Calm Water.
    GlassEffectCalmWaterSettings calm_water{};

    // Reglages propres a Liquid.
    GlassEffectLiquidSettings liquid{};

    // Reglages propres a Rain.
    GlassEffectRainSettings rain{};
};

// ----------------------------------------------------------------------------
// Liste les anciens modes exclusifs lus uniquement pendant la migration INI.
// ----------------------------------------------------------------------------
enum class LegacyGlassEffectAnimationMode {
    Off,
    CalmWater,
    Liquid,
    Rain,
};

// ----------------------------------------------------------------------------
// Regroupe les anciens reglages communs lus uniquement pendant la migration INI.
// ----------------------------------------------------------------------------
struct LegacyGlassEffectAnimationOptions {
    // Intensite globale de deformation en pourcentage.
    int intensity_percent = 100;

    // Vitesse globale de l'animation en pourcentage.
    int speed_percent = 100;

    // Densite des impacts du mode Rain en pourcentage.
    int rain_density_percent = 100;

    // Taille maximale des anneaux du mode Rain en pourcentage.
    int rain_ring_size_percent = 100;
};

// ----------------------------------------------------------------------------
// Retourne le mode glass utilise quand aucun reglage valide n'existe.
//
// Retour :
// - mode glass par defaut.
// ----------------------------------------------------------------------------
GlassEffectMode DefaultGlassEffectMode();

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en mode glass.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - mode glass correspondant, ou mode par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
GlassEffectMode GlassEffectModeFromString(const std::wstring& value);

// ----------------------------------------------------------------------------
// Convertit un mode glass en chaine persistante.
//
// Parametres :
// - mode : mode glass a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring GlassEffectModeToString(GlassEffectMode mode);

// ----------------------------------------------------------------------------
// Retourne le preset GlassEffect utilise quand aucun reglage valide n'existe.
//
// Retour :
// - preset GlassEffect par defaut.
// ----------------------------------------------------------------------------
GlassEffectPreset DefaultGlassEffectPreset();

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en preset GlassEffect.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - preset GlassEffect correspondant, ou preset par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
GlassEffectPreset GlassEffectPresetFromString(const std::wstring& value);

// ----------------------------------------------------------------------------
// Convertit un preset GlassEffect en chaine persistante.
//
// Parametres :
// - preset : preset GlassEffect a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring GlassEffectPresetToString(GlassEffectPreset preset);

// ----------------------------------------------------------------------------
// Retourne le mode d'animation GlassEffect utilise par defaut.
//
// Retour :
// - mode d'animation GlassEffect par defaut.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationMode DefaultLegacyGlassEffectAnimationMode();

// ----------------------------------------------------------------------------
// Convertit une chaine persistante en mode d'animation GlassEffect.
//
// Parametres :
// - value : valeur texte lue depuis les reglages.
//
// Retour :
// - mode d'animation correspondant, ou mode par defaut si la valeur est inconnue.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationMode LegacyGlassEffectAnimationModeFromString(const std::wstring& value);

// ----------------------------------------------------------------------------
// Convertit un mode d'animation GlassEffect en chaine persistante.
//
// Parametres :
// - mode : mode d'animation a convertir.
//
// Retour :
// - valeur texte a sauvegarder.
// ----------------------------------------------------------------------------
std::wstring LegacyGlassEffectAnimationModeToString(LegacyGlassEffectAnimationMode mode);

// ----------------------------------------------------------------------------
// Retourne les options d'animation GlassEffect par defaut.
//
// Retour :
// - options d'animation utilisees quand aucun reglage valide n'existe.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationOptions DefaultLegacyGlassEffectAnimationOptions();

// ----------------------------------------------------------------------------
// Encadre les options d'animation GlassEffect dans les plages supportees.
//
// Parametres :
// - options : options a normaliser.
//
// Retour :
// - options avec valeurs utilisables par le rendu et le menu.
// ----------------------------------------------------------------------------
LegacyGlassEffectAnimationOptions NormalizeLegacyGlassEffectAnimationOptions(
    LegacyGlassEffectAnimationOptions options
);

// ----------------------------------------------------------------------------
// Retourne l'apparence complete associee a un preset.
//
// Parametres :
// - preset : preset demande ; Custom utilise le preset par defaut.
//
// Retour :
// - reglages d'apparence normalises.
// ----------------------------------------------------------------------------
GlassEffectAppearanceSettings GlassEffectAppearanceForPreset(GlassEffectPreset preset);

// ----------------------------------------------------------------------------
// Retourne le bloc GlassEffect utilise par defaut.
//
// Retour :
// - apparence Frosted et animations desactivees.
// ----------------------------------------------------------------------------
GlassEffectSettings DefaultGlassEffectSettings();

// ----------------------------------------------------------------------------
// Normalise les reglages d'apparence dans les bornes du renderer.
// ----------------------------------------------------------------------------
GlassEffectAppearanceSettings NormalizeGlassEffectAppearanceSettings(
    GlassEffectAppearanceSettings settings
);

// ----------------------------------------------------------------------------
// Genere une apparence Glass aleatoire dans les bornes du menu.
// ----------------------------------------------------------------------------
GlassEffectAppearanceSettings RandomizeGlassEffectAppearanceSettings();

// ----------------------------------------------------------------------------
// Genere une opacite de fenetre aleatoire exprimee en pourcentage.
// ----------------------------------------------------------------------------
int RandomGlassEffectOpacityPercent();

// ----------------------------------------------------------------------------
// Normalise les reglages Calm Water.
// ----------------------------------------------------------------------------
GlassEffectCalmWaterSettings NormalizeGlassEffectCalmWaterSettings(
    GlassEffectCalmWaterSettings settings
);

// ----------------------------------------------------------------------------
// Genere des reglages Calm Water aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
GlassEffectCalmWaterSettings RandomizeGlassEffectCalmWaterSettings(
    GlassEffectCalmWaterSettings current
);

// ----------------------------------------------------------------------------
// Normalise les reglages Liquid.
// ----------------------------------------------------------------------------
GlassEffectLiquidSettings NormalizeGlassEffectLiquidSettings(
    GlassEffectLiquidSettings settings
);

// ----------------------------------------------------------------------------
// Genere des reglages Liquid aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
GlassEffectLiquidSettings RandomizeGlassEffectLiquidSettings(
    GlassEffectLiquidSettings current
);

// ----------------------------------------------------------------------------
// Normalise les reglages Rain.
// ----------------------------------------------------------------------------
GlassEffectRainSettings NormalizeGlassEffectRainSettings(GlassEffectRainSettings settings);

// ----------------------------------------------------------------------------
// Genere des reglages Rain aleatoires en conservant leur activation.
// ----------------------------------------------------------------------------
GlassEffectRainSettings RandomizeGlassEffectRainSettings(GlassEffectRainSettings current);

// ----------------------------------------------------------------------------
// Normalise l'ensemble du bloc GlassEffect et recalcule son preset affiche.
// ----------------------------------------------------------------------------
GlassEffectSettings NormalizeGlassEffectSettings(GlassEffectSettings settings);

// ----------------------------------------------------------------------------
// Retourne le preset exact correspondant a une apparence.
//
// Retour :
// - preset connu ou Custom si aucune correspondance exacte n'existe.
// ----------------------------------------------------------------------------
GlassEffectPreset MatchingGlassEffectPreset(const GlassEffectAppearanceSettings& settings);

// ----------------------------------------------------------------------------
// Indique si au moins une animation GlassEffect continue est active.
// ----------------------------------------------------------------------------
bool HasActiveGlassEffectAnimation(const GlassEffectSettings& settings);

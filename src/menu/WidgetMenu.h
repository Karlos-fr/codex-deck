// ============================================================================
// Codex Glass - Menu contextuel du widget
// ----------------------------------------------------------------------------
// Ce fichier declare les commandes et helpers de creation du menu contextuel
// Win32 utilise par la fenetre et l'icone de notification.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <windows.h>

#include <vector>

// Item de bandeau cliquable affiche en tete du menu.
constexpr UINT kCommandMenuBanner = 1000;

// Libelle court du depot GitHub affiche dans le bandeau.
constexpr wchar_t kCodexGlassRepositoryLabel[] = L"github.com/Karlos-fr/codex-glass";

// Commande de menu qui affiche ou masque le widget.
constexpr UINT kCommandToggleVisible = 1001;

// Commande de menu qui force un rafraichissement immediat.
constexpr UINT kCommandRefreshNow = 1002;

// Commande de menu qui active ou desactive le premier plan.
constexpr UINT kCommandToggleAlwaysOnTop = 1003;

// Commande de menu qui verrouille ou deverrouille la position.
constexpr UINT kCommandToggleLockPosition = 1004;

// Commande de menu qui active ou desactive l'aimantation aux bords.
constexpr UINT kCommandToggleDockToEdges = 1010;

// Commande de menu qui active ou desactive le graphe.
constexpr UINT kCommandToggleGraph = 1005;

// Commande qui affiche ou masque la ligne de quota principale sur cinq heures.
constexpr UINT kCommandToggleQuotaFiveHour = 1012;

// Commande qui affiche ou masque la ligne de quota principale hebdomadaire.
constexpr UINT kCommandToggleQuotaWeekly = 1013;

// Commande qui affiche ou masque la ligne de quota Spark sur cinq heures.
constexpr UINT kCommandToggleQuotaSparkFiveHour = 1014;

// Commande qui affiche ou masque la ligne de quota Spark hebdomadaire.
constexpr UINT kCommandToggleQuotaSparkWeekly = 1015;

// Commande de menu qui quitte l'application.
constexpr UINT kCommandExit = 1006;

// Commande de debug qui copie une capture complete du widget dans le presse-papiers.
constexpr UINT kCommandCopyInterfaceScreenshot = 1007;

// Commande de menu qui active ou desactive le click-through.
constexpr UINT kCommandToggleClickThrough = 1008;

// Commande de menu qui active ou desactive le masquage en plein ecran.
constexpr UINT kCommandToggleHideWhenFullscreen = 1009;

// Commande de menu qui active ou desactive le lancement avec Windows.
constexpr UINT kCommandToggleStartWithWindows = 1011;

// Commande de menu qui selectionne le mode minimal.
constexpr UINT kCommandDisplayMinimal = 1100;

// Commande de menu qui selectionne le mode compact.
constexpr UINT kCommandDisplayCompact = 1101;

// Commande de menu qui selectionne le mode complet.
constexpr UINT kCommandDisplayComplete = 1102;

// Commande de menu qui selectionne le mode horizontal.
constexpr UINT kCommandDisplayHorizontal = 1103;

// Commande de menu qui selectionne le mode vertical.
constexpr UINT kCommandDisplayVertical = 1104;

// Commande de menu qui selectionne le rafraichissement manuel.
constexpr UINT kCommandRefreshManual = 1201;

// Commande de menu qui selectionne un rafraichissement de 10 secondes.
constexpr UINT kCommandRefresh10Seconds = 1206;

// Commande de menu qui selectionne un rafraichissement de 30 secondes.
constexpr UINT kCommandRefresh30Seconds = 1202;

// Commande de menu qui selectionne un rafraichissement de 1 minute.
constexpr UINT kCommandRefresh1Minute = 1203;

// Commande de menu qui selectionne un rafraichissement de 5 minutes.
constexpr UINT kCommandRefresh5Minutes = 1204;

// Commande de menu qui selectionne un rafraichissement de 15 minutes.
constexpr UINT kCommandRefresh15Minutes = 1205;

// Commande de menu qui active ou desactive l'effet glass.
constexpr UINT kCommandToggleGlassEffect = 1250;

// Commande de menu qui selectionne le preset GlassEffect clair.
constexpr UINT kCommandGlassEffectPresetClear = 1260;

// Commande de menu qui selectionne le preset GlassEffect givree.
constexpr UINT kCommandGlassEffectPresetFrosted = 1261;

// Commande de menu qui selectionne le preset GlassEffect subtil.
constexpr UINT kCommandGlassEffectPresetSubtle = 1262;

// Commande de menu qui selectionne le preset GlassEffect fort.
constexpr UINT kCommandGlassEffectPresetStrong = 1263;

// Commande representant une apparence GlassEffect personnalisee.
constexpr UINT kCommandGlassEffectPresetCustom = 1265;

// Commande de menu qui reinitialise les reglages principaux.
constexpr UINT kCommandResetDefaults = 1264;

// Commande qui active independamment Calm Water.
constexpr UINT kCommandToggleGlassCalmWater = 1275;
// Commande qui active independamment Liquid.
constexpr UINT kCommandToggleGlassLiquid = 1276;
// Commande qui active independamment Rain.
constexpr UINT kCommandToggleGlassRain = 1277;
// Commande qui reinitialise uniquement l'apparence GlassEffect.
constexpr UINT kCommandResetGlassAppearance = 1278;
// Commande qui reinitialise uniquement Calm Water.
constexpr UINT kCommandResetGlassCalmWater = 1279;
// Commande qui reinitialise uniquement Liquid.
constexpr UINT kCommandResetGlassLiquid = 1280;
// Commande qui reinitialise uniquement Rain.
constexpr UINT kCommandResetGlassRain = 1281;
// Commande qui reinitialise tout le bloc GlassEffect.
constexpr UINT kCommandResetGlassEffect = 1282;
// Commande qui active le verre local statique des composants graphiques.
constexpr UINT kCommandToggleGlassElementLens = 1283;
// Commande qui genere une apparence Glass aleatoire.
constexpr UINT kCommandRandomizeGlassAppearance = 1284;
// Commande qui genere des reglages Calm Water aleatoires.
constexpr UINT kCommandRandomizeGlassCalmWater = 1285;
// Commande qui genere des reglages Liquid aleatoires.
constexpr UINT kCommandRandomizeGlassLiquid = 1286;
// Commande qui genere des reglages Rain aleatoires.
constexpr UINT kCommandRandomizeGlassRain = 1287;

// Commande de menu qui active ou desactive la vibration.
constexpr UINT kCommandToggleVibration = 1300;

// Commande de menu qui active ou desactive le test automatique de vibration.
constexpr UINT kCommandToggleVibrationTestAuto = 1340;

// Commande de menu temporaire qui teste l'animation des chiffres.
constexpr UINT kCommandToggleRollingNumberTest = 1360;

// Commande qui active independamment Vibration.
constexpr UINT kCommandToggleMotionVibration = 1370;
// Commande qui active independamment Pulse.
constexpr UINT kCommandToggleMotionPulse = 1371;
// Commande qui active independamment Wave.
constexpr UINT kCommandToggleMotionWave = 1372;

// Commande qui active l'axe horizontal de Vibration.
constexpr UINT kCommandMotionVibrationHorizontal = 1373;
// Commande qui active l'axe vertical de Vibration.
constexpr UINT kCommandMotionVibrationVertical = 1374;

// Commande qui selectionne une pulsation.
constexpr UINT kCommandMotionPulseRepetitions1 = 1375;
// Commande qui selectionne deux pulsations.
constexpr UINT kCommandMotionPulseRepetitions2 = 1376;
// Commande qui selectionne trois pulsations.
constexpr UINT kCommandMotionPulseRepetitions3 = 1377;

// Commande qui propage Wave vers la gauche.
constexpr UINT kCommandMotionWaveDirectionLeft = 1378;
// Commande qui propage Wave vers la droite.
constexpr UINT kCommandMotionWaveDirectionRight = 1379;
// Commande qui propage Wave vers le haut.
constexpr UINT kCommandMotionWaveDirectionUp = 1380;
// Commande qui propage Wave vers le bas.
constexpr UINT kCommandMotionWaveDirectionDown = 1381;
// Commande qui propage Wave radialement.
constexpr UINT kCommandMotionWaveDirectionRadial = 1382;

// Commande qui reinitialise les reglages de Vibration.
constexpr UINT kCommandResetMotionVibration = 1383;
// Commande qui reinitialise les reglages de Pulse.
constexpr UINT kCommandResetMotionPulse = 1384;
// Commande qui reinitialise les reglages de Wave.
constexpr UINT kCommandResetMotionWave = 1385;
// Commande qui genere des reglages Vibration aleatoires.
constexpr UINT kCommandRandomizeMotionVibration = 1388;
// Commande qui genere des reglages Pulse aleatoires.
constexpr UINT kCommandRandomizeMotionPulse = 1389;
// Commande qui genere des reglages Wave aleatoires.
constexpr UINT kCommandRandomizeMotionWave = 1390;

// Commande qui active le declenchement Motion sur baisse du quota.
constexpr UINT kCommandMotionTriggerUsageDrop = 1386;

// Commande qui active le declenchement Motion sur reset du quota.
constexpr UINT kCommandMotionTriggerQuotaReset = 1387;

// Premiere commande de menu reservee aux presets de couleurs dynamiques.
constexpr UINT kCommandColorPresetBase = 1700;

// Commande de menu qui reinitialise les couleurs.
constexpr UINT kCommandColorReset = 1417;

// Commande de menu qui ouvre la personnalisation des couleurs.
constexpr UINT kCommandColorCustomize = 1418;

// Commande de l'item owner-drawn du slider d'opacite.
constexpr UINT kCommandOpacitySlider = 1420;

// Slider de l'intensite physique de Vibration.
constexpr UINT kCommandMotionVibrationIntensitySlider = 1430;
// Slider de la duree de Vibration.
constexpr UINT kCommandMotionVibrationDurationSlider = 1431;
// Slider de la frequence de Vibration.
constexpr UINT kCommandMotionVibrationFrequencySlider = 1432;
// Slider de la refraction de Vibration.
constexpr UINT kCommandMotionVibrationRefractionSlider = 1433;
// Slider de l'amortissement de Vibration.
constexpr UINT kCommandMotionVibrationDampingSlider = 1434;
// Slider de l'intensite de Pulse.
constexpr UINT kCommandMotionPulseIntensitySlider = 1435;
// Slider de la duree de Pulse.
constexpr UINT kCommandMotionPulseDurationSlider = 1436;
// Slider de la douceur de Pulse.
constexpr UINT kCommandMotionPulseSoftnessSlider = 1437;
// Slider de l'etendue interieure de Pulse.
constexpr UINT kCommandMotionPulseExtentSlider = 1438;
// Slider de l'intensite de Wave.
constexpr UINT kCommandMotionWaveIntensitySlider = 1439;
// Slider de la duree de Wave.
constexpr UINT kCommandMotionWaveDurationSlider = 1440;
// Slider de la vitesse de Wave.
constexpr UINT kCommandMotionWaveSpeedSlider = 1441;
// Slider de la largeur de Wave.
constexpr UINT kCommandMotionWaveWavelengthSlider = 1442;
// Slider du nombre d'ondes de Wave.
constexpr UINT kCommandMotionWaveCountSlider = 1443;
// Slider de l'amortissement de Wave.
constexpr UINT kCommandMotionWaveDampingSlider = 1444;
// Slider de la refraction de Wave.
constexpr UINT kCommandMotionWaveRefractionSlider = 1445;
// Slider de l'ondulation transversale du front Wave.
constexpr UINT kCommandMotionWaveFrontUndulationSlider = 1473;
// Slider du seuil global de declenchement des Effets Motion.
constexpr UINT kCommandMotionThresholdSlider = 1446;
// Slider du delai global entre deux declenchements des Effets Motion.
constexpr UINT kCommandMotionCooldownSlider = 1447;

// Slider de diffusion de l'apparence GlassEffect.
constexpr UINT kCommandGlassAppearanceDiffusionSlider = 1450;
// Slider de teinte de l'apparence GlassEffect.
constexpr UINT kCommandGlassAppearanceTintSlider = 1451;
// Slider de grain de l'apparence GlassEffect.
constexpr UINT kCommandGlassAppearanceGrainSlider = 1452;
// Slider de refraction des bords GlassEffect.
constexpr UINT kCommandGlassAppearanceEdgeRefractionSlider = 1453;
// Slider de largeur des bords GlassEffect.
constexpr UINT kCommandGlassAppearanceEdgeWidthSlider = 1454;
// Slider d'aberration chromatique GlassEffect.
constexpr UINT kCommandGlassAppearanceChromaticSlider = 1455;
// Slider de zoom signe des composants GlassEffect.
constexpr UINT kCommandGlassAppearanceIndicatorRefractionSlider = 1456;
// Slider d'etendue des lentilles des composants GlassEffect.
constexpr UINT kCommandGlassAppearanceIndicatorWidthSlider = 1457;
// Slider de douceur de la transition du verre local.
constexpr UINT kCommandGlassAppearanceElementSoftnessSlider = 1472;

// Slider d'intensite Calm Water.
constexpr UINT kCommandGlassCalmIntensitySlider = 1458;
// Slider de vitesse Calm Water.
constexpr UINT kCommandGlassCalmSpeedSlider = 1459;
// Slider de longueur d'onde Calm Water.
constexpr UINT kCommandGlassCalmWavelengthSlider = 1460;
// Slider de perturbations Calm Water.
constexpr UINT kCommandGlassCalmNoiseSlider = 1461;

// Slider d'intensite Liquid.
constexpr UINT kCommandGlassLiquidIntensitySlider = 1462;
// Slider de vitesse Liquid.
constexpr UINT kCommandGlassLiquidSpeedSlider = 1463;
// Slider de longueur d'onde Liquid.
constexpr UINT kCommandGlassLiquidWavelengthSlider = 1464;
// Slider de fluidite Liquid.
constexpr UINT kCommandGlassLiquidFluiditySlider = 1465;
// Slider de perturbations Liquid.
constexpr UINT kCommandGlassLiquidNoiseSlider = 1466;

// Slider d'intensite Rain.
constexpr UINT kCommandGlassRainIntensitySlider = 1467;
// Slider de vitesse Rain.
constexpr UINT kCommandGlassRainSpeedSlider = 1468;
// Slider de densite Rain.
constexpr UINT kCommandGlassRainDensitySlider = 1469;
// Slider de taille des anneaux Rain.
constexpr UINT kCommandGlassRainRingSizeSlider = 1470;
// Slider de fondu des impacts Rain.
constexpr UINT kCommandGlassRainFadeSlider = 1471;

// Commande de menu qui selectionne la langue automatique.
constexpr UINT kCommandLanguageAuto = 1601;

// Commande de menu qui selectionne le francais.
constexpr UINT kCommandLanguageFrench = 1602;

// Commande de menu qui selectionne l'anglais.
constexpr UINT kCommandLanguageEnglish = 1603;

// ----------------------------------------------------------------------------
// Liste les intervalles de recuperation des donnees supportes.
// ----------------------------------------------------------------------------
enum class UsageRefreshInterval {
    Manual,
    Seconds10,
    Seconds30,
    Minute1,
    Minutes5,
    Minutes15,
};

// ----------------------------------------------------------------------------
// Regroupe l'etat necessaire pour construire le menu sans acceder aux globals.
// ----------------------------------------------------------------------------
struct WidgetMenuState {
    // Indique si le widget est actuellement visible.
    bool visible = true;

    // Indique si le widget reste au premier plan.
    bool always_on_top = true;

    // Indique si la position du widget est verrouillee.
    bool lock_position = false;

    // Indique si le widget s'aimante aux bords de l'ecran.
    bool dock_to_screen_edges = false;

    // Indique si les clics traversent le widget.
    bool click_through = false;

    // Indique si le widget se masque en plein ecran.
    bool hide_when_fullscreen = true;

    // Indique si le widget demarre avec la session Windows.
    bool start_with_windows = false;

    // Mode d'affichage courant du widget.
    WidgetDisplayMode display_mode = WidgetDisplayMode::Complete;

    // Visibilite courante des quatre lignes de quota.
    WidgetQuotaVisibilitySettings quota_visibility{};

    // Mode d'effet glass courant.
    GlassEffectMode glass_effect_mode = GlassEffectMode::Off;

    // Reglages complets de l'apparence et des animations GlassEffect.
    GlassEffectSettings glass_effect{};

    // Reglages courants des Effets Motion cumulables.
    WidgetMotionEffectsSettings motion_effects{};

    // Indique si le test automatique de vibration est actif.
    bool vibration_test_auto_enabled = false;

    // Indique si le test temporaire des chiffres est actif.
    bool rolling_number_test_enabled = false;

    // Intervalle courant de recuperation de l'usage.
    UsageRefreshInterval usage_refresh_interval = UsageRefreshInterval::Minute1;

    // Opacite de fond courante, entre 0 et 1.
    double background_opacity = 1.0;

    // Couleur thematique des controles actifs du menu.
    COLORREF active_control_color = RGB(0x22, 0xC5, 0x5E);

    // Langue courante de l'interface.
    UiLanguage language = UiLanguage::Auto;

    // Presets de couleurs disponibles.
    const std::vector<WidgetColorPreset>* color_presets = nullptr;

    // Indique si l'extension laterale de couleurs est ouverte.
    bool color_panel_open = false;
};

// ----------------------------------------------------------------------------
// Traite WM_MEASUREITEM pour les items owner-drawn du menu contextuel.
//
// Parametres :
// - measure_item : structure Win32 a remplir.
//
// Retour :
// - true si le message a ete traite.
// ----------------------------------------------------------------------------
bool MeasureContextMenuItem(MEASUREITEMSTRUCT* measure_item);

// ----------------------------------------------------------------------------
// Traite WM_DRAWITEM pour les items owner-drawn du menu contextuel.
//
// Parametres :
// - draw_item : structure Win32 a dessiner.
//
// Retour :
// - true si le message a ete traite.
// ----------------------------------------------------------------------------
bool DrawContextMenuItem(DRAWITEMSTRUCT* draw_item);

// ----------------------------------------------------------------------------
// Affiche le menu contextuel a une position ecran donnee.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire du menu.
// - point : position ecran ou afficher le menu.
// - state : etat courant necessaire a la construction du menu.
// - opened_from_tray : indique si le menu vient de l'icone de notification.
// ----------------------------------------------------------------------------
void ShowContextMenu(HWND hwnd, POINT point, const WidgetMenuState& state, bool opened_from_tray = false);

// ----------------------------------------------------------------------------
// Actualise les libelles du menu contextuel actuellement affiche.
//
// Parametres :
// - previous_language : langue effective des libelles encore en memoire.
//
// Effet de bord : remplace et redessine les textes sans fermer TrackPopupMenu.
// ----------------------------------------------------------------------------
void RefreshContextMenuLocalization(UiLanguage previous_language);

// ----------------------------------------------------------------------------
// Resynchronise l'apparence Glass dans un menu contextuel encore ouvert.
//
// Parametres :
// - settings : reglages Glass courants a presenter.
// ----------------------------------------------------------------------------
void RefreshGlassEffectAppearanceMenuState(const GlassEffectSettings& settings);

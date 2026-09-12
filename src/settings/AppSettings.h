// ============================================================================
// Codex Glass - Reglages locaux de l'application
// ----------------------------------------------------------------------------
// Ce fichier declare les structures et fonctions de chargement/sauvegarde des
// reglages utilisateur persistants du widget.
// ============================================================================

#pragma once

#include "../glass/WidgetGlassSettings.h"
#include "../motion/WidgetMotionEffectsSettings.h"
#include "../vibration/WidgetVibrationSettings.h"

#include <windows.h>

#include <string>
#include <vector>

// ----------------------------------------------------------------------------
// Liste les modes d'affichage prevus par le widget.
// ----------------------------------------------------------------------------
enum class WidgetDisplayMode {
    Minimal,
    Compact,
    Complete,
    Horizontal,
    Vertical,
};

// ----------------------------------------------------------------------------
// Liste les pages disponibles dans la zone de graphe.
// ----------------------------------------------------------------------------
enum class WidgetGraphPage {
    Quotas = 0,
    Tokens = 1,
    Activity = 2,
    Summary = 3,
};

// ----------------------------------------------------------------------------
// Convertit une valeur INI en page graphique stable.
//
// Parametres :
// - value : entier lu depuis les reglages.
//
// Retour :
// - page correspondante, Quotas servant de repli pour une valeur inconnue.
// ----------------------------------------------------------------------------
constexpr WidgetGraphPage WidgetGraphPageFromStoredValue(int value) {
    return value == 1 ? WidgetGraphPage::Tokens
        : (value == 2 ? WidgetGraphPage::Activity
            : (value == 3 ? WidgetGraphPage::Summary : WidgetGraphPage::Quotas));
}

// ----------------------------------------------------------------------------
// Convertit une page graphique en valeur INI stable.
//
// Parametres :
// - page : page a persister.
//
// Retour :
// - zero a trois sans modifier les valeurs historiques.
// ----------------------------------------------------------------------------
constexpr int WidgetGraphPageToStoredValue(WidgetGraphPage page) {
    return page == WidgetGraphPage::Tokens ? 1
        : (page == WidgetGraphPage::Activity ? 2
            : (page == WidgetGraphPage::Summary ? 3 : 0));
}

// ----------------------------------------------------------------------------
// Liste les plages temporelles disponibles pour le graphe Quotas.
// ----------------------------------------------------------------------------
enum class GraphRange {
    Minutes5,
    Hours1,
    Hours5,
    Hours24,
    Days7,
    Days30,
};

// ----------------------------------------------------------------------------
// Liste les langues d'interface supportees par les ressources embarquees.
// ----------------------------------------------------------------------------
enum class UiLanguage {
    Auto,
    French,
    English,
};

// ----------------------------------------------------------------------------
// Regroupe les couleurs personnalisables du widget.
// ----------------------------------------------------------------------------
struct WidgetColorSettings {
    // Couleur du fond principal de la fenetre.
    COLORREF background = RGB(0x21, 0x21, 0x21);

    // Couleur du contour de la fenetre et des cadres discrets.
    COLORREF border = RGB(0x3A, 0x3A, 0x3A);

    // Couleur des libelles et valeurs principales.
    COLORREF text = RGB(0xEB, 0xEB, 0xF0);

    // Couleur des libelles, axes et informations secondaires.
    COLORREF secondary_text = RGB(0xEB, 0xEB, 0xF0);

    // Couleur de la courbe du graphe historique.
    COLORREF history_curve = RGB(0x22, 0xC5, 0x5E);

    // Couleur de la partie restante des barres.
    COLORREF remaining_bar = RGB(0x22, 0xC5, 0x5E);

    // Couleur de la partie consommee des barres.
    COLORREF consumed_bar = RGB(0xEB, 0xEB, 0xF0);

    // Couleur des controles actifs et des selections de l'interface.
    COLORREF active_control = RGB(0x22, 0xC5, 0x5E);
};

// ----------------------------------------------------------------------------
// Regroupe un preset de couleurs nomme.
// ----------------------------------------------------------------------------
struct WidgetColorPreset {
    // Identifiant stable du preset, utilise dans settings.ini.
    std::wstring id;

    // Nom affiche dans le menu des presets.
    std::wstring name;

    // Couleurs appliquees par le preset.
    WidgetColorSettings colors{};
};

// ----------------------------------------------------------------------------
// Regroupe la visibilite individuelle des quatre lignes de quota principales.
// ----------------------------------------------------------------------------
struct WidgetQuotaVisibilitySettings {
    // Indique si la ligne du quota principal sur cinq heures est affichee.
    bool five_hour = true;

    // Indique si la ligne du quota principal hebdomadaire est affichee.
    bool weekly = true;

    // Indique si la ligne Spark sur cinq heures est affichee.
    bool spark_five_hour = true;

    // Indique si la ligne Spark hebdomadaire est affichee.
    bool spark_weekly = true;
};

// ----------------------------------------------------------------------------
// Regroupe les reglages locaux persistants de l'application.
// ----------------------------------------------------------------------------
struct AppSettings {
    // Rectangle de fenetre sauvegarde ou calcule par defaut.
    RECT window_rect{};

    // Indique si le widget reste au premier plan.
    bool always_on_top = true;

    // Indique si le deplacement de la fenetre est verrouille.
    bool lock_position = false;

    // Indique si le widget s'aimante aux bords de l'ecran pendant le deplacement.
    bool dock_to_screen_edges = false;

    // Opacite du fond du panneau entre 0 et 1.
    double background_opacity = 0.92;

    // Intervalle de recuperation des donnees en secondes, 0 pour manuel.
    int refresh_interval_seconds = 60;

    // Mode d'affichage courant du widget.
    WidgetDisplayMode display_mode = WidgetDisplayMode::Compact;

    // Couleurs courantes du widget.
    WidgetColorSettings colors{};

    // Presets de couleurs charges depuis settings.ini.
    std::vector<WidgetColorPreset> color_presets{};

    // Langue d'interface courante, Auto suit la langue Windows.
    UiLanguage language = UiLanguage::Auto;

    // Indique si le graphe doit etre affiche quand le mode le permet.
    bool show_graph = false;

    // Visibilite persistante des lignes de quota du widget.
    WidgetQuotaVisibilitySettings quota_visibility{};

    // Page de graphe selectionnee sous le widget complet.
    WidgetGraphPage graph_page = WidgetGraphPage::Quotas;

    // Plage temporelle persistante du graphe Quotas.
    GraphRange graph_range = GraphRange::Hours24;

    // Plage temporelle persistante de l'histogramme Tokens.
    GraphRange token_graph_range = GraphRange::Hours5;

    // Granularite persistante de la vue Activite, jours ou cinq minutes.
    GraphRange activity_graph_range = GraphRange::Days30;

    // Plage temporelle persistante du bilan de consommation.
    GraphRange summary_graph_range = GraphRange::Days7;

    // Indique si la fenetre laisse passer les clics souris vers l'arriere-plan.
    bool click_through = false;

    // Indique si le widget se masque quand une application plein ecran est active.
    bool hide_when_fullscreen = false;

    // Indique si Codex Glass doit demarrer avec la session Windows.
    bool start_with_windows = false;

    // Mode d'effet glass selectionne, sans effet applique pendant la phase B.
    GlassEffectMode glass_effect_mode = GlassEffectMode::Off;

    // Apparence et animations cumulables du nouveau schema GlassEffect.
    GlassEffectSettings glass_effect{};

    // Reglages cumulables des Effets Motion.
    WidgetMotionEffectsSettings motion_effects{};
};

// ----------------------------------------------------------------------------
// Charge les reglages locaux depuis le fichier de configuration.
//
// Parametres :
// - default_rect : rectangle utilise quand aucun reglage valide n'existe.
//
// Retour :
// - reglages charges avec valeurs par defaut robustes.
// ----------------------------------------------------------------------------
AppSettings LoadAppSettings(const RECT& default_rect);

// ----------------------------------------------------------------------------
// Sauvegarde les reglages locaux dans le fichier de configuration.
//
// Parametres :
// - settings : reglages a persister.
//
// Retour :
// - true si la sauvegarde a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool SaveAppSettings(const AppSettings& settings);

// ----------------------------------------------------------------------------
// Retourne le chemin du fichier de reglages local.
//
// Retour :
// - chemin complet vers settings.ini.
// ----------------------------------------------------------------------------
std::wstring GetAppSettingsPath();

// ----------------------------------------------------------------------------
// Retourne les presets de couleurs fournis par defaut.
//
// Retour :
// - liste des presets embarques servant de secours initial.
// ----------------------------------------------------------------------------
std::vector<WidgetColorPreset> DefaultWidgetColorPresets();

// ----------------------------------------------------------------------------
// Relocalise les noms des presets embarques dans la langue courante.
//
// Parametres :
// - settings : reglages dont les presets doivent etre relocalises.
// ----------------------------------------------------------------------------
void LocalizeWidgetColorPresetNames(AppSettings& settings);

// ============================================================================
// Codex Glass - Rendu Direct2D du widget
// ----------------------------------------------------------------------------
// Ce fichier declare le renderer responsable des ressources Direct2D,
// DirectWrite et du dessin complet du widget.
// ============================================================================

#pragma once

#include "../animation/WidgetQuotaGraphAnimation.h"

#include "../settings/AppSettings.h"
#include "../usage/UsageSnapshot.h"
#include "../tokens/TokenUsageTypes.h"
#include "WidgetGraphInteraction.h"
#include "WidgetRenderActivityVein.h"
#include "WidgetTrayHideButton.h"
#include "../color/WidgetColorPanel.h"
#include "../menu/WidgetMenu.h"
#include "../animation/WidgetRollingNumberAnimation.h"
#include "../vibration/WidgetVibrationGlassEffect.h"

#include <memory>
#include <string>
#include <windows.h>

class UsageHistoryStore;
struct WidgetGlassEffectFrame;

// ----------------------------------------------------------------------------
// Gere les ressources graphiques et le dessin du widget Codex Glass.
// ----------------------------------------------------------------------------
class WidgetRenderer {
public:
    // ------------------------------------------------------------------------
    // Cree le renderer et son etat interne.
    // ------------------------------------------------------------------------
    WidgetRenderer();

    // ------------------------------------------------------------------------
    // Libere les ressources graphiques encore conservees.
    // ------------------------------------------------------------------------
    ~WidgetRenderer();

    // ------------------------------------------------------------------------
    // Initialise les factories Direct2D et DirectWrite independantes de la
    // fenetre.
    //
    // Retour :
    // - true si les factories sont disponibles.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool Initialize();

    // ------------------------------------------------------------------------
    // Force la recreation des ressources graphiques au prochain rendu.
    // ------------------------------------------------------------------------
    void DiscardDeviceResources();

    // ------------------------------------------------------------------------
    // Redimensionne le render target Direct2D lorsque la fenetre change.
    //
    // Parametres :
    // - hwnd : handle de la fenetre redimensionnee.
    // ------------------------------------------------------------------------
    void Resize(HWND hwnd);

    // ------------------------------------------------------------------------
    // Force la recreation des ressources graphiques et invalide la fenetre.
    //
    // Parametres :
    // - hwnd : handle de la fenetre a redessiner.
    // ------------------------------------------------------------------------
    void RefreshVisualResources(HWND hwnd);

    // ------------------------------------------------------------------------
    // Mesure un texte avec le format partage par les titres de quotas.
    //
    // Parametres :
    // - text : texte localise a mesurer.
    //
    // Retour :
    // - largeur DirectWrite en DIPs, ou zero si la mesure echoue.
    // ------------------------------------------------------------------------
    float MeasureGraphTitleTextWidth(const std::wstring& text) const;

    // ------------------------------------------------------------------------
    // Mesure un texte avec le format partage par les hints compacts.
    //
    // Parametres :
    // - text : texte localise a mesurer.
    //
    // Retour :
    // - largeur DirectWrite en DIPs, ou zero si la mesure echoue.
    // ------------------------------------------------------------------------
    float MeasureCaptionTextWidth(const std::wstring& text) const;

    // ------------------------------------------------------------------------
    // Rend l'onglet graphique actif hors ecran et le copie dans le presse-papiers.
    //
    // Parametres :
    // - hwnd : fenetre qui fournit les ressources, le DPI et le presse-papiers.
    // - settings : page, plage et palette actuellement selectionnees.
    // - snapshot : quotas courants necessaires aux vues et au layout.
    // - token_snapshot : donnees locales des vues Tokens, Activite et Bilan.
    // - history_store : historique utilise par la vue Quotas.
    // - graph_range : plage active de la vue Quotas.
    // - quota_graph_animation : serie actuellement visible dans la vue Quotas.
    //
    // Retour :
    // - true si l'image sans controles ni effets Glass a ete copiee.
    // ------------------------------------------------------------------------
    bool CopyActiveGraphTabToClipboard(
        HWND hwnd,
        const AppSettings& settings,
        const UsageSnapshot& snapshot,
        const TokenUsageSnapshot& token_snapshot,
        const UsageHistoryStore& history_store,
        GraphRange graph_range,
        const WidgetQuotaGraphAnimationFrame& quota_graph_animation
    );

    // ------------------------------------------------------------------------
    // Dessine l'interface du widget.
    //
    // Parametres :
    // - hwnd : handle de la fenetre a redessiner.
    // - settings : reglages visuels et d'affichage courants.
    // - snapshot : releve d'usage courant.
    // - tray_button_interaction : etat du bouton de masquage dans le tray.
    // - history_store : historique local utilise pour le graphe.
    // - graph_range : plage temporelle du graphe.
    // - activity_frame : etats stabilises de la veine d'activite Codex.
    // - activity_interaction : survol global du filament et de son hint.
    // - quota_graph_animation : transition courante de la serie Quotas.
    // - glass_effect_frame : fond capture pour le mode GlassEffect, ou nullptr.
    // - vibration_glass_effect : variations temporaires de vibration visuelle.
    // - five_hour_rolling : animation du pourcentage 5 h.
    // - weekly_rolling : animation du pourcentage semaine.
    // - refreshing_label_dot_count : nombre de points du statut de refresh.
    // - show_graph_header_controls : affiche les controles exclus des captures.
    // ------------------------------------------------------------------------
    void Render(
        HWND hwnd,
        const AppSettings& settings,
        const UsageSnapshot& snapshot,
        const TokenUsageSnapshot& token_snapshot,
        const WidgetGraphInteraction& graph_interaction,
        const WidgetTrayHideButtonInteraction& tray_button_interaction,
        const UsageHistoryStore& history_store,
        GraphRange graph_range,
        const WidgetActivityFrame& activity_frame,
        const WidgetActivityVeinInteraction& activity_interaction,
        const WidgetQuotaGraphAnimationFrame& quota_graph_animation,
        const WidgetGlassEffectFrame* glass_effect_frame,
        const WidgetVibrationGlassEffectState& vibration_glass_effect,
        const WidgetRollingNumberFrame& five_hour_rolling,
        const WidgetRollingNumberFrame& weekly_rolling,
        std::size_t refreshing_label_dot_count,
        bool show_graph_header_controls = true
    );

    // ------------------------------------------------------------------------
    // Dessine uniquement la palette flottante de personnalisation des couleurs.
    //
    // Parametres :
    // - hwnd : handle de la fenetre outil a dessiner.
    // - settings : reglages visuels et couleurs courants.
    // - interaction : etat souris de la palette.
    // - glass_effect_frame : capture du bureau situee derriere la palette.
    // - vibration_glass_effect : deformation de vibration courante.
    // ------------------------------------------------------------------------
    void RenderColorPanelWindow(
        HWND hwnd,
        const AppSettings& settings,
        const WidgetColorPanelInteraction& interaction,
        const WidgetGlassEffectFrame* glass_effect_frame,
        const WidgetVibrationGlassEffectState& vibration_glass_effect
    );

private:
    struct Impl;

    // Implementation privee gardant Direct2D hors du header public.
    std::unique_ptr<Impl> impl_;
};

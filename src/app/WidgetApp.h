// ============================================================================
// Codex Glass - Orchestration de l'application
// ----------------------------------------------------------------------------
// Ce fichier declare la classe qui possede l'etat principal du widget, gere le
// cycle de vie Win32 et coordonne les modules de fenetre, menu, rendu et usage.
// ============================================================================

#pragma once

#include "../activity/CodexActivityMonitor.h"
#include "../activity/WidgetActivityController.h"
#include "../settings/AppSettings.h"
#include "../settings/AppSettingsSaveWorker.h"
#include "../usage/UsageHistoryStore.h"
#include "../usage/UsageRefreshWorker.h"
#include "../usage/UsageSnapshot.h"
#include "../tokens/TokenUsageRefreshWorker.h"
#include "../tokens/TokenUsageTypes.h"
#include "../color/WidgetColorPanel.h"
#include "../color/WidgetColorTools.h"
#include "../glass/WidgetGlassEffect.h"
#include "../menu/WidgetMenu.h"
#include "../rendering/WidgetRendering.h"
#include "../animation/WidgetQuotaGraphAnimation.h"
#include "../animation/WidgetRollingNumberAnimation.h"
#include "../vibration/WidgetVibrationAnimation.h"
#include "../vibration/WidgetVibrationController.h"
#include "../vibration/WidgetVibrationGlassEffect.h"
#include "../vibration/WidgetVibrationMotion.h"

#include <windows.h>

#include <memory>

// ----------------------------------------------------------------------------
// Application native du widget Codex Glass.
// ----------------------------------------------------------------------------
class WidgetApp {
public:
    // ------------------------------------------------------------------------
    // Initialise une application avec ses valeurs par defaut.
    // ------------------------------------------------------------------------
    WidgetApp();

    // ------------------------------------------------------------------------
    // Detruit les ressources possedees par l'application.
    // ------------------------------------------------------------------------
    ~WidgetApp();

    // ------------------------------------------------------------------------
    // Lance l'application et execute la boucle de messages Win32.
    //
    // Parametres :
    // - instance : handle de l'instance courante de l'application.
    // - command_show : mode d'affichage initial demande par Windows.
    //
    // Retour :
    // - code de sortie du processus.
    // ------------------------------------------------------------------------
    int Run(HINSTANCE instance, int command_show);

    // ------------------------------------------------------------------------
    // Procedure de fenetre statique branchee sur la classe Win32.
    //
    // Parametres :
    // - hwnd : handle de la fenetre concernee.
    // - message : identifiant du message Win32 recu.
    // - wparam : premier parametre du message.
    // - lparam : second parametre du message.
    //
    // Retour :
    // - valeur de traitement attendue par Win32 pour le message recu.
    // ------------------------------------------------------------------------
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    // ------------------------------------------------------------------------
    // Procedure Win32 statique de la palette flottante de couleurs.
    // ------------------------------------------------------------------------
    static LRESULT CALLBACK ColorPanelWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    // Distribue un message Win32 vers le traitement applicatif approprie.
    LRESULT HandleWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    // Traite les messages Win32 de la palette flottante de couleurs.
    LRESULT HandleColorPanelWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    // Convertit un intervalle d'usage en millisecondes Win32.
    UINT RefreshIntervalMilliseconds(UsageRefreshInterval interval) const;
    // Convertit un nombre de secondes en intervalle d'usage supporte.
    UsageRefreshInterval RefreshIntervalFromSeconds(int seconds) const;
    // Convertit un intervalle d'usage en secondes persistables.
    int RefreshIntervalToSeconds(UsageRefreshInterval interval) const;
    // Recree les ressources visuelles dependantes des reglages courants.
    void RefreshVisualResources(HWND hwnd);
    // Affiche le widget sans lui imposer le focus.
    void ShowWidget(HWND hwnd);
    // Bascule entre l'affichage et le masquage du widget.
    void ToggleWidgetVisibility(HWND hwnd);
    // Indique si une fenetre occupe tout l'ecran du widget.
    bool IsFullscreenWindow(HWND window, HWND widget_hwnd) const;
    // Applique la politique de masquage en plein ecran.
    void UpdateFullscreenVisibility(HWND hwnd);
    // Capture la position courante puis persiste les reglages.
    void SaveCurrentSettings(HWND hwnd);
    // Change l'intervalle de collecte et rearme son timer.
    void SetUsageRefreshInterval(HWND hwnd, UsageRefreshInterval interval);
    // Applique l'opacite de fond et la persiste selon la demande.
    void ApplyBackgroundOpacity(HWND hwnd, double opacity, bool save_immediately);
    // Applique une palette complete et la persiste selon la demande.
    void ApplyWidgetColors(HWND hwnd, const WidgetColorSettings& colors, bool save_immediately);
    // Remplace la palette complete depuis une commande utilisateur.
    void SetWidgetColors(HWND hwnd, const WidgetColorSettings& colors);
    // Modifie une seule couleur de la palette courante.
    void SetSingleWidgetColor(HWND hwnd, WidgetColorField field, COLORREF color);
    // Bascule la visibilite du panneau de couleurs.
    void ToggleColorPanel(HWND hwnd);
    // Ouvre ou ferme explicitement le panneau de couleurs.
    void SetColorPanelOpen(HWND hwnd, bool open);
    // Selectionne le mode Glass historique demande.
    void SetGlassEffectMode(HWND hwnd, GlassEffectMode mode);
    // Applique un prereglage d'apparence Glass.
    void SetGlassEffectPreset(HWND hwnd, GlassEffectPreset preset);
    // Applique les parametres permanents d'apparence Glass.
    void SetGlassEffectAppearanceSettings(HWND hwnd, GlassEffectAppearanceSettings settings, bool save_immediately);
    // Applique les parametres de l'animation eau calme.
    void SetGlassEffectCalmWaterSettings(HWND hwnd, GlassEffectCalmWaterSettings settings, bool save_immediately);
    // Applique les parametres de l'animation liquide.
    void SetGlassEffectLiquidSettings(HWND hwnd, GlassEffectLiquidSettings settings, bool save_immediately);
    // Applique les parametres de l'animation pluie.
    void SetGlassEffectRainSettings(HWND hwnd, GlassEffectRainSettings settings, bool save_immediately);
    // Retablit les parametres par defaut de l'apparence Glass.
    void ResetGlassEffectAppearance(HWND hwnd);
    // Retablit les parametres par defaut de l'eau calme.
    void ResetGlassEffectCalmWater(HWND hwnd);
    // Retablit les parametres par defaut de l'animation liquide.
    void ResetGlassEffectLiquid(HWND hwnd);
    // Retablit les parametres par defaut de l'animation pluie.
    void ResetGlassEffectRain(HWND hwnd);
    // Genere et applique une apparence Glass aleatoire.
    void RandomizeGlassEffectAppearance(HWND hwnd);
    // Genere et applique des reglages Eau calme aleatoires.
    void RandomizeGlassEffectCalmWater(HWND hwnd);
    // Genere et applique des reglages Liquid aleatoires.
    void RandomizeGlassEffectLiquid(HWND hwnd);
    // Genere et applique des reglages Rain aleatoires.
    void RandomizeGlassEffectRain(HWND hwnd);
    // Retablit l'ensemble des reglages Glass par defaut.
    void ResetGlassEffectSettings(HWND hwnd);
    // Applique et persiste la configuration des effets Motion.
    void SetWidgetMotionEffectsSettings(HWND hwnd, WidgetMotionEffectsSettings settings);
    // Retablit tous les reglages applicatifs par defaut.
    void ResetSettingsToDefaults(HWND hwnd);
    // Synchronise le mode Glass demande avec les ressources d'execution.
    void ApplyGlassEffectRuntimeMode(HWND hwnd);
    // Synchronise le runtime GlassEffect propre a la palette flottante.
    void ApplyColorPanelGlassEffectRuntimeMode();
    // Redimensionne la fenetre selon le mode d'affichage courant.
    void ApplyDisplayModeWindowSize(HWND hwnd);
    // Cree la palette flottante lors de sa premiere ouverture.
    bool EnsureColorPanelWindow(HWND owner_hwnd);
    // Replace la palette flottante contre le widget principal.
    void PositionColorPanelWindow();
    // Indique si le point vise la poignee de deplacement en click-through.
    bool IsClickThroughTitleDragHandle(HWND hwnd, POINT client_point) const;
    // Calcule l'action du panneau de couleurs situee sous un point.
    WidgetColorPanelHitTestResult HitTestColorPanel(HWND hwnd, POINT client_point) const;
    // Execute une action issue du hit-test du panneau de couleurs.
    void ExecuteColorPanelAction(HWND hwnd, const WidgetColorPanelHitTestResult& action);
    // Met a jour l'element survole dans le panneau de couleurs.
    void UpdateColorPanelHover(HWND hwnd, POINT client_point);
    // Efface les etats transitoires du panneau de couleurs.
    void ClearColorPanelInteraction(HWND hwnd);
    // Construit l'etat fonctionnel affiche par le menu contextuel.
    WidgetMenuState BuildWidgetMenuState(HWND hwnd) const;
    // Affiche le menu contextuel depuis le widget ou le tray.
    void ShowWidgetContextMenu(HWND hwnd, POINT point, bool opened_from_tray = false);
    // Change la langue de l'interface et recharge les libelles.
    void SetWidgetLanguage(HWND hwnd, UiLanguage language);
    // Demarre une recuperation asynchrone des quotas distants.
    void RefreshUsageSnapshot(HWND hwnd);
    // Applique le resultat de recuperation des quotas sur le thread UI.
    void CompleteUsageRefresh(HWND hwnd);
    // Met a jour l'infobulle tray depuis les quotas actuellement visibles.
    void RefreshTrayQuotaTooltip(HWND hwnd) const;
    // Demarre au besoin un scan asynchrone des sessions locales.
    void RefreshTokenUsageSnapshot(HWND hwnd);
    // Applique le resultat du scan local sur le thread UI.
    void CompleteTokenUsageRefresh(HWND hwnd);
    // Applique le dernier snapshot technique d'activite sur le thread UI.
    void CompleteCodexActivityRefresh(HWND hwnd);
    // Indique si le point client appartient aux controles du graphe.
    bool IsGraphInteractivePoint(HWND hwnd, POINT client_point) const;
    // Indique si le point client appartient a la veine d'activite.
    bool IsActivityVeinPoint(HWND hwnd, POINT client_point) const;
    // Met a jour le survol global de la veine d'activite.
    void UpdateActivityVeinHover(HWND hwnd, POINT client_point);
    // Efface le survol global de la veine d'activite.
    void ClearActivityVeinHover(HWND hwnd);
    // Met a jour les segments et les donnees de graphe survoles puis arme WM_MOUSELEAVE.
    void UpdateGraphHover(HWND hwnd, POINT client_point, bool non_client = false);
    // Efface les seuls etats de survol sans interrompre un clic capture.
    void ClearGraphHover(HWND hwnd);
    // Traite l'enfoncement du bouton gauche dans la zone de graphe.
    bool HandleGraphClick(HWND hwnd, POINT client_point);
    // Transforme un glissement d'onglet en deplacement natif de la fenetre.
    bool HandleGraphDrag(HWND hwnd, POINT client_point);
    // Termine un clic sur un segment et change de page si le relachement est valide.
    bool HandleGraphButtonRelease(HWND hwnd, POINT client_point);
    // Copie le contenu nettoye de l'onglet graphique actif dans le presse-papiers.
    bool CopyActiveGraphTabToClipboard(HWND hwnd);
    // ------------------------------------------------------------------------
    // Indique si un point client cible le bouton de masquage dans le tray.
    //
    // Parametres :
    // - hwnd : fenetre dont le DPI et la taille sont utilises.
    // - client_point : position client physique a tester.
    //
    // Retour :
    // - true lorsque le bouton peut recevoir le clic.
    // ------------------------------------------------------------------------
    bool IsTrayHideButtonPoint(HWND hwnd, POINT client_point) const;

    // ------------------------------------------------------------------------
    // Met a jour le survol du bouton et arme WM_MOUSELEAVE.
    //
    // Parametres :
    // - hwnd : fenetre principale a invalider au besoin.
    // - client_point : position client physique courante.
    // ------------------------------------------------------------------------
    void UpdateTrayHideButtonHover(HWND hwnd, POINT client_point);

    // ------------------------------------------------------------------------
    // Efface les etats transitoires du bouton de masquage.
    //
    // Parametres :
    // - hwnd : fenetre principale a invalider au besoin.
    // ------------------------------------------------------------------------
    void ClearTrayHideButtonInteraction(HWND hwnd);

    // ------------------------------------------------------------------------
    // Traite l'enfoncement du bouton de masquage.
    //
    // Parametres :
    // - hwnd : fenetre principale qui capture la souris.
    // - client_point : position client physique du clic.
    //
    // Retour :
    // - true lorsque le clic est consomme.
    // ------------------------------------------------------------------------
    bool HandleTrayHideButtonClick(HWND hwnd, POINT client_point);

    // ------------------------------------------------------------------------
    // Termine le clic et masque le widget si le pointeur cible encore le bouton.
    //
    // Parametres :
    // - hwnd : fenetre principale a masquer eventuellement.
    // - client_point : position client physique du relachement.
    //
    // Retour :
    // - true lorsqu'une pression du bouton etait active.
    // ------------------------------------------------------------------------
    bool HandleTrayHideButtonRelease(HWND hwnd, POINT client_point);
    // Demarre les animations numeriques entre deux snapshots d'usage.
    void UpdateRollingUsageAnimations(HWND hwnd, const UsageSnapshot& previous_snapshot, const UsageSnapshot& current_snapshot);
    // Avance les animations numeriques et rearme leur timer si necessaire.
    void ApplyRollingNumberAnimationTimer(HWND hwnd);
    // Active le timer uniquement pendant la transition du graphique Quotas.
    void ApplyQuotaGraphAnimationTimer(HWND hwnd);
    // Route un identifiant de commande vers son domaine fonctionnel.
    void HandleMenuCommand(HWND hwnd, UINT command_id);
    // Traite les commandes globales du widget.
    bool HandleGlobalCommand(HWND hwnd, UINT command_id);
    // Traite les commandes de visibilite et de positionnement.
    bool HandleVisibilityCommand(HWND hwnd, UINT command_id);
    // Traite les commandes du mode d'affichage.
    bool HandleDisplayCommand(HWND hwnd, UINT command_id);
    // Traite les commandes d'apparence et d'animation Glass.
    bool HandleGlassEffectCommand(HWND hwnd, UINT command_id);
    // Traite les commandes des effets Motion et de vibration.
    bool HandleVibrationCommand(HWND hwnd, UINT command_id);
    // Traite les commandes de collecte d'usage.
    bool HandleUsageCommand(HWND hwnd, UINT command_id);
    // Traite les commandes de palette et de personnalisation.
    bool HandleColorCommand(HWND hwnd, UINT command_id);
    // Traite les commandes de langue.
    bool HandleLanguageCommand(HWND hwnd, UINT command_id);
    // Arme ou desarme le timer de collecte periodique.
    void ApplyUsageFetchTimer(HWND hwnd);
    // Programme une sauvegarde differee des reglages.
    void ScheduleSettingsSave(HWND hwnd);
    // Arme le timer de suivi necessaire au click-through.
    void ApplyClickThroughHitTestTimer(HWND hwnd);
    // Arme le timer commun aux animations Glass actives.
    void ApplyGlassEffectAnimationTimer(HWND hwnd);
    // Arme le timer uniquement pendant une activite Codex animee.
    void ApplyActivityVeinAnimationTimer(HWND hwnd);
    // Met a jour le style Win32 traversant selon le curseur.
    bool ApplyClickThroughTransparency(HWND hwnd);
    // Rafraichit la capture Glass apres un changement de traversabilite.
    void RefreshGlassEffectAfterClickThroughMouseEvent(HWND hwnd, bool pass_through_under_cursor);
    // Capture et publie une nouvelle frame Glass pour le widget.
    bool RefreshGlassEffectFrame(HWND hwnd);
    // Capture le fond situe derriere la palette flottante.
    bool RefreshColorPanelGlassEffectFrame();
    // Demarre les captures rapides necessaires au reveil de Glass.
    void StartGlassEffectWarmupCapture(HWND hwnd);
    // Demarre une sonde de changement pendant l'inactivite.
    void StartGlassEffectIdleProbeCapture(HWND hwnd);
    // ------------------------------------------------------------------------
    // Reinitialise Glass apres une transition systeme invalidant la capture.
    //
    // Parametres :
    // - hwnd : fenetre principale dont la capture doit etre recreee.
    // - resume_capture : indique si les tentatives de reprise doivent demarrer.
    //
    // Effet de bord :
    // - libere le device et les anciennes frames, puis arme une reprise asynchrone.
    // ------------------------------------------------------------------------
    void ResetGlassEffectAfterSystemTransition(HWND hwnd, bool resume_capture);
    // Demarre les effets Motion actuellement actives.
    void StartWidgetVibration(HWND hwnd);
    // Demarre les effets Motion avec une configuration explicite.
    void StartWidgetVibration(HWND hwnd, const WidgetMotionEffectsSettings& settings);
    // Avance la timeline des effets Motion.
    void UpdateWidgetVibration(HWND hwnd);
    // Arrete les effets Motion et restaure la position stable.
    void StopWidgetVibration(HWND hwnd);
    // Arme le timer du mode de test Motion periodique.
    void ApplyWidgetVibrationTestTimer(HWND hwnd);
    // Active ou desactive le mode de test Motion periodique.
    void ToggleWidgetVibrationTestMode(HWND hwnd);
    // Declenche immediatement un test Motion.
    void TriggerWidgetVibrationTest(HWND hwnd);
    // Maintient le test et le rendu de vibration pendant un deplacement Windows.
    void UpdateVibrationDuringMove(HWND hwnd);
    // Arme le timer du test d'animation des chiffres.
    void ApplyRollingNumberTestTimer(HWND hwnd);
    // Demarre l'animation des points du libelle de rafraichissement.
    void StartRefreshingLabelAnimation(HWND hwnd);
    // Arrete l'animation des points du libelle de rafraichissement.
    void StopRefreshingLabelAnimation(HWND hwnd);
    // Active ou desactive le test periodique des chiffres.
    void ToggleRollingNumberTestMode(HWND hwnd);
    // Declenche immediatement un test d'animation des chiffres.
    void TriggerRollingNumberTest(HWND hwnd);
    // Maintient le test et le rendu des chiffres pendant un deplacement Windows.
    void UpdateRollingNumbersDuringMove(HWND hwnd);
    // Demarre les timers applicatifs lies a la fenetre.
    void StartWidgetTimers(HWND hwnd);
    // Arrete tous les timers applicatifs lies a la fenetre.
    void StopWidgetTimers(HWND hwnd);
    // Recopie la geometrie courante dans les reglages en memoire.
    void UpdateSettingsFromWindow(HWND hwnd);
    // Memorise le dernier rectangle stable de la fenetre.
    void RememberWindowRect(HWND hwnd);
    // Applique au demarrage les reglages qui concernent Windows.
    void ApplyStartupSettings();
    // Execute la boucle de messages et retourne son code de sortie.
    int RunMessageLoop();

    // Derniere position connue du widget pendant l'execution.
    RECT last_window_rect_{
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
    };

    UsageSnapshot usage_snapshot_{};
    // Derniers agregats locaux de tokens disponibles pour le rendu.
    TokenUsageSnapshot token_usage_snapshot_{};
    // Etat de survol partage avec le renderer du graphe.
    WidgetGraphInteraction graph_interaction_{};
    // Etat de survol et de pression du bouton de masquage dans le tray.
    WidgetTrayHideButtonInteraction tray_hide_button_interaction_{};
    // Etat de survol global de la veine et de son hint.
    WidgetActivityVeinInteraction activity_vein_interaction_{};
    // Point client physique memorise au debut d'un clic sur un onglet.
    std::optional<POINT> graph_press_client_point_;
    UsageRefreshInterval usage_refresh_interval_ = UsageRefreshInterval::Minute1;
    bool usage_refresh_in_progress_ = false;
    // Instant monotone auquel le statut de rafraichissement a commence.
    std::chrono::steady_clock::time_point usage_refresh_started_at_{};
    // Indique qu'un resultat rapide attend la fin de l'animation minimale.
    bool usage_refresh_completion_pending_ = false;
    AppSettings app_settings_{};
    // Worker qui conserve les ecritures INI hors du thread de rendu.
    AppSettingsSaveWorker settings_save_worker_;
    UsageHistoryStore usage_history_store_;
    UsageRefreshWorker usage_refresh_worker_;
    // Worker disque dedie aux sessions locales Codex.
    TokenUsageRefreshWorker token_usage_refresh_worker_;
    // Moniteur temps reel independant des agregats de tokens.
    CodexActivityMonitor codex_activity_monitor_;
    // Dernier etat multisession transfere au thread UI.
    CodexActivitySnapshot codex_activity_snapshot_{};
    // Machine d'etats qui stabilise les snapshots pour le futur rendu.
    WidgetActivityController widget_activity_controller_;
    // Derniere frame agregee disponible sur le thread UI.
    WidgetActivityFrame widget_activity_frame_{};
    WidgetRenderer widget_renderer_;
    WidgetRenderer color_panel_renderer_;
    std::unique_ptr<WidgetGlassEffect> glass_effect_;
    WidgetVibrationController vibration_controller_;
    WidgetVibrationAnimation vibration_animation_;
    WidgetVibrationAnimation pulse_animation_;
    WidgetVibrationAnimation wave_animation_;
    WidgetVibrationMotion vibration_motion_;
    WidgetVibrationGlassEffect vibration_glass_effect_;
    WidgetVibrationGlassEffectState vibration_glass_effect_state_{};
    WidgetMotionEffectsSettings active_motion_effects_settings_{};
    // Indique qu'un WM_MOVE courant provient du micro-deplacement de vibration.
    bool vibration_window_move_active_ = false;
    WidgetRollingNumberAnimation five_hour_rolling_animation_;
    WidgetRollingNumberAnimation weekly_rolling_animation_;
    // Fige puis anime la serie Quotas autour d'un rafraichissement asynchrone.
    WidgetQuotaGraphAnimation quota_graph_animation_;
    WidgetColorPanelInteraction color_panel_interaction_{};
    bool color_panel_open_ = false;
    HWND main_hwnd_ = nullptr;
    HWND color_panel_hwnd_ = nullptr;
    bool hidden_by_fullscreen_ = false;
    bool user_hidden_widget_ = false;
    bool glass_effect_capture_interactive_ = false;
    bool glass_effect_capture_idle_probe_ = false;
    int glass_effect_warmup_capture_attempts_ = 0;
    // Maintient le warmup tant que la palette n'a pas obtenu sa premiere frame.
    bool color_panel_glass_effect_warmup_pending_ = false;
    bool click_through_left_button_down_ = false;
    bool click_through_right_button_down_ = false;
    bool click_through_middle_button_down_ = false;
    bool vibration_test_auto_enabled_ = false;
    // Prochaine echeance monotone du test automatique de vibration.
    WidgetVibrationAnimation::TimePoint vibration_test_next_tick_{};
    bool rolling_number_test_enabled_ = false;
    // Prochaine echeance monotone du mode de test des chiffres.
    WidgetRollingNumberAnimation::TimePoint rolling_number_test_next_tick_{};
    // Nombre courant de points affiches apres le libelle de rafraichissement.
    std::size_t refreshing_label_dot_count_ = 1;
};

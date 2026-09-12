// ============================================================================
// Codex Glass - Timers applicatifs du widget
// ----------------------------------------------------------------------------
// Ce fichier centralise les identifiants et intervalles des timers Win32. Les
// handlers restent routes par WidgetApp, tandis que les helpers de demarrage et
// d'arret sont definis dans WidgetTimers.cpp.
// ============================================================================

#pragma once

#include <windows.h>

// Identifiant Win32 du timer qui rafraichit uniquement l'affichage.
constexpr UINT_PTR kDisplayRefreshTimerId = 1;

// Identifiant Win32 du timer qui recupere les donnees d'usage.
constexpr UINT_PTR kUsageFetchTimerId = 2;

// Identifiant Win32 du timer de capture GlassEffect.
constexpr UINT_PTR kGlassEffectCaptureTimerId = 3;

// Identifiant Win32 du timer de suivi click-through.
constexpr UINT_PTR kClickThroughHitTestTimerId = 4;

// Identifiant Win32 du timer de sauvegarde differee des reglages.
constexpr UINT_PTR kSettingsSaveTimerId = 5;

// Identifiant Win32 du timer de vibration du widget.
constexpr UINT_PTR kVibrationAnimationTimerId = 6;

// Identifiant Win32 du timer de test automatique de vibration.
constexpr UINT_PTR kVibrationTestTimerId = 7;

// Identifiant Win32 du timer d'animation des chiffres.
constexpr UINT_PTR kRollingNumberAnimationTimerId = 8;

// Identifiant Win32 du timer de test temporaire des chiffres.
constexpr UINT_PTR kRollingNumberTestTimerId = 9;

// Identifiant Win32 du timer de rendu anime GlassEffect.
constexpr UINT_PTR kGlassEffectAnimationTimerId = 10;

// Identifiant Win32 du timer des points du libelle de rafraichissement.
constexpr UINT_PTR kRefreshingLabelAnimationTimerId = 11;

// Identifiant Win32 du timer de transition du graphique Quotas.
constexpr UINT_PTR kQuotaGraphAnimationTimerId = 12;

// Identifiant Win32 du timer de la veine d'activite Codex.
constexpr UINT_PTR kActivityVeinAnimationTimerId = 13;

// Intervalle de rafraichissement de l'affichage en millisecondes.
constexpr UINT kDisplayRefreshIntervalMs = 1000;

// Intervalle de capture GlassEffect active en millisecondes.
constexpr UINT kGlassEffectActiveCaptureIntervalMs = 1;

// Intervalle des tentatives courtes de capture GlassEffect apres un changement de geometrie.
constexpr UINT kGlassEffectWarmupCaptureIntervalMs = 16;

// Intervalle du probe GlassEffect idle qui detecte les changements du bureau.
constexpr UINT kGlassEffectIdleProbeCaptureIntervalMs = 16;

// Nombre maximal de tentatives courtes de capture GlassEffect hors deplacement.
constexpr int kGlassEffectWarmupCaptureAttempts = 30;

// Intervalle de suivi souris du click-through en millisecondes.
constexpr UINT kClickThroughHitTestIntervalMs = 50;

// Delai de regroupement des sauvegardes de reglages en millisecondes.
constexpr UINT kSettingsSaveDelayMs = 350;

// Intervalle de rendu de la vibration en millisecondes.
constexpr UINT kVibrationAnimationIntervalMs = 16;

// Intervalle du test automatique de vibration en millisecondes.
constexpr UINT kVibrationTestIntervalMs = 3000;

// Intervalle de rendu de l'animation des chiffres en millisecondes.
constexpr UINT kRollingNumberAnimationIntervalMs = 16;

// Intervalle de test temporaire des chiffres en millisecondes.
constexpr UINT kRollingNumberTestIntervalMs = 2000;

// Intervalle de rendu anime GlassEffect en millisecondes.
constexpr UINT kGlassEffectAnimationIntervalMs = 16;

// Intervalle entre deux nombres de points du libelle de rafraichissement.
constexpr UINT kRefreshingLabelAnimationIntervalMs = 250;

// Intervalle de rendu de la transition du graphique Quotas.
constexpr UINT kQuotaGraphAnimationIntervalMs = 16;

// Intervalle de rendu de la veine active, proche de 60 images par seconde.
constexpr UINT kActivityVeinAnimationIntervalMs = 16;

// Duree minimale d'affichage du libelle de rafraichissement anime.
constexpr UINT kRefreshingLabelMinimumVisibleMs = 750;

// ----------------------------------------------------------------------------
// Route les timers de vibration meme pendant une boucle modale de menu Win32.
//
// Parametres :
// - hwnd : fenetre proprietaire du timer.
// - message : message systeme reserve.
// - timer_id : identifiant du timer a transmettre au handler WM_TIMER.
// - tick_count : compteur systeme reserve.
// ----------------------------------------------------------------------------
void CALLBACK ForwardWidgetVibrationTimer(
    HWND hwnd,
    UINT message,
    UINT_PTR timer_id,
    DWORD tick_count
);

// ----------------------------------------------------------------------------
// Route le timer Glass pendant les boucles modales de deplacement Win32.
//
// Parametres :
// - hwnd : fenetre proprietaire du timer.
// - message : message systeme reserve.
// - timer_id : identifiant du timer a transmettre au handler WM_TIMER.
// - tick_count : compteur systeme reserve.
//
// Effet de bord :
// - appelle synchroniquement le traitement WM_TIMER de la fenetre principale.
// ----------------------------------------------------------------------------
void CALLBACK ForwardWidgetGlassEffectTimer(
    HWND hwnd,
    UINT message,
    UINT_PTR timer_id,
    DWORD tick_count
);

// ----------------------------------------------------------------------------
// Route le timer de la veine pendant les boucles modales de Windows.
//
// Parametres :
// - hwnd : fenetre proprietaire du timer.
// - message : message systeme reserve.
// - timer_id : identifiant du timer a transmettre au handler WM_TIMER.
// - tick_count : compteur systeme reserve.
//
// Effet de bord :
// - appelle synchroniquement le traitement WM_TIMER de la fenetre principale.
// ----------------------------------------------------------------------------
void CALLBACK ForwardWidgetActivityVeinTimer(
    HWND hwnd,
    UINT message,
    UINT_PTR timer_id,
    DWORD tick_count
);

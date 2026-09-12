// ============================================================================
// Codex Glass - Capture du widget vers le presse-papiers
// ----------------------------------------------------------------------------
// Ce fichier declare la capture visuelle complete de la fenetre principale.
// Il isole le presse-papiers et les objets GDI du routage applicatif.
// ============================================================================

#pragma once

#include <windows.h>
#include <d2d1.h>

// Message differe qui lance la capture apres la fermeture du menu contextuel.
constexpr UINT kCopyWidgetScreenshotMessage = WM_APP + 23;

// ----------------------------------------------------------------------------
// Copie l'apparence actuellement composee du widget dans le presse-papiers.
//
// Parametres :
// - hwnd : fenetre visible dont le rectangle complet doit etre capture.
//
// Retour :
// - true si une image CF_DIB a ete placee dans le presse-papiers.
// - false si la capture ou l'acces au presse-papiers a echoue.
// ----------------------------------------------------------------------------
bool CopyWidgetScreenshotToClipboard(HWND hwnd);

// ----------------------------------------------------------------------------
// Copie une cible Direct2D hors ecran dans le presse-papiers.
//
// Parametres :
// - hwnd : fenetre proprietaire du presse-papiers.
// - render_target : cible bitmap compatible GDI contenant l'image finale.
//
// Retour :
// - true si l'image a ete publiee au format CF_DIB.
// - false si la cible ou le presse-papiers est indisponible.
// ----------------------------------------------------------------------------
bool CopyD2DRenderTargetScreenshotToClipboard(
    HWND hwnd,
    ID2D1BitmapRenderTarget* render_target
);

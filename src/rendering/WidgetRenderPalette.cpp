// ============================================================================
// Codex Glass - Implementation de la palette Direct2D
// ----------------------------------------------------------------------------
// Ce fichier convertit les couleurs Win32 persistantes en couleurs Direct2D
// simples, sans connaitre les ressources graphiques du renderer.
// ============================================================================

#include "WidgetRenderPalette.h"

// ----------------------------------------------------------------------------
// Convertit une couleur Win32 en couleur Direct2D.
//
// Parametres :
// - color : couleur Win32 a convertir.
// - alpha : opacite Direct2D a appliquer.
//
// Retour :
// - couleur Direct2D normalisee.
// ----------------------------------------------------------------------------
D2D1_COLOR_F ColorRefToD2DColor(COLORREF color, float alpha) {
    return D2D1::ColorF(
        static_cast<float>(GetRValue(color)) / 255.0F,
        static_cast<float>(GetGValue(color)) / 255.0F,
        static_cast<float>(GetBValue(color)) / 255.0F,
        alpha
    );
}

// ----------------------------------------------------------------------------
// Retourne une palette derivee des couleurs actuellement configurees.
//
// Parametres :
// - settings : reglages contenant les couleurs utilisateur.
//
// Retour :
// - palette Direct2D prete pour la creation des brosses.
// ----------------------------------------------------------------------------
Palette ActivePalette(const AppSettings& settings) {
    return Palette{
        ColorRefToD2DColor(settings.colors.background),
        ColorRefToD2DColor(settings.colors.background),
        ColorRefToD2DColor(settings.colors.border),
        ColorRefToD2DColor(settings.colors.text),
        ColorRefToD2DColor(settings.colors.text),
        ColorRefToD2DColor(settings.colors.secondary_text, 0.68F),
        ColorRefToD2DColor(settings.colors.consumed_bar),
        ColorRefToD2DColor(settings.colors.remaining_bar),
        ColorRefToD2DColor(settings.colors.history_curve),
        ColorRefToD2DColor(settings.colors.active_control),
    };
}

// ============================================================================
// Codex Deck - Reglages Glass du Workbench
// ----------------------------------------------------------------------------
// Ce module ajoute l'activation et l'opacite propres a Deck autour des reglages
// optiques et animes conserves depuis Codex Glass.
// ============================================================================

#pragma once

#include "WidgetGlassSettings.h"

// Reglages complets de l'effet Glass applique au Workbench.
struct DeckGlassSettings {
    // Active la capture et la composition Glass.
    bool enabled = true;

    // Opacite utilisateur du fond capture en pourcentage.
    int opacity_percent = 39;

    // Apparence et animations issues du moteur Codex Glass.
    GlassEffectSettings effect = [] {
        GlassEffectSettings settings = DefaultGlassEffectSettings();
        settings.preset = GlassEffectPreset::Custom;
        settings.appearance = GlassEffectAppearanceForPreset(GlassEffectPreset::Clear);
        settings.appearance.chromatic_aberration_percent = 200;
        settings.calm_water.enabled = true;
        settings.rain.enabled = true;
        return settings;
    }();
};

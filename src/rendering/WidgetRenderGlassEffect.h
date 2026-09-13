// ============================================================================
// Codex Glass - Rendu visuel GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers de rendu GlassEffect propres a l'interface.
// Le moteur de capture et de deformation reste dans `src/glass/`.
// ============================================================================

#pragma once

#include "WidgetRenderTypes.h"
#include "../glass/WidgetGlassEffectGpuProcessor.h"
#include "../glass/WidgetGlassEffectFrame.h"
#include "../glass/WidgetGlassSettings.h"
#include "../theme/ThemePalette.h"

#include <d2d1.h>
#include <wrl/client.h>
#include <vector>

// ----------------------------------------------------------------------------
// Retourne les parametres de rendu associes a un preset GlassEffect.
//
// Parametres :
// - preset : preset GlassEffect selectionne.
//
// Retour :
// - profil de rendu a appliquer.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile GlassEffectProfileForPreset(GlassEffectPreset preset);

// ----------------------------------------------------------------------------
// Transforme une apparence persistante en profil numerique de rendu.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile GlassEffectProfileForAppearance(
    const GlassEffectAppearanceSettings& appearance
);

// ----------------------------------------------------------------------------
// Applique le reglage d'opacite utilisateur au profil GlassEffect.
//
// Parametres :
// - profile : profil de rendu issu du preset courant.
// - opacity : opacite utilisateur entre 0 et 1.
//
// Retour :
// - profil ajuste sans utiliser l'alpha Win32 global.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile ApplyUserOpacityToGlassEffectProfile(GlassEffectRenderProfile profile, double opacity);

// ----------------------------------------------------------------------------
// Applique les ajustements visuels des animations cumulables actives.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile ApplyAnimationsToGlassEffectProfile(
    GlassEffectRenderProfile profile,
    const GlassEffectSettings& settings
);

// ----------------------------------------------------------------------------
// Retourne le temps monotone utilise par les animations GlassEffect.
//
// Retour :
// - temps courant en secondes.
// ----------------------------------------------------------------------------
float GlassEffectAnimationTimeSeconds();

// ----------------------------------------------------------------------------
// Calcule une signature stable pour le cache des lentilles GlassEffect.
//
// Parametres :
// - lenses : lentilles appliquees a la frame GlassEffect.
//
// Retour :
// - signature compacte des lentilles.
// ----------------------------------------------------------------------------
uint64_t GlassEffectLensSignature(const std::vector<WidgetGlassEffectLens>& lenses);

// ----------------------------------------------------------------------------
// Calcule une signature stable pour le profil GlassEffect courant.
//
// Parametres :
// - profile : profil de rendu applique au bitmap capture.
//
// Retour :
// - signature compacte des parametres qui changent la frame deformee.
// ----------------------------------------------------------------------------
uint64_t GlassEffectProfileSignature(const GlassEffectRenderProfile& profile);

// ----------------------------------------------------------------------------
// Calcule une signature stable pour le cache des animations GlassEffect.
//
// Parametres :
// - animation : reglages animes appliques a la frame.
//
// Retour :
// - signature compacte des parametres d'animation.
// ----------------------------------------------------------------------------
uint64_t GlassEffectAnimationSignature(const WidgetGlassEffectAnimationSettings& animation);

// ----------------------------------------------------------------------------
// Possede le bitmap Direct2D cree depuis la derniere frame GlassEffect.
// ----------------------------------------------------------------------------
class WidgetRenderGlassEffectCache {
public:
    // ------------------------------------------------------------------------
    // Reinitialise le bitmap et les signatures de cache GlassEffect.
    // ------------------------------------------------------------------------
    void Discard();

    // ------------------------------------------------------------------------
    // Dessine le fond capture GlassEffect sous l'interface Direct2D existante.
    //
    // Parametres :
    // - render_target : cible Direct2D courante.
    // - fallback_brush : brosse de fond utilisee si le bitmap est indisponible.
    // - frame : snapshot capture a afficher.
    // - panel_rect : zone principale du widget.
    // - render_size : taille courante du render target en DIPs.
    // - palette : couleurs courantes pour la teinte.
    // - lenses : lentilles internes a appliquer avant le rendu.
    // - glass_effect_profile : profil GlassEffect courant.
    // - animation : deformation animee optionnelle.
    // ------------------------------------------------------------------------
    void DrawBackground(
        ID2D1RenderTarget* render_target,
        ID2D1Brush* fallback_brush,
        const WidgetGlassEffectFrame& frame,
        const D2D1_RECT_F& panel_rect,
        D2D1_SIZE_F render_size,
        const ThemePalette& palette,
        const std::vector<WidgetGlassEffectLens>& lenses,
        const GlassEffectRenderProfile& glass_effect_profile,
        const WidgetGlassEffectAnimationSettings& animation
    );

private:
    // ------------------------------------------------------------------------
    // Met a jour le bitmap Direct2D utilise pour le fond GlassEffect.
    //
    // Parametres :
    // - render_target : cible Direct2D courante.
    // - frame : snapshot BGRA produit par la capture GlassEffect.
    // - lenses : lentilles internes a appliquer avant le rendu.
    // - glass_effect_profile : profil GlassEffect courant.
    // - animation : deformation animee optionnelle.
    //
    // Retour :
    // - true si un bitmap est disponible.
    // ------------------------------------------------------------------------
    bool EnsureBackgroundBitmap(
        ID2D1RenderTarget* render_target,
        const WidgetGlassEffectFrame& frame,
        const std::vector<WidgetGlassEffectLens>& lenses,
        const GlassEffectRenderProfile& glass_effect_profile,
        const WidgetGlassEffectAnimationSettings& animation
    );

    // Bitmap Direct2D cree depuis la derniere frame GlassEffect.
    Microsoft::WRL::ComPtr<ID2D1Bitmap> background_bitmap_;

    // Generation de frame GlassEffect associee au bitmap Direct2D.
    uint64_t background_generation_ = 0;

    // Frame GlassEffect deformee avant creation du bitmap Direct2D.
    WidgetGlassEffectFrame distorted_frame_;

    // Processeur D3D11 utilise par Calm Water, Liquid et Rain.
    WidgetGlassEffectGpuProcessor gpu_processor_;

    // Signature des lentilles internes utilisees pour le bitmap GlassEffect.
    uint64_t lens_signature_ = 0;

    // Signature du profil de deformation utilise pour le bitmap GlassEffect.
    uint64_t profile_signature_ = 0;

    // Signature de l'animation utilisee pour le bitmap GlassEffect.
    uint64_t animation_signature_ = 0;
};

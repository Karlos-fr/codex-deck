// ============================================================================
// Codex Glass - Processeur GPU GlassEffect
// ----------------------------------------------------------------------------
// Ce module execute les animations GlassEffect dans un pixel shader D3D11.
// Il produit une frame compatible avec le pont Direct2D sans gerer le dessin UI.
// ============================================================================

#pragma once

#include "WidgetGlassEffectDistortion.h"

#include <memory>
#include <vector>

// ----------------------------------------------------------------------------
// Possede les shaders et textures reutilisables du traitement GlassEffect GPU.
// ----------------------------------------------------------------------------
class WidgetGlassEffectGpuProcessor {
public:
    // Implementation opaque exposee uniquement aux helpers internes du module.
    struct Impl;

    // ------------------------------------------------------------------------
    // Cree un processeur sans allouer de ressource D3D11.
    // ------------------------------------------------------------------------
    WidgetGlassEffectGpuProcessor();

    // ------------------------------------------------------------------------
    // Libere les ressources D3D11 possedees par le processeur.
    // ------------------------------------------------------------------------
    ~WidgetGlassEffectGpuProcessor();

    WidgetGlassEffectGpuProcessor(const WidgetGlassEffectGpuProcessor&) = delete;
    WidgetGlassEffectGpuProcessor& operator=(const WidgetGlassEffectGpuProcessor&) = delete;

    // ------------------------------------------------------------------------
    // Execute la refraction GlassEffect sur la texture GPU de la frame.
    //
    // Parametres :
    // - source : frame capturee et references D3D11 non possedees.
    // - lenses : zones de refraction des composants graphiques.
    // - settings : profil de refraction principal.
    // - animation : animation Calm Water, Liquid ou Rain a appliquer.
    // - output : frame BGRA recevant le resultat du shader.
    //
    // Retour :
    // - true si le shader a produit et relu une frame complete.
    // - false si le GPU est indisponible ou si le mode n'est pas pris en charge.
    //
    // Effet de bord :
    // - utilise temporairement le contexte D3D11 immediat de la capture.
    // ------------------------------------------------------------------------
    bool Process(
        const WidgetGlassEffectFrame& source,
        const std::vector<WidgetGlassEffectLens>& lenses,
        const WidgetGlassEffectDistortionSettings& settings,
        const WidgetGlassEffectAnimationSettings& animation,
        WidgetGlassEffectFrame& output
    );

    // ------------------------------------------------------------------------
    // Relit une texture capturee uniquement lorsque le fallback CPU l'exige.
    //
    // Parametres :
    // - source : frame portant la texture D3D11 a relire.
    // - output : frame BGRA compacte recevant les pixels.
    //
    // Retour :
    // - true si la texture a ete entierement relue.
    // ------------------------------------------------------------------------
    bool Readback(
        const WidgetGlassEffectFrame& source,
        WidgetGlassEffectFrame& output
    );

    // ------------------------------------------------------------------------
    // Libere les ressources et oublie le device D3D11 courant.
    // ------------------------------------------------------------------------
    void Discard();

private:
    // Implementation privee contenant les types et ressources D3D11.
    std::unique_ptr<Impl> impl_;
};

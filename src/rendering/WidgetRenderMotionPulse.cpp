// ============================================================================
// Codex Glass - Implementation du rendu lumineux de Pulse
// ----------------------------------------------------------------------------
// Ce fichier compose plusieurs traits Direct2D ponderes pour produire un halo
// doux accelere par la cible existante, sans imposer Direct2D 1.1 au renderer.
// ============================================================================

#include "WidgetRenderMotionPulse.h"

#include <wrl/client.h>

#include <algorithm>
#include <cmath>

namespace {

// Intensite maximale produite par le moteur Pulse.
constexpr float kMaximumPulseStrength = 0.55F;

// Rayon du panneau utilise par le halo exterieur.
constexpr float kPulseCornerRadius = 7.0F;

// Nombre de passes superposees pour le halo principal.
constexpr int kPulseHaloLayerCount = 7;

// Epaisseur de la passe la plus diffuse du halo principal.
constexpr float kPulseMaximumHaloWidth = 18.0F;

// Epaisseur de la passe la plus proche du contour lumineux.
constexpr float kPulseMinimumHaloWidth = 2.2F;

// Nombre de passes composant le front lumineux interieur.
constexpr int kPulseWaveLayerCount = 5;

// Distance minimale parcourue par le front lumineux interieur.
constexpr float kPulseMinimumTravel = 6.0F;

// Distance supplementaire maximale pilotee par le reglage Etendue.
constexpr float kPulseExtentTravel = 30.0F;

// ----------------------------------------------------------------------------
// Retourne un rectangle reduit sans inverser ses bords.
//
// Parametres :
// - rect : rectangle source.
// - inset : retrait uniforme demande.
//
// Retour :
// - rectangle interieur valide.
// ----------------------------------------------------------------------------
D2D1_RECT_F InsetPulseRect(const D2D1_RECT_F& rect, float inset) {
    const float maximum_inset = std::max(
        0.0F,
        (std::min(rect.right - rect.left, rect.bottom - rect.top) * 0.5F) - 1.0F
    );
    const float safe_inset = std::clamp(inset, 0.0F, maximum_inset);
    return D2D1::RectF(
        rect.left + safe_inset,
        rect.top + safe_inset,
        rect.right - safe_inset,
        rect.bottom - safe_inset
    );
}

// ----------------------------------------------------------------------------
// Calcule une interpolation douce entre zero et un.
//
// Parametres :
// - value : progression normalisee.
//
// Retour :
// - progression lissee.
// ----------------------------------------------------------------------------
float SmoothPulseProgress(float value) {
    const float safe_value = std::clamp(value, 0.0F, 1.0F);
    return safe_value * safe_value * (3.0F - (2.0F * safe_value));
}

} // namespace

// ----------------------------------------------------------------------------
// Dessine le halo Pulse autour du panneau et sa propagation interieure.
// ----------------------------------------------------------------------------
void DrawWidgetMotionPulse(
    ID2D1RenderTarget* render_target,
    const D2D1_RECT_F& panel_rect,
    D2D1_COLOR_F accent_color,
    const WidgetVibrationGlassEffectState& state
) {
    if (render_target == nullptr || state.border_pulse <= 0.0F) {
        return;
    }

    const float strength = std::clamp(
        state.border_pulse / kMaximumPulseStrength,
        0.0F,
        1.0F
    );
    accent_color.a = 1.0F;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accent_brush;
    if (FAILED(render_target->CreateSolidColorBrush(accent_color, accent_brush.GetAddressOf()))) {
        return;
    }

    const D2D1_ROUNDED_RECT outer_rect = D2D1::RoundedRect(
        panel_rect,
        kPulseCornerRadius,
        kPulseCornerRadius
    );
    for (int layer = 0; layer < kPulseHaloLayerCount; ++layer) {
        const float ratio = static_cast<float>(layer)
            / static_cast<float>(kPulseHaloLayerCount - 1);
        const float stroke_width = kPulseMaximumHaloWidth
            - ((kPulseMaximumHaloWidth - kPulseMinimumHaloWidth) * ratio);
        const float gaussian_weight = std::exp(-2.4F * (1.0F - ratio) * (1.0F - ratio));
        accent_brush->SetOpacity(strength * (0.018F + (0.070F * gaussian_weight)));
        render_target->DrawRoundedRectangle(outer_rect, accent_brush.Get(), stroke_width);
    }

    accent_brush->SetOpacity(strength * (0.42F + (0.30F * strength)));
    render_target->DrawRoundedRectangle(outer_rect, accent_brush.Get(), 1.15F);

    if (!state.pulse_active || state.pulse_extent <= 0.0F) {
        return;
    }

    const float phase = std::clamp(state.pulse_phase, 0.0F, 1.0F);
    const float extent = std::clamp(state.pulse_extent, 0.0F, 1.0F);
    const float travel = kPulseMinimumTravel + (kPulseExtentTravel * extent);
    const float inset = 2.0F + (travel * SmoothPulseProgress(phase));
    const float wave_visibility = std::sin(phase * 3.14159265F);
    const D2D1_RECT_F wave_rect = InsetPulseRect(panel_rect, inset);
    const float wave_radius = std::max(2.0F, kPulseCornerRadius - (inset * 0.10F));
    const D2D1_ROUNDED_RECT rounded_wave = D2D1::RoundedRect(
        wave_rect,
        wave_radius,
        wave_radius
    );
    for (int layer = 0; layer < kPulseWaveLayerCount; ++layer) {
        const float ratio = static_cast<float>(layer)
            / static_cast<float>(kPulseWaveLayerCount - 1);
        const float stroke_width = 11.0F - (9.2F * ratio);
        const float opacity = strength * wave_visibility * (0.020F + (0.075F * ratio * ratio));
        accent_brush->SetOpacity(opacity);
        render_target->DrawRoundedRectangle(rounded_wave, accent_brush.Get(), stroke_width);
    }
}

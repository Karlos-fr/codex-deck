// ============================================================================
// Codex Glass - Implementation du rendu visuel GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier regroupe les profils visuels GlassEffect utilises par le rendu du
// widget, sans gerer la capture ni l'activation applicative.
// ============================================================================

#include "WidgetRenderGlassEffect.h"

#include "WidgetRenderConstants.h"
#include "../glass/WidgetGlassEffectDistortion.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <dxgiformat.h>

// ----------------------------------------------------------------------------
// Retourne le temps monotone utilise par les animations GlassEffect.
//
// Retour :
// - temps courant en secondes.
// ----------------------------------------------------------------------------
float GlassEffectAnimationTimeSeconds() {
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start_time = Clock::now();
    const Clock::duration elapsed = Clock::now() - start_time;
    return static_cast<float>(std::chrono::duration<double>(elapsed).count());
}

// ----------------------------------------------------------------------------
// Retourne les parametres de rendu associes a un preset GlassEffect.
//
// Parametres :
// - preset : preset GlassEffect selectionne.
//
// Retour :
// - profil de rendu a appliquer.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile GlassEffectProfileForPreset(GlassEffectPreset preset) {
    switch (preset) {
    case GlassEffectPreset::Clear:
        return GlassEffectRenderProfile{
            0.82F,
            0.55F,
            0.26F,
            0.018F,
            8.0F,
            5.0F,
            WidgetGlassEffectDistortionSettings{24.0F, 42.0F, 11.0F, 0.70F, 0.45F},
        };

    case GlassEffectPreset::Subtle:
        return GlassEffectRenderProfile{
            0.76F,
            0.85F,
            0.38F,
            0.030F,
            10.0F,
            6.0F,
            WidgetGlassEffectDistortionSettings{25.0F, 48.0F, 14.0F, 0.95F, 0.60F},
        };

    case GlassEffectPreset::Strong:
        return GlassEffectRenderProfile{
            0.68F,
            1.45F,
            0.55F,
            0.060F,
            15.0F,
            11.0F,
            WidgetGlassEffectDistortionSettings{28.0F, 70.0F, 24.0F, 1.65F, 1.05F},
        };

    case GlassEffectPreset::Frosted:
    default:
        return GlassEffectRenderProfile{};
    }
}

// ----------------------------------------------------------------------------
// Transforme une apparence persistante en profil numerique de rendu.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile GlassEffectProfileForAppearance(
    const GlassEffectAppearanceSettings& appearance
) {
    const GlassEffectAppearanceSettings normalized = NormalizeGlassEffectAppearanceSettings(appearance);
    GlassEffectRenderProfile profile{};
    profile.soft_sample_offset *= static_cast<float>(normalized.diffusion_percent) / 100.0F;
    profile.tint_opacity *= static_cast<float>(normalized.tint_percent) / 100.0F;
    profile.noise_opacity *= static_cast<float>(normalized.grain_percent) / 100.0F;
    profile.progress_lens_strength = normalized.element_glass_enabled
        ? profile.progress_lens_strength * static_cast<float>(normalized.indicator_refraction_percent) / 100.0F
        : 0.0F;
    profile.progress_lens_band *= static_cast<float>(normalized.indicator_width_percent) / 100.0F;
    profile.element_lens_softness = static_cast<float>(normalized.element_softness_percent) / 100.0F;
    profile.distortion.edge_strength *= static_cast<float>(normalized.edge_refraction_percent) / 100.0F;
    profile.distortion.edge_band *= static_cast<float>(normalized.edge_width_percent) / 100.0F;
    profile.distortion.chromatic_shift *= static_cast<float>(normalized.chromatic_aberration_percent) / 100.0F;
    profile.distortion.lens_chromatic_shift *= static_cast<float>(normalized.chromatic_aberration_percent) / 100.0F;
    return profile;
}

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
GlassEffectRenderProfile ApplyUserOpacityToGlassEffectProfile(GlassEffectRenderProfile profile, double opacity) {
    const float opacity_ratio = static_cast<float>(std::clamp(
        opacity,
        kMinimumBackgroundOpacity,
        kMaximumBackgroundOpacity
    ));

    profile.background_opacity = std::clamp(
        profile.background_opacity + ((1.0F - opacity_ratio) * 0.18F),
        0.0F,
        1.0F
    );
    profile.tint_opacity *= opacity_ratio;
    profile.noise_opacity *= opacity_ratio;
    return profile;
}

// ----------------------------------------------------------------------------
// Applique une variation temporaire de vibration au profil GlassEffect.
//
// Parametres :
// - profile : profil de rendu courant.
// - vibration : variation temporaire calculee par le moteur de vibration.
//
// Retour :
// - profil ajuste pour la frame courante.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile ApplyVibrationToGlassEffectProfile(
    GlassEffectRenderProfile profile,
    const WidgetVibrationGlassEffectState& vibration
) {
    if (!IsWidgetVibrationGlassEffectActive(vibration)) {
        return profile;
    }

    profile.distortion.edge_strength += vibration.refraction_boost;
    profile.distortion.edge_band += vibration.edge_wave_strength;
    profile.tint_opacity = std::clamp(profile.tint_opacity + vibration.tint_opacity_boost, 0.0F, 0.72F);
    return profile;
}

// ----------------------------------------------------------------------------
// Compose la vague Motion transitoire avec une animation Glass existante.
// ----------------------------------------------------------------------------
WidgetGlassEffectAnimationSettings ApplyMotionWaveToGlassEffectAnimation(
    WidgetGlassEffectAnimationSettings animation,
    const WidgetVibrationGlassEffectState& motion
) {
    if (motion.wave_refraction <= 0.0F) {
        return animation;
    }

    animation.motion_wave_enabled = true;
    animation.motion_wave_progress = motion.wave_position;
    animation.motion_wave_count = motion.wave_count;
    animation.motion_wave_damping = motion.wave_damping;
    animation.motion_wave_refraction = motion.wave_refraction;
    animation.motion_wave_speed = motion.wave_speed;
    animation.motion_wave_wavelength = std::clamp(72.0F * motion.wave_wavelength / 100.0F, 18.0F, 216.0F);
    animation.motion_wave_front_undulation = motion.wave_front_undulation;
    switch (motion.wave_direction) {
    case WidgetMotionWaveDirection::Left:
        animation.motion_wave_direction_x = -1.0F;
        animation.motion_wave_direction_y = 0.0F;
        break;
    case WidgetMotionWaveDirection::Up:
        animation.motion_wave_direction_x = 0.0F;
        animation.motion_wave_direction_y = -1.0F;
        break;
    case WidgetMotionWaveDirection::Down:
        animation.motion_wave_direction_x = 0.0F;
        animation.motion_wave_direction_y = 1.0F;
        break;
    case WidgetMotionWaveDirection::Radial:
        animation.motion_wave_radial = true;
        break;
    case WidgetMotionWaveDirection::Right:
    default:
        animation.motion_wave_direction_x = 1.0F;
        animation.motion_wave_direction_y = 0.0F;
        break;
    }
    return animation;
}

// ----------------------------------------------------------------------------
// Applique les ajustements visuels des animations cumulables actives.
// ----------------------------------------------------------------------------
GlassEffectRenderProfile ApplyAnimationsToGlassEffectProfile(
    GlassEffectRenderProfile profile,
    const GlassEffectSettings& settings
) {
    if (settings.liquid.enabled) {
        profile.distortion.edge_strength += 4.0F;
        profile.distortion.edge_band += 8.0F;
    }
    if (settings.rain.enabled) {
        profile.distortion.edge_strength += 2.0F;
    }
    return profile;
}

// ----------------------------------------------------------------------------
// Calcule une signature stable pour le cache des lentilles GlassEffect.
//
// Parametres :
// - lenses : lentilles appliquees a la frame GlassEffect.
//
// Retour :
// - signature compacte des lentilles.
// ----------------------------------------------------------------------------
uint64_t GlassEffectLensSignature(const std::vector<WidgetGlassEffectLens>& lenses) {
    uint64_t signature = 1469598103934665603ULL;
    const auto mix = [&signature](int value) {
        signature ^= static_cast<uint64_t>(value);
        signature *= 1099511628211ULL;
    };

    mix(static_cast<int>(lenses.size()));
    for (const WidgetGlassEffectLens& lens : lenses) {
        mix(static_cast<int>(std::lround(lens.left * 4.0F)));
        mix(static_cast<int>(std::lround(lens.top * 4.0F)));
        mix(static_cast<int>(std::lround(lens.right * 4.0F)));
        mix(static_cast<int>(std::lround(lens.bottom * 4.0F)));
        mix(static_cast<int>(std::lround(lens.radius * 4.0F)));
        mix(static_cast<int>(std::lround(lens.band * 4.0F)));
        mix(static_cast<int>(std::lround(lens.strength * 4.0F)));
        mix(static_cast<int>(std::lround(lens.softness * 100.0F)));
    }

    return signature;
}

// ----------------------------------------------------------------------------
// Calcule une signature stable pour le profil GlassEffect courant.
//
// Parametres :
// - profile : profil de rendu applique au bitmap capture.
//
// Retour :
// - signature compacte des parametres qui changent la frame deformee.
// ----------------------------------------------------------------------------
uint64_t GlassEffectProfileSignature(const GlassEffectRenderProfile& profile) {
    uint64_t signature = 1469598103934665603ULL;
    const auto mix = [&signature](int value) {
        signature ^= static_cast<uint64_t>(value);
        signature *= 1099511628211ULL;
    };

    mix(static_cast<int>(std::lround(profile.progress_lens_band * 4.0F)));
    mix(static_cast<int>(std::lround(profile.progress_lens_strength * 4.0F)));
    mix(static_cast<int>(std::lround(profile.element_lens_softness * 100.0F)));
    mix(static_cast<int>(std::lround(profile.distortion.glass_corner_radius * 4.0F)));
    mix(static_cast<int>(std::lround(profile.distortion.edge_band * 4.0F)));
    mix(static_cast<int>(std::lround(profile.distortion.edge_strength * 4.0F)));
    mix(static_cast<int>(std::lround(profile.distortion.chromatic_shift * 4.0F)));
    mix(static_cast<int>(std::lround(profile.distortion.lens_chromatic_shift * 4.0F)));
    return signature;
}

// ----------------------------------------------------------------------------
// Calcule une signature stable pour le cache des animations GlassEffect.
//
// Parametres :
// - animation : reglages animes appliques a la frame.
//
// Retour :
// - signature compacte des parametres d'animation.
// ----------------------------------------------------------------------------
uint64_t GlassEffectAnimationSignature(const WidgetGlassEffectAnimationSettings& animation) {
    if (!animation.calm_water.enabled
        && !animation.liquid.enabled
        && !animation.rain.enabled
        && !animation.motion_wave_enabled) {
        return 0;
    }

    uint64_t signature = 1469598103934665603ULL;
    const auto mix = [&signature](int value) {
        signature ^= static_cast<uint64_t>(value);
        signature *= 1099511628211ULL;
    };

    const auto mix_channel = [&mix](const WidgetGlassEffectAnimationSettings::Channel& channel) {
        mix(channel.enabled ? 1 : 0);
        mix(static_cast<int>(std::lround(channel.amplitude * 16.0F)));
        mix(static_cast<int>(std::lround(channel.speed * 16.0F)));
        mix(static_cast<int>(std::lround(channel.wavelength * 4.0F)));
        mix(static_cast<int>(std::lround(channel.noise * 32.0F)));
        mix(static_cast<int>(std::lround(channel.fluidity * 32.0F)));
        mix(static_cast<int>(std::lround(channel.density * 32.0F)));
        mix(static_cast<int>(std::lround(channel.ring_radius * 4.0F)));
        mix(static_cast<int>(std::lround(channel.fade * 32.0F)));
    };
    mix_channel(animation.calm_water);
    mix_channel(animation.liquid);
    mix_channel(animation.rain);
    mix(static_cast<int>(std::lround(animation.time_seconds * 60.0F)));
    mix(animation.motion_wave_enabled ? 1 : 0);
    mix(static_cast<int>(std::lround(animation.motion_wave_progress * 240.0F)));
    mix(static_cast<int>(std::lround(animation.motion_wave_refraction * 16.0F)));
    mix(animation.motion_wave_count);
    mix(static_cast<int>(std::lround(animation.motion_wave_direction_x * 10.0F)));
    mix(static_cast<int>(std::lround(animation.motion_wave_direction_y * 10.0F)));
    mix(animation.motion_wave_radial ? 1 : 0);
    mix(static_cast<int>(std::lround(animation.motion_wave_damping * 100.0F)));
    mix(static_cast<int>(std::lround(animation.motion_wave_speed * 100.0F)));
    mix(static_cast<int>(std::lround(animation.motion_wave_wavelength * 4.0F)));
    mix(static_cast<int>(std::lround(animation.motion_wave_front_undulation * 100.0F)));
    return signature;
}

// ----------------------------------------------------------------------------
// Reinitialise le bitmap et les signatures de cache GlassEffect.
// ----------------------------------------------------------------------------
void WidgetRenderGlassEffectCache::Discard() {
    background_bitmap_.Reset();
    distorted_frame_ = WidgetGlassEffectFrame{};
    background_generation_ = 0;
    lens_signature_ = 0;
    profile_signature_ = 0;
    animation_signature_ = 0;
}

// ----------------------------------------------------------------------------
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
// ----------------------------------------------------------------------------
bool WidgetRenderGlassEffectCache::EnsureBackgroundBitmap(
    ID2D1RenderTarget* render_target,
    const WidgetGlassEffectFrame& frame,
    const std::vector<WidgetGlassEffectLens>& lenses,
    const GlassEffectRenderProfile& glass_effect_profile,
    const WidgetGlassEffectAnimationSettings& animation
) {
    const uint64_t lens_signature = GlassEffectLensSignature(lenses);
    const uint64_t profile_signature = GlassEffectProfileSignature(glass_effect_profile);
    const uint64_t animation_signature = GlassEffectAnimationSignature(animation);
    if (
        background_bitmap_
        && background_generation_ == frame.generation
        && lens_signature_ == lens_signature
        && profile_signature_ == profile_signature
        && animation_signature_ == animation_signature
    ) {
        return true;
    }

    Discard();

    const bool frame_available = !frame.pixels.empty() || frame.gpu_texture != nullptr;
    if (render_target == nullptr
        || frame.width == 0
        || frame.height == 0
        || frame.stride == 0
        || !frame_available) {
        return false;
    }

    const D2D1_BITMAP_PROPERTIES bitmap_properties = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
    );

    const bool gpu_processed = gpu_processor_.Process(
        frame,
        lenses,
        glass_effect_profile.distortion,
        animation,
        distorted_frame_
    );
    if (!gpu_processed) {
        const WidgetGlassEffectFrame* cpu_source = &frame;
        WidgetGlassEffectFrame readback_frame;
        if (cpu_source->pixels.empty()) {
            if (!gpu_processor_.Readback(frame, readback_frame)) {
                return false;
            }
            cpu_source = &readback_frame;
        }
        distorted_frame_ = DistortGlassEffectFrame(
            *cpu_source,
            lenses,
            glass_effect_profile.distortion,
            animation
        );
    }

    const HRESULT result = render_target->CreateBitmap(
        D2D1::SizeU(distorted_frame_.width, distorted_frame_.height),
        distorted_frame_.pixels.data(),
        distorted_frame_.stride,
        bitmap_properties,
        background_bitmap_.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    background_generation_ = frame.generation;
    lens_signature_ = lens_signature;
    profile_signature_ = profile_signature;
    animation_signature_ = animation_signature;
    return true;
}

// ----------------------------------------------------------------------------
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
// ----------------------------------------------------------------------------
void WidgetRenderGlassEffectCache::DrawBackground(
    ID2D1RenderTarget* render_target,
    ID2D1Brush* fallback_brush,
    const WidgetGlassEffectFrame& frame,
    const D2D1_RECT_F& panel_rect,
    D2D1_SIZE_F render_size,
    const Palette& palette,
    const std::vector<WidgetGlassEffectLens>& lenses,
    const GlassEffectRenderProfile& glass_effect_profile,
    const WidgetGlassEffectAnimationSettings& animation
) {
    const float horizontal_scale = render_size.width > 0.0F
        ? static_cast<float>(frame.width) / render_size.width
        : 1.0F;
    const float vertical_scale = render_size.height > 0.0F
        ? static_cast<float>(frame.height) / render_size.height
        : 1.0F;
    const float effect_scale = (horizontal_scale + vertical_scale) * 0.5F;
    std::vector<WidgetGlassEffectLens> frame_lenses = lenses;
    for (WidgetGlassEffectLens& lens : frame_lenses) {
        lens.left *= horizontal_scale;
        lens.right *= horizontal_scale;
        lens.top *= vertical_scale;
        lens.bottom *= vertical_scale;
        lens.radius *= effect_scale;
        lens.band *= effect_scale;
        lens.strength *= effect_scale;
    }
    if (!EnsureBackgroundBitmap(render_target, frame, frame_lenses, glass_effect_profile, animation)) {
        if (render_target != nullptr && fallback_brush != nullptr) {
            render_target->FillRectangle(panel_rect, fallback_brush);
        }
        return;
    }

    const std::array<D2D1_POINT_2F, 5> offsets{
        D2D1::Point2F(0.0F, 0.0F),
        D2D1::Point2F(-glass_effect_profile.soft_sample_offset, 0.0F),
        D2D1::Point2F(glass_effect_profile.soft_sample_offset, 0.0F),
        D2D1::Point2F(0.0F, -glass_effect_profile.soft_sample_offset),
        D2D1::Point2F(0.0F, glass_effect_profile.soft_sample_offset),
    };

    for (const D2D1_POINT_2F& offset : offsets) {
        const D2D1_RECT_F target_rect = D2D1::RectF(
            panel_rect.left + offset.x,
            panel_rect.top + offset.y,
            panel_rect.right + offset.x,
            panel_rect.bottom + offset.y
        );
        const D2D1_RECT_F source_rect = D2D1::RectF(
            render_size.width > 0.0F ? (target_rect.left / render_size.width) * static_cast<float>(frame.width) : 0.0F,
            render_size.height > 0.0F ? (target_rect.top / render_size.height) * static_cast<float>(frame.height) : 0.0F,
            render_size.width > 0.0F ? (target_rect.right / render_size.width) * static_cast<float>(frame.width) : static_cast<float>(frame.width),
            render_size.height > 0.0F ? (target_rect.bottom / render_size.height) * static_cast<float>(frame.height) : static_cast<float>(frame.height)
        );
        render_target->DrawBitmap(
            background_bitmap_.Get(),
            target_rect,
            glass_effect_profile.background_opacity / static_cast<float>(offsets.size()),
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            source_rect
        );
    }

    D2D1_COLOR_F tint_color = palette.panel_background;
    tint_color.a = glass_effect_profile.tint_opacity;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> tint_brush;
    if (SUCCEEDED(render_target->CreateSolidColorBrush(tint_color, tint_brush.GetAddressOf()))) {
        render_target->FillRectangle(panel_rect, tint_brush.Get());
    }

    D2D1_COLOR_F noise_color = palette.title_text;
    noise_color.a = glass_effect_profile.noise_opacity;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> noise_brush;
    if (FAILED(render_target->CreateSolidColorBrush(noise_color, noise_brush.GetAddressOf()))) {
        return;
    }

    // Espacement du bruit discret pose sur le fond GlassEffect.
    constexpr float noise_step = 11.0F;

    // Taille des points de bruit discret du fond GlassEffect.
    constexpr float noise_size = 0.75F;
    int row_index = 0;
    for (float y = panel_rect.top + 3.0F; y < panel_rect.bottom; y += noise_step, ++row_index) {
        for (float x = panel_rect.left + 5.0F + static_cast<float>((row_index % 3) * 2); x < panel_rect.right; x += noise_step) {
            render_target->FillRectangle(
                D2D1::RectF(x, y, x + noise_size, y + noise_size),
                noise_brush.Get()
            );
        }
    }
}

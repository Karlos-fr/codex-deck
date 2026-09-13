// ============================================================================
// Codex Glass - Implementation de la distortion GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier applique une refraction CPU lisible : SDF de rectangle arrondi,
// normales de bord, deformation UV et aberration chromatique. Le shader HLSL
// de phase I garde la cible GPU equivalente.
// ============================================================================

#include "WidgetGlassEffectDistortion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Vecteur 2D leger pour le calcul de SDF.
// ----------------------------------------------------------------------------
struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;
};

// ----------------------------------------------------------------------------
// Forme de refraction evaluee dans la frame capturee.
// ----------------------------------------------------------------------------
struct RefractionShape {
    Vec2 center{};
    Vec2 half_size{};
    float radius = 0.0F;
    float band = 0.0F;
    float strength = 0.0F;
    float chromatic_shift = 0.0F;
    bool inside_only = false;
    float sample_direction = -1.0F;
    bool surface_wave = false;
    float softness = 1.0F;
};

// ----------------------------------------------------------------------------
// Resultat d'evaluation de refraction pour un pixel.
// ----------------------------------------------------------------------------
struct RefractionSample {
    Vec2 normal{};
    float amount = 0.0F;
    float chromatic_shift = 0.0F;
    float sample_direction = -1.0F;
};

// ----------------------------------------------------------------------------
// Clamp flottant local.
// ----------------------------------------------------------------------------
float Clamp(float value, float minimum, float maximum) {
    return std::min(maximum, std::max(minimum, value));
}

// ----------------------------------------------------------------------------
// Interpolation douce entre deux bornes.
// ----------------------------------------------------------------------------
float SmoothStep(float edge0, float edge1, float value) {
    const float t = Clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
}

// ----------------------------------------------------------------------------
// Indique si une deformation animee est active.
//
// Parametres :
// - animation : reglages animes a tester.
//
// Retour :
// - true si la deformation animee doit modifier la frame.
// ----------------------------------------------------------------------------
bool HasActiveAnimation(const WidgetGlassEffectAnimationSettings& animation) {
    const bool glass_animation = (animation.calm_water.enabled && animation.calm_water.amplitude > 0.0F)
        || (animation.liquid.enabled && animation.liquid.amplitude > 0.0F)
        || (animation.rain.enabled && animation.rain.amplitude > 0.0F);
    const bool motion_wave = animation.motion_wave_enabled
        && animation.motion_wave_refraction > 0.0F
        && animation.motion_wave_wavelength > 1.0F;
    return glass_animation || motion_wave;
}

// ----------------------------------------------------------------------------
// Calcule une attenuation douce pres des bords de frame.
//
// Parametres :
// - x : coordonnee horizontale du pixel.
// - y : coordonnee verticale du pixel.
// - frame : frame source utilisee pour connaitre les dimensions.
//
// Retour :
// - coefficient entre 0 et 1.
// ----------------------------------------------------------------------------
float EdgeFade(uint32_t x, uint32_t y, const WidgetGlassEffectFrame& frame) {
    // Largeur de fondu qui raccorde la deformation au contenu central.
    constexpr float fade_width = 18.0F;
    const float left = static_cast<float>(x);
    const float top = static_cast<float>(y);
    const float right = static_cast<float>(frame.width - 1U - x);
    const float bottom = static_cast<float>(frame.height - 1U - y);
    const float distance = std::min(std::min(left, right), std::min(top, bottom));
    return SmoothStep(0.0F, fade_width, distance);
}

// ----------------------------------------------------------------------------
// Calcule une valeur pseudo-aleatoire stable entre 0 et 1.
//
// Parametres :
// - seed : valeur source utilisee pour decorreler les gouttes.
//
// Retour :
// - valeur pseudo-aleatoire stable.
// ----------------------------------------------------------------------------
float Hash01(float seed) {
    const float value = std::sin(seed * 12.9898F) * 43758.5453F;
    return value - std::floor(value);
}

// ----------------------------------------------------------------------------
// Calcule le decalage radial produit par une goutte de pluie.
//
// Parametres :
// - position : position pixel evaluee.
// - center : centre de l'impact de goutte.
// - age : progression de l'impact entre 0 et 1.
// - strength : amplitude de deformation.
// - ring_radius : rayon maximal de l'anneau en pixels.
//
// Retour :
// - decalage de sampling en pixels.
// ----------------------------------------------------------------------------
Vec2 RainDropRippleOffset(Vec2 position, Vec2 center, float age, float strength, float ring_radius) {
    const Vec2 delta{
        position.x - center.x,
        position.y - center.y,
    };
    const float distance = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (distance <= 0.001F) {
        return Vec2{};
    }

    const float radius = 10.0F + (age * ring_radius);
    const float band = 5.5F + (age * std::max(3.0F, ring_radius * 0.086F));
    const float ring_distance = std::fabs(distance - radius);
    if (ring_distance > band) {
        return Vec2{};
    }

    const float ring = 1.0F - SmoothStep(0.0F, band, ring_distance);
    const float decay = (1.0F - age) * (1.0F - age);
    const float ripple_wave = std::sin((distance - radius) * 0.62F);
    const float amount = ring * decay * ripple_wave * strength;
    return Vec2{
        (delta.x / distance) * amount,
        (delta.y / distance) * amount,
    };
}

// ----------------------------------------------------------------------------
// Calcule le decalage anime de gouttes vues du dessus avec anneaux d'impact.
//
// Parametres :
// - animation : reglages animes courants.
// - position : position pixel evaluee.
// - frame : frame source utilisee pour placer les gouttes.
//
// Retour :
// - decalage de sampling en pixels.
// ----------------------------------------------------------------------------
Vec2 AnimatedRainOffset(
    const WidgetGlassEffectAnimationSettings& animation,
    const WidgetGlassEffectAnimationSettings::Channel& rain,
    Vec2 position,
    const WidgetGlassEffectFrame& frame
) {
    const int rain_drop_count = std::clamp(static_cast<int>(std::lround(11.0F * rain.density)), 3, 28);
    const float time = animation.time_seconds * rain.speed;
    Vec2 offset{};

    for (int index = 0; index < rain_drop_count; ++index) {
        const float seed = static_cast<float>(index + 1);
        const float cycle_offset = Hash01(seed * 2.31F);
        const float cycle_speed = 0.28F + (Hash01(seed * 3.19F) * 0.18F);
        const float cycle = (time * cycle_speed) + cycle_offset;
        const float cycle_index = std::floor(cycle);
        const float normalized_age = cycle - cycle_index;
        const float center_x = (0.08F + (Hash01(seed * 5.47F + cycle_index * 11.17F) * 0.84F))
            * static_cast<float>(frame.width - 1U);
        const float center_y = (0.10F + (Hash01(seed * 8.91F + cycle_index * 13.31F) * 0.80F))
            * static_cast<float>(frame.height - 1U);
        const Vec2 drop_center{center_x, center_y};
        const Vec2 ripple = RainDropRippleOffset(
            position,
            drop_center,
            normalized_age,
            rain.amplitude * 2.1F,
            rain.ring_radius
        );
        offset.x += ripple.x;
        offset.y += ripple.y;

        const float drop_distance = std::sqrt(
            ((position.x - drop_center.x) * (position.x - drop_center.x))
            + ((position.y - drop_center.y) * (position.y - drop_center.y))
        );
        const float drop_highlight = 1.0F - SmoothStep(0.0F, 5.5F, drop_distance);
        const float impact_flash = 1.0F - SmoothStep(0.0F, 0.16F * rain.fade, normalized_age);
        offset.y += drop_highlight * rain.amplitude * 1.25F * impact_flash;
    }

    return offset;
}

// Amplitude maximale du relief du front relativement a la longueur d'onde.
constexpr float kMotionWaveMaximumFrontUndulationRatio = 0.32F;

// Periode transversale du relief relativement a la longueur d'onde.
constexpr float kMotionWaveFrontUndulationPeriodRatio = 1.65F;

// Nombre de lobes utilises pour onduler un front radial.
constexpr float kMotionWaveRadialFrontLobeCount = 6.0F;

// Donnee spatiale Wave reutilisee entre les frames d'une meme geometrie.
struct MotionWaveSpatialPoint {
    float coordinate = 0.0F;
    float normal_x = 1.0F;
    float normal_y = 0.0F;
    float edge_fade = 0.0F;
    // Relief normalise applique localement au front de vague.
    float front_undulation_unit = 0.0F;
};

// Cache des donnees Wave qui ne dependent pas de la progression temporelle.
struct MotionWaveSpatialCache {
    uint32_t width = 0;
    uint32_t height = 0;
    float direction_x = 0.0F;
    float direction_y = 0.0F;
    bool radial = false;
    float wavelength = 0.0F;
    int wave_count = 0;
    float travel_length = 1.0F;
    std::vector<MotionWaveSpatialPoint> points{};
};

// Deux caches limites au thread couvrent le widget et son panneau attache.
thread_local std::array<MotionWaveSpatialCache, 2> g_motion_wave_spatial_caches{};

// Index circulaire du prochain cache spatial a remplacer.
thread_local size_t g_motion_wave_next_cache_index = 0;

// ----------------------------------------------------------------------------
// Reconstruit le cache spatial uniquement lorsque la geometrie Wave change.
// ----------------------------------------------------------------------------
const MotionWaveSpatialCache& EnsureMotionWaveSpatialCache(
    const WidgetGlassEffectAnimationSettings& animation,
    const WidgetGlassEffectFrame& frame
) {
    for (const MotionWaveSpatialCache& candidate : g_motion_wave_spatial_caches) {
        if (candidate.width == frame.width
            && candidate.height == frame.height
            && candidate.direction_x == animation.motion_wave_direction_x
            && candidate.direction_y == animation.motion_wave_direction_y
            && candidate.radial == animation.motion_wave_radial
            && candidate.wavelength == animation.motion_wave_wavelength
            && candidate.wave_count == animation.motion_wave_count) {
            return candidate;
        }
    }

    MotionWaveSpatialCache& cache = g_motion_wave_spatial_caches[
        g_motion_wave_next_cache_index++ % g_motion_wave_spatial_caches.size()
    ];
    cache = MotionWaveSpatialCache{};
    cache.width = frame.width;
    cache.height = frame.height;
    cache.direction_x = animation.motion_wave_direction_x;
    cache.direction_y = animation.motion_wave_direction_y;
    cache.radial = animation.motion_wave_radial;
    cache.wavelength = animation.motion_wave_wavelength;
    cache.wave_count = animation.motion_wave_count;
    cache.points.resize(static_cast<size_t>(frame.width) * frame.height);

    const float width = std::max(1.0F, static_cast<float>(frame.width - 1U));
    const float height = std::max(1.0F, static_cast<float>(frame.height - 1U));
    const Vec2 center{width * 0.5F, height * 0.5F};
    Vec2 fixed_normal{animation.motion_wave_direction_x, animation.motion_wave_direction_y};
    const float direction_length = std::sqrt(
        (fixed_normal.x * fixed_normal.x) + (fixed_normal.y * fixed_normal.y)
    );
    fixed_normal.x /= std::max(0.001F, direction_length);
    fixed_normal.y /= std::max(0.001F, direction_length);
    const float minimum_projection = std::min(0.0F, width * fixed_normal.x)
        + std::min(0.0F, height * fixed_normal.y);
    const float maximum_projection = std::max(0.0F, width * fixed_normal.x)
        + std::max(0.0F, height * fixed_normal.y);
    cache.travel_length = animation.motion_wave_radial
        ? std::sqrt((center.x * center.x) + (center.y * center.y))
        : std::max(1.0F, maximum_projection - minimum_projection);

    for (uint32_t y = 0; y < frame.height; ++y) {
        for (uint32_t x = 0; x < frame.width; ++x) {
            MotionWaveSpatialPoint& point = cache.points[static_cast<size_t>(y) * frame.width + x];
            point.edge_fade = EdgeFade(x, y, frame);
            if (animation.motion_wave_radial) {
                const float delta_x = static_cast<float>(x) - center.x;
                const float delta_y = static_cast<float>(y) - center.y;
                point.coordinate = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));
                point.front_undulation_unit = std::sin(
                    std::atan2(delta_y, delta_x) * kMotionWaveRadialFrontLobeCount
                );
                if (point.coordinate > 0.001F) {
                    point.normal_x = delta_x / point.coordinate;
                    point.normal_y = delta_y / point.coordinate;
                }
            } else {
                point.normal_x = fixed_normal.x;
                point.normal_y = fixed_normal.y;
                point.coordinate = (static_cast<float>(x) * fixed_normal.x)
                    + (static_cast<float>(y) * fixed_normal.y)
                    - minimum_projection;
                const float transverse = (-static_cast<float>(x) * fixed_normal.y)
                    + (static_cast<float>(y) * fixed_normal.x);
                const float undulation_period = std::max(
                    1.0F,
                    animation.motion_wave_wavelength * kMotionWaveFrontUndulationPeriodRatio
                );
                point.front_undulation_unit = std::sin(
                    transverse * 6.2831853F / undulation_period
                );
            }
        }
    }
    return cache;
}

// ----------------------------------------------------------------------------
// Calcule le champ de deplacement d'une vague Motion vue du dessus.
// ----------------------------------------------------------------------------
Vec2 MotionWaveOffset(
    const WidgetGlassEffectAnimationSettings& animation,
    Vec2 position,
    const WidgetGlassEffectFrame& frame
) {
    if (!animation.motion_wave_enabled || animation.motion_wave_refraction <= 0.0F) {
        return Vec2{};
    }

    const MotionWaveSpatialCache& cache = EnsureMotionWaveSpatialCache(animation, frame);
    const uint32_t x = std::min(static_cast<uint32_t>(position.x), frame.width - 1U);
    const uint32_t y = std::min(static_cast<uint32_t>(position.y), frame.height - 1U);
    const MotionWaveSpatialPoint& point = cache.points[static_cast<size_t>(y) * frame.width + x];
    const float progress = Clamp(
        animation.motion_wave_progress * std::max(0.25F, animation.motion_wave_speed),
        0.0F,
        1.35F
    );
    const float front = progress * (cache.travel_length + animation.motion_wave_wavelength);
    const float front_undulation = point.front_undulation_unit
        * animation.motion_wave_wavelength
        * kMotionWaveMaximumFrontUndulationRatio
        * Clamp(animation.motion_wave_front_undulation, 0.0F, 1.0F);
    const float behind_front = front + front_undulation - point.coordinate;
    const float train_length = animation.motion_wave_wavelength * static_cast<float>(std::max(1, animation.motion_wave_count));
    if (behind_front < 0.0F || behind_front > train_length) {
        return Vec2{};
    }
    const float phase = behind_front * 6.2831853F / std::max(2.0F, animation.motion_wave_wavelength);
    const float attack = SmoothStep(0.0F, animation.motion_wave_wavelength * 0.18F, behind_front);
    const float tail = 1.0F - SmoothStep(train_length * 0.58F, train_length, behind_front);
    const float spatial_damping = std::pow(
        std::max(0.0F, 1.0F - (behind_front / std::max(1.0F, train_length))),
        0.35F + (2.4F * animation.motion_wave_damping)
    );
    const float amount = std::sin(phase) * attack * tail * spatial_damping
        * animation.motion_wave_refraction * point.edge_fade;
    return Vec2{point.normal_x * amount, point.normal_y * amount};
}

// ----------------------------------------------------------------------------
// Calcule le decalage anime de type eau pour une position de frame.
//
// Parametres :
// - animation : reglages animes courants.
// - position : position pixel evaluee.
// - frame : frame source utilisee pour attenuer les bords.
//
// Retour :
// - decalage de sampling en pixels.
// ----------------------------------------------------------------------------
Vec2 AnimatedWaterOffset(const WidgetGlassEffectAnimationSettings& animation, Vec2 position, const WidgetGlassEffectFrame& frame) {
    if (!HasActiveAnimation(animation)) {
        return Vec2{};
    }

    const Vec2 motion_wave = MotionWaveOffset(animation, position, frame);
    float offset_x = motion_wave.x;
    float offset_y = motion_wave.y;

    const auto add_water = [&](const WidgetGlassEffectAnimationSettings::Channel& channel, bool liquid) {
        if (!channel.enabled || channel.amplitude <= 0.0F) {
            return;
        }
        const float time = animation.time_seconds * channel.speed;
        const float wave_scale = 6.2831853F / std::max(1.0F, channel.wavelength);
        const float primary = std::sin((position.x * wave_scale) + (position.y * wave_scale * 0.35F) + time);
        const float secondary = std::sin((position.y * wave_scale * (liquid ? 0.83F : 1.2F)) - (time * 0.72F));
        const float diagonal = std::sin(((position.x + position.y) * wave_scale * 0.62F) + (time * (liquid ? 0.31F : 0.48F)));
        const float boost = liquid ? 1.15F + (channel.fluidity * 0.20F) : 1.0F;
        offset_x += ((primary * 0.72F) + (diagonal * 0.28F)) * channel.amplitude * boost;
        offset_y += ((secondary * 0.62F) - (diagonal * 0.24F)) * channel.amplitude * boost;
        const float shimmer = std::sin((position.x * 0.113F) + (position.y * 0.071F) + (time * 1.37F));
        offset_x += shimmer * channel.noise;
        offset_y -= shimmer * channel.noise * 0.55F;
    };

    add_water(animation.calm_water, false);
    add_water(animation.liquid, true);
    if (animation.rain.enabled && animation.rain.amplitude > 0.0F) {
        const Vec2 rain_offset = AnimatedRainOffset(animation, animation.rain, position, frame);
        offset_x += rain_offset.x;
        offset_y += rain_offset.y;
        const float rain_time = animation.time_seconds * animation.rain.speed;
        const float shimmer = std::sin((position.x * 0.097F) + (position.y * 0.083F) + rain_time);
        offset_x += shimmer * animation.rain.noise;
        offset_y -= shimmer * animation.rain.noise * 0.45F;
    }

    const float offset_length = std::sqrt((offset_x * offset_x) + (offset_y * offset_y));
    if (offset_length > 18.0F) {
        const float scale = 18.0F / offset_length;
        offset_x *= scale;
        offset_y *= scale;
    }

    const float fade = EdgeFade(static_cast<uint32_t>(position.x), static_cast<uint32_t>(position.y), frame);
    return Vec2{offset_x * fade, offset_y * fade};
}

// ----------------------------------------------------------------------------
// Distance signee a un rectangle arrondi centre.
// ----------------------------------------------------------------------------
float RoundedRectSdf(Vec2 point, Vec2 half_size, float radius) {
    const Vec2 q{
        std::fabs(point.x) - half_size.x + radius,
        std::fabs(point.y) - half_size.y + radius,
    };

    const Vec2 outside{
        std::max(q.x, 0.0F),
        std::max(q.y, 0.0F),
    };

    const float outside_distance = std::sqrt((outside.x * outside.x) + (outside.y * outside.y));
    const float inside_distance = std::min(std::max(q.x, q.y), 0.0F);
    return outside_distance + inside_distance - radius;
}

// ----------------------------------------------------------------------------
// Calcule une normale depuis le gradient du SDF.
// ----------------------------------------------------------------------------
Vec2 SdfNormal(Vec2 point, Vec2 half_size, float radius) {
    // Marge numerique utilisee pour detecter une geometrie vide.
    constexpr float epsilon = 1.0F;
    const float dx = RoundedRectSdf({point.x + epsilon, point.y}, half_size, radius)
        - RoundedRectSdf({point.x - epsilon, point.y}, half_size, radius);
    const float dy = RoundedRectSdf({point.x, point.y + epsilon}, half_size, radius)
        - RoundedRectSdf({point.x, point.y - epsilon}, half_size, radius);
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length <= 0.0001F) {
        return Vec2{};
    }

    return Vec2{dx / length, dy / length};
}

// ----------------------------------------------------------------------------
// Evalue une forme de refraction pour une position absolue de frame.
// ----------------------------------------------------------------------------
RefractionSample EvaluateShape(const RefractionShape& shape, Vec2 position) {
    if (shape.half_size.x <= 0.0F || shape.half_size.y <= 0.0F || shape.band <= 0.0F || shape.strength <= 0.0F) {
        return RefractionSample{};
    }

    const Vec2 point{
        position.x - shape.center.x,
        position.y - shape.center.y,
    };
    const float sdf = RoundedRectSdf(point, shape.half_size, shape.radius);
    const float edge_distance = shape.inside_only ? std::max(0.0F, -sdf) : std::fabs(sdf);
    if (edge_distance > shape.band) {
        return RefractionSample{};
    }

    const float raw_edge_amount = 1.0F - SmoothStep(0.0F, shape.band, edge_distance);
    const float softness = std::max(0.25F, shape.softness);
    const float edge_amount = std::pow(raw_edge_amount, 1.0F / softness);
    return RefractionSample{
        SdfNormal(point, shape.half_size, shape.radius),
        edge_amount * shape.strength,
        edge_amount * shape.chromatic_shift,
        shape.sample_direction,
    };
}

// ----------------------------------------------------------------------------
// Retourne la refraction dominante parmi les formes disponibles.
// ----------------------------------------------------------------------------
RefractionSample EvaluateDominantRefraction(
    const std::vector<RefractionShape>& shapes,
    Vec2 position,
    uint32_t x,
    uint32_t y
) {
    RefractionSample best{};
    for (const RefractionShape& shape : shapes) {
        RefractionSample candidate = EvaluateShape(shape, position);
        if (candidate.amount <= best.amount) {
            continue;
        }

        if (shape.surface_wave) {
            const float wave = std::sin((static_cast<float>(x) * 0.035F) + (static_cast<float>(y) * 0.022F));
            candidate.amount += wave * 2.0F * (candidate.amount / shape.strength);
        }
        best = candidate;
    }

    return best;
}

// ----------------------------------------------------------------------------
// Lit un canal BGRA avec sampling bilineaire.
// ----------------------------------------------------------------------------
float SampleChannel(const WidgetGlassEffectFrame& frame, float x, float y, int channel) {
    x = Clamp(x, 0.0F, static_cast<float>(frame.width - 1));
    y = Clamp(y, 0.0F, static_cast<float>(frame.height - 1));

    const uint32_t x0 = static_cast<uint32_t>(std::floor(x));
    const uint32_t y0 = static_cast<uint32_t>(std::floor(y));
    const uint32_t x1 = std::min<uint32_t>(x0 + 1, frame.width - 1);
    const uint32_t y1 = std::min<uint32_t>(y0 + 1, frame.height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    const auto sample = [&frame, channel](uint32_t sx, uint32_t sy) {
        const size_t index = (static_cast<size_t>(sy) * frame.stride) + (static_cast<size_t>(sx) * 4U) + channel;
        return static_cast<float>(frame.pixels[index]);
    };

    const float top = sample(x0, y0) + ((sample(x1, y0) - sample(x0, y0)) * tx);
    const float bottom = sample(x0, y1) + ((sample(x1, y1) - sample(x0, y1)) * tx);
    return top + ((bottom - top) * ty);
}

// ----------------------------------------------------------------------------
// Ecrit un pixel BGRA dans la frame cible.
// ----------------------------------------------------------------------------
void WritePixel(WidgetGlassEffectFrame& frame, uint32_t x, uint32_t y, float blue, float green, float red, float alpha) {
    const size_t index = (static_cast<size_t>(y) * frame.stride) + (static_cast<size_t>(x) * 4U);
    frame.pixels[index + 0] = static_cast<uint8_t>(Clamp(blue, 0.0F, 255.0F));
    frame.pixels[index + 1] = static_cast<uint8_t>(Clamp(green, 0.0F, 255.0F));
    frame.pixels[index + 2] = static_cast<uint8_t>(Clamp(red, 0.0F, 255.0F));
    frame.pixels[index + 3] = static_cast<uint8_t>(Clamp(alpha, 0.0F, 255.0F));
}

}  // namespace

// ----------------------------------------------------------------------------
// Transforme les trois animations persistantes en reglages runtime cumulables.
// ----------------------------------------------------------------------------
WidgetGlassEffectAnimationSettings GlassEffectAnimationSettingsForSettings(
    const GlassEffectSettings& raw_settings,
    float time_seconds
) {
    const GlassEffectSettings settings = NormalizeGlassEffectSettings(raw_settings);
    WidgetGlassEffectAnimationSettings result{};
    result.time_seconds = time_seconds;
    result.calm_water.enabled = settings.calm_water.enabled;
    result.calm_water.amplitude = 1.8F * settings.calm_water.intensity_percent / 100.0F;
    result.calm_water.speed = 0.72F * settings.calm_water.speed_percent / 100.0F;
    result.calm_water.wavelength = 118.0F * settings.calm_water.wavelength_percent / 100.0F;
    result.calm_water.noise = 0.18F * settings.calm_water.noise_percent / 100.0F;
    result.liquid.enabled = settings.liquid.enabled;
    result.liquid.amplitude = 3.4F * settings.liquid.intensity_percent / 100.0F;
    result.liquid.speed = 0.46F * settings.liquid.speed_percent / 100.0F;
    result.liquid.wavelength = 86.0F * settings.liquid.wavelength_percent / 100.0F;
    result.liquid.fluidity = settings.liquid.fluidity_percent / 100.0F;
    result.liquid.noise = 0.36F * settings.liquid.noise_percent / 100.0F;
    result.rain.enabled = settings.rain.enabled;
    result.rain.amplitude = 2.6F * settings.rain.intensity_percent / 100.0F;
    result.rain.speed = settings.rain.speed_percent / 100.0F;
    result.rain.density = settings.rain.density_percent / 100.0F;
    result.rain.ring_radius = 58.0F * settings.rain.ring_size_percent / 100.0F;
    result.rain.fade = settings.rain.fade_percent / 100.0F;
    result.rain.noise = 0.12F * settings.rain.intensity_percent / 100.0F;
    return result;
}

// ----------------------------------------------------------------------------
// Applique une deformation de type verre glass_effecte a une frame capturee.
// ----------------------------------------------------------------------------
WidgetGlassEffectFrame DistortGlassEffectFrame(
    const WidgetGlassEffectFrame& source,
    const std::vector<WidgetGlassEffectLens>& lenses,
    const WidgetGlassEffectDistortionSettings& settings,
    const WidgetGlassEffectAnimationSettings& animation
) {
    if (source.width < 4 || source.height < 4 || source.stride < source.width * 4U || source.pixels.empty()) {
        return source;
    }

    WidgetGlassEffectFrame target = source;
    const bool animation_active = HasActiveAnimation(animation);
    const Vec2 center{
        (static_cast<float>(source.width) - 1.0F) * 0.5F,
        (static_cast<float>(source.height) - 1.0F) * 0.5F,
    };
    const Vec2 half_size{
        std::max(1.0F, center.x),
        std::max(1.0F, center.y),
    };
    const float radius = std::min(settings.glass_corner_radius, std::min(half_size.x, half_size.y) * 0.42F);

    std::vector<RefractionShape> shapes;
    shapes.reserve(lenses.size() + 1U);
    shapes.push_back(RefractionShape{
        center,
        half_size,
        radius,
        settings.edge_band,
        settings.edge_strength,
        settings.chromatic_shift,
        true,
        -1.0F,
        true,
        1.0F,
    });

    for (const WidgetGlassEffectLens& lens : lenses) {
        const float width = lens.right - lens.left;
        const float height = lens.bottom - lens.top;
        if (width <= 2.0F || height <= 2.0F) {
            continue;
        }

        const Vec2 lens_half_size{
            width * 0.5F,
            height * 0.5F,
        };
        const float signed_zoom = lens.strength;
        const float zoom_strength = std::fabs(signed_zoom);
        if (zoom_strength <= 0.0001F) {
            continue;
        }

        shapes.push_back(RefractionShape{
            Vec2{lens.left + lens_half_size.x, lens.top + lens_half_size.y},
            lens_half_size,
            std::min(lens.radius, std::min(lens_half_size.x, lens_half_size.y)),
            lens.band,
            zoom_strength,
            settings.lens_chromatic_shift,
            false,
            signed_zoom > 0.0F ? -1.0F : 1.0F,
            false,
            lens.softness,
        });
    }

    for (uint32_t y = 0; y < source.height; ++y) {
        for (uint32_t x = 0; x < source.width; ++x) {
            const Vec2 position{
                static_cast<float>(x),
                static_cast<float>(y),
            };
            const RefractionSample refraction_sample = EvaluateDominantRefraction(shapes, position, x, y);
            const Vec2 animated_offset = AnimatedWaterOffset(animation, position, source);
            if (refraction_sample.amount <= 0.0F && !animation_active) {
                continue;
            }

            const Vec2 normal = refraction_sample.normal;
            const float sample_direction = refraction_sample.sample_direction;
            const float sample_x = position.x + animated_offset.x + (normal.x * refraction_sample.amount * sample_direction);
            const float sample_y = position.y + animated_offset.y + (normal.y * refraction_sample.amount * sample_direction);
            const float chroma = refraction_sample.chromatic_shift;

            float blue = SampleChannel(source, sample_x + (normal.x * chroma), sample_y + (normal.y * chroma), 0);
            float green = SampleChannel(source, sample_x, sample_y, 1);
            float red = SampleChannel(source, sample_x - (normal.x * chroma), sample_y - (normal.y * chroma), 2);
            const float alpha = SampleChannel(source, sample_x, sample_y, 3);

            WritePixel(target, x, y, blue, green, red, alpha);
        }
    }

    return target;
}

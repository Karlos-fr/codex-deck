// ============================================================================
// Codex Glass - Composition Direct2D de Wave
// ----------------------------------------------------------------------------
// Ce module deplace des bandes du bitmap final sur la cible Direct2D. Direct2D
// conserve l'execution sur le GPU lorsque le render target est materiel.
// ============================================================================

#include "WidgetRenderMotionWave.h"

#include <algorithm>
#include <cmath>

namespace {

// Largeur d'une bande verticale ou horizontale recomposee en DIPs.
constexpr float kWaveStripSize = 2.0F;

// Taille des tuiles utilisees lorsque le front Wave est ondule.
constexpr float kWaveUndulatedTileSize = 6.0F;

// Approximation locale de deux fois pi pour l'oscillation de la surface.
constexpr float kTwoPi = 6.2831853F;

// Amplitude maximale du relief du front relativement a la longueur d'onde.
constexpr float kMaximumFrontUndulationRatio = 0.32F;

// Periode transversale du relief relativement a la longueur d'onde.
constexpr float kFrontUndulationPeriodRatio = 1.65F;

// Nombre de lobes utilises pour onduler un front radial.
constexpr float kRadialFrontLobeCount = 6.0F;

// ----------------------------------------------------------------------------
// Calcule le decalage local d'un front selon sa coordonnee transversale.
//
// Parametres :
// - transverse : position le long du front en DIPs.
// - wavelength : longueur d'onde en DIPs.
// - strength : ondulation demandee entre zero et un.
//
// Retour :
// - avance ou retard local du front en DIPs.
// ----------------------------------------------------------------------------
float WaveFrontUndulationOffset(float transverse, float wavelength, float strength) {
    const float period = std::max(1.0F, wavelength * kFrontUndulationPeriodRatio);
    return std::sin(transverse * kTwoPi / period)
        * wavelength
        * kMaximumFrontUndulationRatio
        * std::clamp(strength, 0.0F, 1.0F);
}

// ----------------------------------------------------------------------------
// Calcule le decalage local d'un front radial selon son angle.
//
// Parametres :
// - angle : angle du point autour du centre en radians.
// - wavelength : longueur d'onde en DIPs.
// - strength : ondulation demandee entre zero et un.
//
// Retour :
// - avance ou retard local du rayon du front en DIPs.
// ----------------------------------------------------------------------------
float RadialWaveFrontUndulationOffset(float angle, float wavelength, float strength) {
    return std::sin(angle * kRadialFrontLobeCount)
        * wavelength
        * kMaximumFrontUndulationRatio
        * std::clamp(strength, 0.0F, 1.0F);
}

// ----------------------------------------------------------------------------
// Calcule le decalage signe autour du front de vague.
//
// Parametres :
// - coordinate : position de la bande sur l'axe de propagation.
// - extent : longueur totale de cet axe.
// - progress : progression du front entre zero et un.
// - wavelength : longueur d'onde en DIPs.
// - wave_count : nombre de cretes du train.
// - damping : amortissement entre zero et un.
// - amplitude : deplacement maximal en DIPs.
// - reverse : true lorsque le front part de la fin de l'axe.
// - front_offset : avance ou retard local du front en DIPs.
//
// Retour :
// - decalage de sampling signe en DIPs.
// ----------------------------------------------------------------------------
float WaveStripOffset(
    float coordinate,
    float extent,
    float progress,
    float wavelength,
    int wave_count,
    float damping,
    float amplitude,
    bool reverse,
    float front_offset
) {
    const float train_length = wavelength * static_cast<float>(std::clamp(wave_count, 1, 6));
    const float travel = extent + train_length;
    const float front = reverse
        ? extent - (progress * travel)
        : progress * travel;
    const float local_front = front + front_offset;
    const float behind_front = reverse ? coordinate - local_front : local_front - coordinate;
    if (behind_front < 0.0F || behind_front > train_length) {
        return 0.0F;
    }

    const float normalized_tail = behind_front / std::max(1.0F, train_length);
    const float attack = std::clamp(behind_front / std::max(1.0F, wavelength * 0.20F), 0.0F, 1.0F);
    const float tail = std::clamp((1.0F - normalized_tail) * 2.2F, 0.0F, 1.0F);
    const float attenuation = std::pow(
        std::max(0.0F, 1.0F - normalized_tail),
        0.25F + (1.75F * std::clamp(damping, 0.0F, 1.0F))
    );
    const float phase = behind_front * kTwoPi / std::max(8.0F, wavelength);
    return std::sin(phase) * attack * tail * attenuation * amplitude;
}

} // namespace

// ----------------------------------------------------------------------------
// Calcule le deplacement horizontal d'une bande pour l'etat Wave courant.
// ----------------------------------------------------------------------------
float EvaluateWidgetMotionWaveStripOffset(
    float coordinate,
    float extent,
    const WidgetVibrationGlassEffectState& wave
) {
    const float progress = std::clamp(wave.wave_position * std::max(0.25F, wave.wave_speed), 0.0F, 1.0F);
    const float wavelength = std::clamp(72.0F * wave.wave_wavelength / 100.0F, 18.0F, 216.0F);
    const float amplitude = std::clamp(wave.wave_refraction * 1.35F, 2.0F, 24.0F);
    return WaveStripOffset(
        coordinate,
        extent,
        progress,
        wavelength,
        wave.wave_count,
        wave.wave_damping,
        amplitude,
        wave.wave_direction == WidgetMotionWaveDirection::Left,
        0.0F
    );
}

// ----------------------------------------------------------------------------
// Recompose un bitmap complet avec la deformation Wave courante.
// ----------------------------------------------------------------------------
void DrawWidgetMotionWaveComposition(
    ID2D1RenderTarget* target,
    ID2D1Bitmap* source,
    D2D1_SIZE_F size,
    const WidgetVibrationGlassEffectState& wave
) {
    if (target == nullptr || source == nullptr || wave.wave_refraction <= 0.0F) {
        return;
    }

    target->DrawBitmap(source, D2D1::RectF(0.0F, 0.0F, size.width, size.height));
    if (wave.wave_front_undulation > 0.0F) {
        const float progress = std::clamp(
            wave.wave_position * std::max(0.25F, wave.wave_speed),
            0.0F,
            1.0F
        );
        const float wavelength = std::clamp(
            72.0F * wave.wave_wavelength / 100.0F,
            18.0F,
            216.0F
        );
        const float amplitude = std::clamp(wave.wave_refraction * 1.35F, 2.0F, 24.0F);
        const bool radial = wave.wave_direction == WidgetMotionWaveDirection::Radial;
        const bool vertical = wave.wave_direction == WidgetMotionWaveDirection::Up
            || wave.wave_direction == WidgetMotionWaveDirection::Down;
        const bool reverse = wave.wave_direction == WidgetMotionWaveDirection::Left
            || wave.wave_direction == WidgetMotionWaveDirection::Up;
        const float center_x = size.width * 0.5F;
        const float center_y = size.height * 0.5F;
        const float radial_extent = std::sqrt(
            (center_x * center_x) + (center_y * center_y)
        );
        for (float y = 0.0F; y < size.height; y += kWaveUndulatedTileSize) {
            const float tile_bottom = std::min(size.height, y + kWaveUndulatedTileSize + 1.0F);
            for (float x = 0.0F; x < size.width; x += kWaveUndulatedTileSize) {
                const float tile_center_x = x + (kWaveUndulatedTileSize * 0.5F);
                const float tile_center_y = y + (kWaveUndulatedTileSize * 0.5F);
                float coordinate = vertical ? tile_center_y : tile_center_x;
                float extent = vertical ? size.height : size.width;
                float normal_x = vertical ? 0.0F : 1.0F;
                float normal_y = vertical ? 1.0F : 0.0F;
                float front_offset = WaveFrontUndulationOffset(
                    vertical ? tile_center_x : tile_center_y,
                    wavelength,
                    wave.wave_front_undulation
                );
                if (radial) {
                    const float delta_x = tile_center_x - center_x;
                    const float delta_y = tile_center_y - center_y;
                    coordinate = std::sqrt((delta_x * delta_x) + (delta_y * delta_y));
                    extent = radial_extent;
                    if (coordinate > 0.001F) {
                        normal_x = delta_x / coordinate;
                        normal_y = delta_y / coordinate;
                    }
                    front_offset = RadialWaveFrontUndulationOffset(
                        std::atan2(delta_y, delta_x),
                        wavelength,
                        wave.wave_front_undulation
                    );
                }
                const float offset = WaveStripOffset(
                    coordinate,
                    extent,
                    progress,
                    wavelength,
                    wave.wave_count,
                    wave.wave_damping,
                    amplitude,
                    radial ? false : reverse,
                    front_offset
                );
                if (std::fabs(offset) < 0.01F) {
                    continue;
                }
                const float tile_right = std::min(size.width, x + kWaveUndulatedTileSize + 1.0F);
                const D2D1_RECT_F source_rect = D2D1::RectF(x, y, tile_right, tile_bottom);
                const D2D1_RECT_F destination_rect = D2D1::RectF(
                    x + (normal_x * offset),
                    y + (normal_y * offset),
                    tile_right + (normal_x * offset),
                    tile_bottom + (normal_y * offset)
                );
                target->DrawBitmap(
                    source,
                    destination_rect,
                    1.0F,
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                    source_rect
                );
            }
        }
        return;
    }

    const bool horizontal = wave.wave_direction == WidgetMotionWaveDirection::Left
        || wave.wave_direction == WidgetMotionWaveDirection::Right;
    if (!horizontal) {
        return;
    }

    for (float x = 0.0F; x < size.width; x += kWaveStripSize) {
        const float strip_right = std::min(size.width, x + kWaveStripSize + 1.0F);
        const float offset = EvaluateWidgetMotionWaveStripOffset(
            x + (kWaveStripSize * 0.5F),
            size.width,
            wave
        );
        const D2D1_RECT_F source_rect = D2D1::RectF(x, 0.0F, strip_right, size.height);
        const D2D1_RECT_F destination_rect = D2D1::RectF(
            x + offset,
            0.0F,
            strip_right + offset,
            size.height
        );
        target->DrawBitmap(
            source,
            destination_rect,
            1.0F,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            source_rect
        );
    }
}

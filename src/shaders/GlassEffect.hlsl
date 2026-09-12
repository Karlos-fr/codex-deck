// ============================================================================
// Codex Deck - Reference du shader GlassEffect
// ----------------------------------------------------------------------------
// Reference lisible du rendu GPU GlassEffect conservee pour les prochains
// modules de rendu. Ce fichier n'est pas encore compile par la fondation.
// ============================================================================

Texture2D source_texture : register(t0);
SamplerState linear_sampler : register(s0);

cbuffer GlassEffectConstants : register(b0)
{
    float2 target_size;
    float corner_radius;
    float refraction_band;
    float refraction_strength;
    float chromatic_shift;
    float animation_time;
    float animation_padding;
    float4 calm_params;
    float4 liquid_params;
    float4 rain_params;
    float4 rain_extra;
    float4 animation_flags;
}

// ----------------------------------------------------------------------------
// Calcule la distance signee d'un rectangle arrondi.
//
// Parametres :
// - point : position locale a evaluer.
// - half_size : demi-taille du rectangle.
// - radius : rayon des coins arrondis.
//
// Retour :
// - distance signee a la forme.
// ----------------------------------------------------------------------------
float rounded_rect_sdf(float2 point, float2 half_size, float radius)
{
    float2 q = abs(point) - half_size + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// ----------------------------------------------------------------------------
// Calcule la normale du rectangle arrondi depuis son SDF.
//
// Parametres :
// - point : position locale a evaluer.
// - half_size : demi-taille du rectangle.
// - radius : rayon des coins arrondis.
//
// Retour :
// - normale 2D normalisee.
// ----------------------------------------------------------------------------
float2 sdf_normal(float2 point, float2 half_size, float radius)
{
    const float epsilon = 1.0;
    float dx = rounded_rect_sdf(point + float2(epsilon, 0.0), half_size, radius)
        - rounded_rect_sdf(point - float2(epsilon, 0.0), half_size, radius);
    float dy = rounded_rect_sdf(point + float2(0.0, epsilon), half_size, radius)
        - rounded_rect_sdf(point - float2(0.0, epsilon), half_size, radius);
    return normalize(float2(dx, dy));
}

// ----------------------------------------------------------------------------
// Calcule une attenuation douce pour preserver les bords du widget.
//
// Parametres :
// - pixel : position du pixel dans la texture cible.
//
// Retour :
// - coefficient entre 0 et 1.
// ----------------------------------------------------------------------------
float edge_fade(float2 pixel)
{
    const float fade_width = 18.0;
    float2 distance_to_edge = min(pixel, target_size - 1.0 - pixel);
    return smoothstep(0.0, fade_width, min(distance_to_edge.x, distance_to_edge.y));
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
float hash01(float seed)
{
    float value = sin(seed * 12.9898) * 43758.5453;
    return frac(value);
}

// ----------------------------------------------------------------------------
// Calcule le decalage radial produit par une goutte de pluie.
//
// Parametres :
// - pixel : position du pixel dans la texture cible.
// - center : centre de l'impact de goutte.
// - age : progression de l'impact entre 0 et 1.
// - strength : amplitude de deformation.
//
// Retour :
// - decalage de sampling en pixels.
// ----------------------------------------------------------------------------
float2 rain_drop_ripple_offset(float2 pixel, float2 center, float age, float strength, float ring_radius)
{
    float2 delta = pixel - center;
    float distance = length(delta);
    if (distance <= 0.001) {
        return float2(0.0, 0.0);
    }

    float radius = 10.0 + age * ring_radius;
    float band = 5.5 + age * max(3.0, ring_radius * 0.086);
    float ring_distance = abs(distance - radius);
    if (ring_distance > band) {
        return float2(0.0, 0.0);
    }

    float ring = 1.0 - smoothstep(0.0, band, ring_distance);
    float decay = (1.0 - age) * (1.0 - age);
    float ripple_wave = sin((distance - radius) * 0.62);
    float amount = ring * decay * ripple_wave * strength;
    return normalize(delta) * amount;
}

// ----------------------------------------------------------------------------
// Calcule le decalage anime de gouttes vues du dessus avec anneaux d'impact.
//
// Parametres :
// - pixel : position du pixel dans la texture cible.
//
// Retour :
// - decalage de sampling en pixels.
// ----------------------------------------------------------------------------
float2 rain_offset(float2 pixel)
{
    int rain_drop_count = clamp((int)round(11.0 * rain_params.z), 3, 28);
    float time = animation_time * rain_params.y;
    float2 offset = float2(0.0, 0.0);

    for (int index = 0; index < rain_drop_count; ++index) {
        float seed = float(index + 1);
        float cycle_offset = hash01(seed * 2.31);
        float cycle_speed = 0.28 + hash01(seed * 3.19) * 0.18;
        float cycle = time * cycle_speed + cycle_offset;
        float cycle_index = floor(cycle);
        float normalized_age = cycle - cycle_index;
        float center_x = (0.08 + hash01(seed * 5.47 + cycle_index * 11.17) * 0.84) * (target_size.x - 1.0);
        float center_y = (0.10 + hash01(seed * 8.91 + cycle_index * 13.31) * 0.80) * (target_size.y - 1.0);
        float2 drop_center = float2(center_x, center_y);
        offset += rain_drop_ripple_offset(pixel, drop_center, normalized_age, rain_params.x * 2.1, rain_params.w);

        float drop_distance = length(pixel - drop_center);
        float drop_highlight = 1.0 - smoothstep(0.0, 5.5, drop_distance);
        float impact_flash = 1.0 - smoothstep(0.0, 0.16 * rain_extra.x, normalized_age);
        offset.y += drop_highlight * rain_params.x * 1.25 * impact_flash;
    }

    return offset;
}

// ----------------------------------------------------------------------------
// Calcule le decalage anime equivalent a la reference CPU.
//
// Parametres :
// - pixel : position du pixel dans la texture cible.
//
// Retour :
// - decalage de sampling en pixels.
// ----------------------------------------------------------------------------
float2 wave_offset(float2 pixel, float4 parameters, float liquid)
{
    float time = animation_time * parameters.y;
    float wave_scale = 6.2831853 / max(1.0, parameters.z);
    float primary = sin((pixel.x * wave_scale) + (pixel.y * wave_scale * 0.35) + time);
    float secondary = sin((pixel.y * wave_scale * lerp(1.2, 0.83, liquid)) - (time * 0.72));
    float diagonal = sin(((pixel.x + pixel.y) * wave_scale * 0.62) + (time * lerp(0.48, 0.31, liquid)));
    float boost = lerp(1.0, 1.35, liquid);
    float2 offset = float2(
        (primary * 0.72 + diagonal * 0.28) * parameters.x * boost,
        (secondary * 0.62 - diagonal * 0.24) * parameters.x * boost
    );
    float shimmer = sin((pixel.x * 0.113) + (pixel.y * 0.071) + (time * 1.37));
    offset.x += shimmer * parameters.w;
    offset.y -= shimmer * parameters.w * 0.55;
    return offset;
}

// ----------------------------------------------------------------------------
// Combine les offsets d'eau actives pour le pixel courant.
//
// Parametres :
// - pixel : position du pixel dans la texture cible.
//
// Retour :
// - decalage de sampling en pixels apres attenuation des bords.
// ----------------------------------------------------------------------------
float2 water_offset(float2 pixel)
{
    float2 offset = float2(0.0, 0.0);
    if (animation_flags.x > 0.5) offset += wave_offset(pixel, calm_params, 0.0);
    if (animation_flags.y > 0.5) offset += wave_offset(pixel, liquid_params, 1.0);
    if (animation_flags.z > 0.5) {
        offset += rain_offset(pixel);
        float shimmer = sin((pixel.x * 0.097) + (pixel.y * 0.083) + animation_time * rain_params.y);
        offset += float2(shimmer, -shimmer * 0.45) * rain_extra.y;
    }
    float offset_length = length(offset);
    if (offset_length > 18.0) offset *= 18.0 / offset_length;
    return offset * edge_fade(pixel);
}

// ----------------------------------------------------------------------------
// Applique la refraction principale du widget.
//
// Parametres :
// - position : position ecran fournie par le pipeline.
// - uv : coordonnees normalisees de sampling.
//
// Retour :
// - couleur finale du pixel.
// ----------------------------------------------------------------------------
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float2 pixel = uv * target_size;
    float2 center = (target_size - 1.0) * 0.5;
    float2 half_size = max(center, 1.0);
    float2 point = pixel - center;
    float sdf = rounded_rect_sdf(point, half_size, corner_radius);
    float inside_distance = max(0.0, -sdf);
    float edge_amount = 1.0 - smoothstep(0.0, refraction_band, inside_distance);
    float2 normal = sdf_normal(point, half_size, corner_radius);
    float wave = sin((pixel.x * 0.035) + (pixel.y * 0.022));
    float refraction = edge_amount * (refraction_strength + (wave * 2.0));
    float2 sample_pixel = center + point + water_offset(pixel) - (normal * refraction);
    float2 sample_uv = sample_pixel / target_size;
    float2 chroma_uv = (normal * chromatic_shift * edge_amount) / target_size;

    float blue = source_texture.Sample(linear_sampler, sample_uv + chroma_uv).b;
    float green = source_texture.Sample(linear_sampler, sample_uv).g;
    float red = source_texture.Sample(linear_sampler, sample_uv - chroma_uv).r;
    float alpha = source_texture.Sample(linear_sampler, sample_uv).a;

    return float4(red, green, blue, alpha);
}

// ============================================================================
// Codex Glass - Implementation du processeur GPU GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier compile et execute un pixel shader D3D11 pour Calm Water, Liquid
// et Rain. La lecture finale reste le pont de compatibilite du renderer D2D1.
// ============================================================================

#include "WidgetGlassEffectGpuProcessor.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace {

// Nombre maximal de composants graphiques transmis au shader en une frame.
constexpr size_t kMaximumGpuLensCount = 64U;

// Code HLSL du triangle plein ecran et de la refraction GlassEffect.
constexpr char kGlassEffectShaderSource[] = R"hlsl(
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
    float4 calm_params;
    float4 liquid_params;
    float4 rain_params;
    float4 rain_extra;
    float4 animation_flags;
    float4 lens_global;
    float4 lens_rects[64];
    float4 lens_params[64];
};

struct VertexOutput
{
    float4 position : SV_POSITION;
};

VertexOutput vertex_main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    float2 position = float2(
        vertex_id == 2 ? 3.0 : -1.0,
        vertex_id == 1 ? 3.0 : -1.0
    );
    output.position = float4(position, 0.0, 1.0);
    return output;
}

float rounded_rect_sdf(float2 local_position, float2 half_size, float radius)
{
    float2 q = abs(local_position) - half_size + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float2 sdf_normal(float2 local_position, float2 half_size, float radius)
{
    const float epsilon = 1.0;
    float dx = rounded_rect_sdf(local_position + float2(epsilon, 0.0), half_size, radius)
        - rounded_rect_sdf(local_position - float2(epsilon, 0.0), half_size, radius);
    float dy = rounded_rect_sdf(local_position + float2(0.0, epsilon), half_size, radius)
        - rounded_rect_sdf(local_position - float2(0.0, epsilon), half_size, radius);
    float2 gradient = float2(dx, dy);
    float gradient_length = length(gradient);
    return gradient_length > 0.0001 ? gradient / gradient_length : float2(0.0, 0.0);
}

float edge_fade(float2 pixel)
{
    const float fade_width = 18.0;
    float2 distance_to_edge = min(pixel, target_size - 1.0 - pixel);
    return smoothstep(0.0, fade_width, min(distance_to_edge.x, distance_to_edge.y));
}

float hash01(float seed)
{
    return frac(sin(seed * 12.9898) * 43758.5453);
}

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
    return delta * (amount / distance);
}

float2 rain_offset(float2 pixel)
{
    int rain_drop_count = clamp((int)round(11.0 * rain_params.z), 3, 28);
    float time = animation_time * rain_params.y;
    float2 offset = float2(0.0, 0.0);

    [loop]
    for (int index = 0; index < rain_drop_count; ++index) {
        float seed = (float)(index + 1);
        float cycle_offset = hash01(seed * 2.31);
        float cycle_speed = 0.28 + hash01(seed * 3.19) * 0.18;
        float cycle = time * cycle_speed + cycle_offset;
        float cycle_index = floor(cycle);
        float normalized_age = cycle - cycle_index;
        float center_x = (0.08 + hash01(seed * 5.47 + cycle_index * 11.17) * 0.84)
            * (target_size.x - 1.0);
        float center_y = (0.10 + hash01(seed * 8.91 + cycle_index * 13.31) * 0.80)
            * (target_size.y - 1.0);
        float2 drop_center = float2(center_x, center_y);
        offset += rain_drop_ripple_offset(
            pixel,
            drop_center,
            normalized_age,
            rain_params.x * 2.1,
            rain_params.w
        );

        float drop_distance = length(pixel - drop_center);
        float drop_highlight = 1.0 - smoothstep(0.0, 5.5, drop_distance);
        float impact_flash = 1.0 - smoothstep(0.0, 0.16 * rain_extra.x, normalized_age);
        offset.y += drop_highlight * rain_params.x * 1.25 * impact_flash;
    }
    return offset;
}

float2 wave_offset(float2 pixel, float4 parameters, float liquid)
{
    float time = animation_time * parameters.y;
    float wave_scale = 6.2831853 / max(1.0, parameters.z);
    float2 offset = float2(0.0, 0.0);
    float primary = sin((pixel.x * wave_scale) + (pixel.y * wave_scale * 0.35) + time);
    float secondary = sin((pixel.y * wave_scale * lerp(1.2, 0.83, liquid)) - (time * 0.72));
    float diagonal = sin(((pixel.x + pixel.y) * wave_scale * 0.62) + (time * lerp(0.48, 0.31, liquid)));
    float boost = lerp(1.0, 1.35, liquid);
    offset.x = ((primary * 0.72) + (diagonal * 0.28)) * parameters.x * boost;
    offset.y = ((secondary * 0.62) - (diagonal * 0.24)) * parameters.x * boost;
    float shimmer = sin((pixel.x * 0.113) + (pixel.y * 0.071) + (time * 1.37));
    offset.x += shimmer * parameters.w;
    offset.y -= shimmer * parameters.w * 0.55;
    return offset;
}

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

float4 pixel_main(VertexOutput input) : SV_TARGET
{
    float2 pixel = input.position.xy;
    float2 center = (target_size - 1.0) * 0.5;
    float2 half_size = max(center, 1.0);
    float2 local_position = pixel - center;
    float sdf = rounded_rect_sdf(local_position, half_size, corner_radius);
    float inside_distance = max(0.0, -sdf);
    float edge_amount = 1.0 - smoothstep(0.0, refraction_band, inside_distance);
    float2 normal = sdf_normal(local_position, half_size, corner_radius);
    float wave = sin((pixel.x * 0.035) + (pixel.y * 0.022));
    float refraction = edge_amount * (refraction_strength + (wave * 2.0));
    float chroma = chromatic_shift * edge_amount;
    float sample_direction = -1.0;

    int lens_count = clamp((int)round(lens_global.y), 0, 64);
    [loop]
    for (int lens_index = 0; lens_index < lens_count; ++lens_index) {
        float4 lens_rect = lens_rects[lens_index];
        float4 lens_param = lens_params[lens_index];
        float2 lens_center = (lens_rect.xy + lens_rect.zw) * 0.5;
        float2 lens_half_size = max((lens_rect.zw - lens_rect.xy) * 0.5, 1.0);
        float2 lens_local = pixel - lens_center;
        float lens_sdf = rounded_rect_sdf(
            lens_local,
            lens_half_size,
            min(lens_param.x, min(lens_half_size.x, lens_half_size.y))
        );
        float lens_distance = abs(lens_sdf);
        float lens_strength = abs(lens_param.z);
        float lens_edge_amount = 1.0 - smoothstep(0.0, lens_param.y, lens_distance);
        lens_edge_amount = pow(saturate(lens_edge_amount), 1.0 / max(0.25, lens_param.w));
        float lens_amount = lens_edge_amount * lens_strength;
        if (lens_amount > refraction) {
            normal = sdf_normal(lens_local, lens_half_size, lens_param.x);
            refraction = lens_amount;
            chroma = lens_global.x * lens_edge_amount;
            sample_direction = lens_param.z > 0.0 ? -1.0 : 1.0;
        }
    }

    float2 sample_pixel = pixel + water_offset(pixel) + (normal * refraction * sample_direction);
    float2 sample_uv = sample_pixel / target_size;
    float2 chroma_uv = (normal * chroma) / target_size;

    float blue = source_texture.Sample(linear_sampler, sample_uv + chroma_uv).b;
    float green = source_texture.Sample(linear_sampler, sample_uv).g;
    float red = source_texture.Sample(linear_sampler, sample_uv - chroma_uv).r;
    float alpha = source_texture.Sample(linear_sampler, sample_uv).a;
    return float4(red, green, blue, alpha);
}
)hlsl";

// Constantes alignees transmises au pixel shader pour une frame.
struct alignas(16) GlassEffectShaderConstants {
    float target_size[2]{};
    float corner_radius = 0.0F;
    float refraction_band = 0.0F;
    float refraction_strength = 0.0F;
    float chromatic_shift = 0.0F;
    float animation_time = 0.0F;
    float animation_padding = 0.0F;
    float calm_params[4]{};
    float liquid_params[4]{};
    float rain_params[4]{};
    float rain_extra[4]{};
    float animation_flags[4]{};
    float lens_global[4]{};
    float lens_rects[kMaximumGpuLensCount][4]{};
    float lens_params[kMaximumGpuLensCount][4]{};
};

static_assert(sizeof(GlassEffectShaderConstants) % 16U == 0U);

// ----------------------------------------------------------------------------
// Compile une entree du shader HLSL embarque.
//
// Parametres :
// - entry_point : fonction HLSL a compiler.
// - profile : profil shader D3D11 demande.
// - bytecode : blob recevant le bytecode compile.
//
// Retour :
// - true si la compilation a reussi.
// ----------------------------------------------------------------------------
bool CompileShader(const char* entry_point, const char* profile, ID3DBlob** bytecode) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        kGlassEffectShaderSource,
        sizeof(kGlassEffectShaderSource) - 1U,
        "WidgetGlassEffectGpuProcessor",
        nullptr,
        nullptr,
        entry_point,
        profile,
        flags,
        0,
        bytecode,
        errors.GetAddressOf()
    );
    if (FAILED(result) && errors != nullptr) {
        const auto* message = static_cast<const char*>(errors->GetBufferPointer());
        OutputDebugStringA(message);
#if defined(_DEBUG)
        std::fwrite(message, 1, errors->GetBufferSize(), stderr);
#endif
    }
    return SUCCEEDED(result);
}

}  // namespace

// Implementation privee des ressources D3D11 du traitement GPU.
struct WidgetGlassEffectGpuProcessor::Impl {
    // Device D3D11 utilise pour toutes les ressources possedees.
    ComPtr<ID3D11Device> device;

    // Contexte immediat emprunte a la capture pendant Process.
    ComPtr<ID3D11DeviceContext> context;

    // Vertex shader generant un triangle plein ecran sans vertex buffer.
    ComPtr<ID3D11VertexShader> vertex_shader;

    // Pixel shader appliquant la refraction et les animations d'eau.
    ComPtr<ID3D11PixelShader> pixel_shader;

    // Sampler lineaire avec clamp aux bords de la capture.
    ComPtr<ID3D11SamplerState> sampler;

    // Buffer constant mis a jour pour chaque frame animee.
    ComPtr<ID3D11Buffer> constants;

    // Texture GPU recevant le resultat du pixel shader.
    ComPtr<ID3D11Texture2D> output_texture;

    // Vue render target de la texture de sortie.
    ComPtr<ID3D11RenderTargetView> output_view;

    // Texture staging utilisee uniquement par le pont Direct2D existant.
    ComPtr<ID3D11Texture2D> staging_texture;

    // Largeur courante des textures reutilisables.
    uint32_t width = 0;

    // Hauteur courante des textures reutilisables.
    uint32_t height = 0;
};

// ----------------------------------------------------------------------------
// Cree un processeur sans allouer de ressource D3D11.
// ----------------------------------------------------------------------------
WidgetGlassEffectGpuProcessor::WidgetGlassEffectGpuProcessor() = default;

// ----------------------------------------------------------------------------
// Libere les ressources D3D11 possedees par le processeur.
// ----------------------------------------------------------------------------
WidgetGlassEffectGpuProcessor::~WidgetGlassEffectGpuProcessor() = default;

// ----------------------------------------------------------------------------
// Libere les ressources et oublie le device D3D11 courant.
// ----------------------------------------------------------------------------
void WidgetGlassEffectGpuProcessor::Discard() {
    impl_.reset();
}

// ----------------------------------------------------------------------------
// Initialise les shaders et les ressources independantes de la taille.
//
// Parametres :
// - impl : implementation a initialiser.
// - device : device D3D11 de la capture.
// - context : contexte immediat associe.
//
// Retour :
// - true si toutes les ressources permanentes sont disponibles.
// ----------------------------------------------------------------------------
static bool EnsurePipeline(
    WidgetGlassEffectGpuProcessor::Impl& impl,
    ID3D11Device* device,
    ID3D11DeviceContext* context
) {
    if (impl.device.Get() != device) {
        impl = WidgetGlassEffectGpuProcessor::Impl{};
        impl.device = device;
    }
    impl.context = context;
    if (impl.vertex_shader && impl.pixel_shader) {
        return true;
    }

    ComPtr<ID3DBlob> vertex_bytecode;
    ComPtr<ID3DBlob> pixel_bytecode;
    const bool shader_model_five = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0;
    const char* vertex_profile = shader_model_five ? "vs_5_0" : "vs_4_0";
    const char* pixel_profile = shader_model_five ? "ps_5_0" : "ps_4_0";
    if (!CompileShader("vertex_main", vertex_profile, vertex_bytecode.GetAddressOf())
        || !CompileShader("pixel_main", pixel_profile, pixel_bytecode.GetAddressOf())) {
        return false;
    }

    if (FAILED(device->CreateVertexShader(
            vertex_bytecode->GetBufferPointer(),
            vertex_bytecode->GetBufferSize(),
            nullptr,
            impl.vertex_shader.GetAddressOf()
        ))
        || FAILED(device->CreatePixelShader(
            pixel_bytecode->GetBufferPointer(),
            pixel_bytecode->GetBufferSize(),
            nullptr,
            impl.pixel_shader.GetAddressOf()
        ))) {
        return false;
    }

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&sampler_desc, impl.sampler.GetAddressOf()))) {
        return false;
    }

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(GlassEffectShaderConstants);
    buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&buffer_desc, nullptr, impl.constants.GetAddressOf()));
}

// ----------------------------------------------------------------------------
// Cree ou adapte les textures de sortie a la taille de la capture.
//
// Parametres :
// - impl : implementation GPU deja initialisee.
// - source_desc : description de la texture source.
//
// Retour :
// - true si la cible GPU et sa staging sont disponibles.
// ----------------------------------------------------------------------------
static bool EnsureSizedResources(
    WidgetGlassEffectGpuProcessor::Impl& impl,
    const D3D11_TEXTURE2D_DESC& source_desc
) {
    if (impl.output_texture && impl.width == source_desc.Width && impl.height == source_desc.Height) {
        return true;
    }

    impl.output_view.Reset();
    impl.output_texture.Reset();
    impl.staging_texture.Reset();
    impl.width = 0;
    impl.height = 0;

    D3D11_TEXTURE2D_DESC output_desc{};
    output_desc.Width = source_desc.Width;
    output_desc.Height = source_desc.Height;
    output_desc.MipLevels = 1;
    output_desc.ArraySize = 1;
    output_desc.Format = source_desc.Format;
    output_desc.SampleDesc.Count = 1;
    output_desc.Usage = D3D11_USAGE_DEFAULT;
    output_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(impl.device->CreateTexture2D(&output_desc, nullptr, impl.output_texture.GetAddressOf()))
        || FAILED(impl.device->CreateRenderTargetView(
            impl.output_texture.Get(),
            nullptr,
            impl.output_view.GetAddressOf()
        ))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC staging_desc = output_desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(impl.device->CreateTexture2D(&staging_desc, nullptr, impl.staging_texture.GetAddressOf()))) {
        return false;
    }

    impl.width = source_desc.Width;
    impl.height = source_desc.Height;
    return true;
}

// ----------------------------------------------------------------------------
// Relit la texture staging vers une frame BGRA compacte.
//
// Parametres :
// - impl : implementation possedant la staging et son contexte.
// - generation : generation de capture a conserver.
// - output : frame cible remplacee en cas de succes.
//
// Retour :
// - true si toutes les lignes ont ete copiees.
// ----------------------------------------------------------------------------
static bool ReadOutputFrame(
    WidgetGlassEffectGpuProcessor::Impl& impl,
    uint64_t generation,
    WidgetGlassEffectFrame& output
) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(impl.context->Map(impl.staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }

    WidgetGlassEffectFrame next;
    next.width = impl.width;
    next.height = impl.height;
    next.stride = impl.width * 4U;
    next.generation = generation;
    next.pixels.resize(static_cast<size_t>(next.stride) * next.height);
    const auto* source = static_cast<const uint8_t*>(mapped.pData);
    for (uint32_t row = 0; row < next.height; ++row) {
        std::memcpy(
            next.pixels.data() + (static_cast<size_t>(row) * next.stride),
            source + (static_cast<size_t>(row) * mapped.RowPitch),
            next.stride
        );
    }
    impl.context->Unmap(impl.staging_texture.Get(), 0);
    output = std::move(next);
    return true;
}

// ----------------------------------------------------------------------------
// Relit une texture capturee uniquement lorsque le fallback CPU l'exige.
//
// Parametres :
// - source : frame portant la texture D3D11 a relire.
// - output : frame BGRA compacte recevant les pixels.
//
// Retour :
// - true si la texture a ete entierement relue.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectGpuProcessor::Readback(
    const WidgetGlassEffectFrame& source,
    WidgetGlassEffectFrame& output
) {
    if (source.gpu_texture == nullptr || source.gpu_device == nullptr || source.gpu_context == nullptr) {
        return false;
    }
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    if (impl_->device.Get() != source.gpu_device) {
        *impl_ = Impl{};
        impl_->device = source.gpu_device;
    }
    impl_->context = source.gpu_context;

    D3D11_TEXTURE2D_DESC source_desc{};
    source.gpu_texture->GetDesc(&source_desc);
    if (!EnsureSizedResources(*impl_, source_desc)) {
        return false;
    }
    impl_->context->CopyResource(impl_->staging_texture.Get(), source.gpu_texture);
    return ReadOutputFrame(*impl_, source.generation, output);
}

// ----------------------------------------------------------------------------
// Execute la refraction GlassEffect sur la texture GPU de la frame.
//
// Parametres :
// - source : frame capturee et references D3D11 non possedees.
// - lenses : zones de refraction des composants graphiques.
// - settings : profil de refraction principal.
// - animation : animation d'eau a appliquer.
// - output : frame BGRA recevant le resultat.
//
// Retour :
// - true si le shader a produit une frame complete.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectGpuProcessor::Process(
    const WidgetGlassEffectFrame& source,
    const std::vector<WidgetGlassEffectLens>& lenses,
    const WidgetGlassEffectDistortionSettings& settings,
    const WidgetGlassEffectAnimationSettings& animation,
    WidgetGlassEffectFrame& output
) {
    if (source.gpu_texture == nullptr
        || source.gpu_device == nullptr
        || source.gpu_context == nullptr
        || animation.motion_wave_enabled) {
        return false;
    }

    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    if (!EnsurePipeline(*impl_, source.gpu_device, source.gpu_context)) {
        Discard();
        return false;
    }

    D3D11_TEXTURE2D_DESC source_desc{};
    source.gpu_texture->GetDesc(&source_desc);
    if (source_desc.Width == 0
        || source_desc.Height == 0
        || source_desc.SampleDesc.Count != 1
        || !EnsureSizedResources(*impl_, source_desc)) {
        return false;
    }

    ComPtr<ID3D11ShaderResourceView> source_view;
    if (FAILED(impl_->device->CreateShaderResourceView(
            source.gpu_texture,
            nullptr,
            source_view.GetAddressOf()
        ))) {
        return false;
    }

    GlassEffectShaderConstants constants{};
    constants.target_size[0] = static_cast<float>(source_desc.Width);
    constants.target_size[1] = static_cast<float>(source_desc.Height);
    constants.corner_radius = std::min(
        settings.glass_corner_radius,
        static_cast<float>(std::min(source_desc.Width, source_desc.Height)) * 0.21F
    );
    constants.refraction_band = settings.edge_band;
    constants.refraction_strength = settings.edge_strength;
    constants.chromatic_shift = settings.chromatic_shift;
    constants.animation_time = animation.time_seconds;
    constants.calm_params[0] = animation.calm_water.amplitude;
    constants.calm_params[1] = animation.calm_water.speed;
    constants.calm_params[2] = animation.calm_water.wavelength;
    constants.calm_params[3] = animation.calm_water.noise;
    constants.liquid_params[0] = animation.liquid.amplitude;
    constants.liquid_params[1] = animation.liquid.speed * animation.liquid.fluidity;
    constants.liquid_params[2] = animation.liquid.wavelength;
    constants.liquid_params[3] = animation.liquid.noise;
    constants.rain_params[0] = animation.rain.amplitude;
    constants.rain_params[1] = animation.rain.speed;
    constants.rain_params[2] = animation.rain.density;
    constants.rain_params[3] = animation.rain.ring_radius;
    constants.rain_extra[0] = animation.rain.fade;
    constants.rain_extra[1] = animation.rain.noise;
    constants.animation_flags[0] = animation.calm_water.enabled ? 1.0F : 0.0F;
    constants.animation_flags[1] = animation.liquid.enabled ? 1.0F : 0.0F;
    constants.animation_flags[2] = animation.rain.enabled ? 1.0F : 0.0F;
    constants.lens_global[0] = settings.lens_chromatic_shift;
    const size_t lens_count = std::min(lenses.size(), kMaximumGpuLensCount);
    constants.lens_global[1] = static_cast<float>(lens_count);
    for (size_t index = 0; index < lens_count; ++index) {
        const WidgetGlassEffectLens& lens = lenses[index];
        constants.lens_rects[index][0] = lens.left;
        constants.lens_rects[index][1] = lens.top;
        constants.lens_rects[index][2] = lens.right;
        constants.lens_rects[index][3] = lens.bottom;
        constants.lens_params[index][0] = lens.radius;
        constants.lens_params[index][1] = lens.band;
        constants.lens_params[index][2] = lens.strength;
        constants.lens_params[index][3] = lens.softness;
    }

    D3D11_MAPPED_SUBRESOURCE mapped_constants{};
    if (FAILED(impl_->context->Map(
            impl_->constants.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped_constants
        ))) {
        return false;
    }
    std::memcpy(mapped_constants.pData, &constants, sizeof(constants));
    impl_->context->Unmap(impl_->constants.Get(), 0);

    const D3D11_VIEWPORT viewport{
        0.0F,
        0.0F,
        static_cast<float>(source_desc.Width),
        static_cast<float>(source_desc.Height),
        0.0F,
        1.0F,
    };
    ID3D11RenderTargetView* output_view = impl_->output_view.Get();
    ID3D11ShaderResourceView* input_view = source_view.Get();
    ID3D11SamplerState* sampler = impl_->sampler.Get();
    ID3D11Buffer* constant_buffer = impl_->constants.Get();
    impl_->context->OMSetRenderTargets(1, &output_view, nullptr);
    impl_->context->RSSetViewports(1, &viewport);
    impl_->context->IASetInputLayout(nullptr);
    impl_->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    impl_->context->VSSetShader(impl_->vertex_shader.Get(), nullptr, 0);
    impl_->context->PSSetShader(impl_->pixel_shader.Get(), nullptr, 0);
    impl_->context->PSSetShaderResources(0, 1, &input_view);
    impl_->context->PSSetSamplers(0, 1, &sampler);
    impl_->context->PSSetConstantBuffers(0, 1, &constant_buffer);
    impl_->context->Draw(3, 0);

    ID3D11ShaderResourceView* no_input = nullptr;
    ID3D11RenderTargetView* no_output = nullptr;
    impl_->context->PSSetShaderResources(0, 1, &no_input);
    impl_->context->OMSetRenderTargets(1, &no_output, nullptr);
    impl_->context->CopyResource(impl_->staging_texture.Get(), impl_->output_texture.Get());
    return ReadOutputFrame(*impl_, source.generation, output);
}

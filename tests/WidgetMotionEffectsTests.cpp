// ============================================================================
// Codex Glass - Tests des reglages Effets Motion
// ----------------------------------------------------------------------------
// Ce fichier valide les reglages, les timelines et la deformation pixel Wave.
// Il n'ouvre ni fenetre ni menu et reste independant du rendu Direct2D.
// ============================================================================

#include "motion/WidgetMotionEffectsSettings.h"
#include "motion/WidgetMotionCurves.h"
#include "motion/WidgetMotionWaveField.h"
#include "glass/WidgetGlassEffectDistortion.h"
#include "glass/WidgetGlassEffectGpuProcessor.h"
#include "rendering/WidgetRenderMotionWave.h"
#include "rendering/WidgetRenderMotionPulse.h"
#include "vibration/WidgetVibrationAnimation.h"
#include "vibration/WidgetVibrationGlassEffect.h"

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Leve une erreur de test lorsqu'une condition n'est pas satisfaite.
//
// Parametres :
// - condition : resultat a verifier.
// - message : diagnostic associe a l'echec.
// ----------------------------------------------------------------------------
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// ----------------------------------------------------------------------------
// Verifie les bornes, presets et activations cumulables GlassEffect.
// ----------------------------------------------------------------------------
void TestGlassEffectSettings() {
    GlassEffectSettings settings = DefaultGlassEffectSettings();
    settings.appearance.diffusion_percent = 999;
    settings.calm_water.enabled = true;
    settings.liquid.enabled = true;
    settings.rain.enabled = true;
    settings.rain.density_percent = -10;
    settings = NormalizeGlassEffectSettings(settings);
    Require(settings.appearance.diffusion_percent == 200, "borne de diffusion Glass incorrecte");
    Require(settings.rain.density_percent == 25, "borne de densite Rain incorrecte");
    Require(HasActiveGlassEffectAnimation(settings), "animations Glass cumulables perdues");
    Require(settings.calm_water.enabled && settings.liquid.enabled && settings.rain.enabled,
        "activation simultanee Glass impossible");
    Require(settings.preset == GlassEffectPreset::Custom, "preset personnalise non detecte");

    settings.appearance = GlassEffectAppearanceForPreset(GlassEffectPreset::Strong);
    settings = NormalizeGlassEffectSettings(settings);
    Require(settings.preset == GlassEffectPreset::Strong, "retour exact au preset Strong non detecte");
}

// ----------------------------------------------------------------------------
// Cree un chemin INI temporaire propre au processus de test.
//
// Retour :
// - chemin absolu dans le dossier temporaire Windows.
// ----------------------------------------------------------------------------
std::wstring TemporaryIniPath() {
    wchar_t directory[MAX_PATH]{};
    GetTempPathW(static_cast<DWORD>(std::size(directory)), directory);
    return std::wstring(directory)
        + L"codex_glass_motion_effects_"
        + std::to_wstring(GetCurrentProcessId())
        + L".ini";
}

// ----------------------------------------------------------------------------
// Verifie les bornes et la restauration d'un axe Vibration valide.
// ----------------------------------------------------------------------------
void TestNormalization() {
    WidgetMotionEffectsSettings settings{};
    settings.usage_drop_threshold_percent = -10;
    settings.minimum_interval_seconds = 99999;
    settings.vibration.horizontal_enabled = false;
    settings.vibration.vertical_enabled = false;
    settings.vibration.intensity_percent = 999;
    settings.pulse.softness_percent = -1;
    settings.wave.wave_count = 99;
    settings.wave.direction = static_cast<WidgetMotionWaveDirection>(999);

    const WidgetMotionEffectsSettings normalized = NormalizeWidgetMotionEffectsSettings(settings);
    Require(normalized.usage_drop_threshold_percent == 1, "seuil non borne");
    Require(normalized.minimum_interval_seconds == 3600, "cooldown non borne");
    Require(normalized.vibration.horizontal_enabled, "aucun axe restaure");
    Require(normalized.vibration.intensity_percent == 300, "intensite non bornee");
    Require(normalized.pulse.softness_percent == 0, "douceur non bornee");
    Require(normalized.wave.wave_count == 6, "nombre d'ondes non borne");
    Require(normalized.wave.direction == WidgetMotionWaveDirection::Right, "direction invalide");
}

// ----------------------------------------------------------------------------
// Verifie que chaque reset restaure un seul style sans changer son activation.
// ----------------------------------------------------------------------------
void TestIndividualMotionResets() {
    WidgetMotionVibrationSettings vibration{};
    vibration.enabled = false;
    vibration.intensity_percent = 250;
    vibration.vertical_enabled = true;
    vibration.frequency_percent = 175;
    const WidgetMotionVibrationSettings reset_vibration = ResetWidgetMotionVibrationSettings(vibration);
    Require(!reset_vibration.enabled, "activation Vibration modifiee par le reset");
    Require(reset_vibration.intensity_percent == 100, "intensite Vibration non reinitialisee");
    Require(!reset_vibration.vertical_enabled, "axe Vibration non reinitialise");
    Require(reset_vibration.frequency_percent == 100, "frequence Vibration non reinitialisee");

    WidgetMotionPulseSettings pulse{};
    pulse.enabled = true;
    pulse.duration_ms = 1800;
    pulse.repetitions = WidgetMotionPulseRepetitions::Three;
    pulse.extent_percent = 90;
    const WidgetMotionPulseSettings reset_pulse = ResetWidgetMotionPulseSettings(pulse);
    Require(reset_pulse.enabled, "activation Pulse modifiee par le reset");
    Require(reset_pulse.duration_ms == 600, "duree Pulse non reinitialisee");
    Require(reset_pulse.repetitions == WidgetMotionPulseRepetitions::One, "repetitions Pulse non reinitialisees");
    Require(reset_pulse.extent_percent == 35, "etendue Pulse non reinitialisee");

    WidgetMotionWaveSettings wave{};
    wave.enabled = true;
    wave.direction = WidgetMotionWaveDirection::Radial;
    wave.speed_percent = 225;
    wave.wave_count = 6;
    const WidgetMotionWaveSettings reset_wave = ResetWidgetMotionWaveSettings(wave);
    Require(reset_wave.enabled, "activation Wave modifiee par le reset");
    Require(reset_wave.direction == WidgetMotionWaveDirection::Right, "direction Wave non reinitialisee");
    Require(reset_wave.speed_percent == 100, "vitesse Wave non reinitialisee");
    Require(reset_wave.wave_count == 2, "nombre d'ondes Wave non reinitialise");
}

// ----------------------------------------------------------------------------
// Verifie la migration de chaque ancien style exclusif.
// ----------------------------------------------------------------------------
void TestLegacyMigration() {
    WidgetVibrationSettings legacy{};
    legacy.enabled = true;
    legacy.intensity_percent = 175;
    legacy.duration_ms = 900;

    legacy.style = WidgetVibrationStyle::Glass;
    legacy.motion_enabled = true;
    legacy.glass_effect_enabled = false;
    WidgetMotionEffectsSettings migrated = MigrateLegacyWidgetVibrationSettings(legacy);
    Require(migrated.enabled && migrated.vibration.enabled, "migration Glass inactive");
    Require(!migrated.pulse.enabled && !migrated.wave.enabled, "migration Glass non exclusive");
    Require(migrated.vibration.intensity_percent == 175, "intensite Glass perdue");
    Require(migrated.vibration.refraction_percent == 0, "flag refraction Glass ignore");

    legacy.style = WidgetVibrationStyle::Pulse;
    migrated = MigrateLegacyWidgetVibrationSettings(legacy);
    Require(migrated.pulse.enabled && !migrated.vibration.enabled, "migration Pulse incorrecte");
    Require(migrated.pulse.duration_ms == 900, "duree Pulse perdue");

    legacy.style = WidgetVibrationStyle::Wave;
    migrated = MigrateLegacyWidgetVibrationSettings(legacy);
    Require(migrated.wave.enabled && !migrated.pulse.enabled, "migration Wave incorrecte");

    legacy.style = WidgetVibrationStyle::None;
    migrated = MigrateLegacyWidgetVibrationSettings(legacy);
    Require(
        !migrated.vibration.enabled && !migrated.pulse.enabled && !migrated.wave.enabled,
        "migration None incorrecte"
    );
}

// ----------------------------------------------------------------------------
// Verifie la persistance de trois effets actifs et du mode debug.
// ----------------------------------------------------------------------------
void TestIniRoundTrip() {
    const std::wstring path = TemporaryIniPath();
    DeleteFileW(path.c_str());

    WidgetMotionEffectsSettings source{};
    source.enabled = true;
    source.trigger_on_usage_drop = false;
    source.trigger_on_quota_reset = false;
    source.debug_menu_enabled = false;
    source.vibration.enabled = true;
    source.vibration.vertical_enabled = true;
    source.pulse.enabled = true;
    source.pulse.repetitions = WidgetMotionPulseRepetitions::Three;
    source.wave.enabled = true;
    source.wave.direction = WidgetMotionWaveDirection::Radial;
    source.wave.wave_count = 5;
    Require(SaveWidgetMotionEffectsSettings(path, source), "sauvegarde INI impossible");

    const WidgetMotionEffectsSettings loaded = LoadWidgetMotionEffectsSettings(
        path,
        WidgetVibrationSettings{}
    );
    Require(loaded.enabled, "activation globale perdue");
    Require(!loaded.trigger_on_usage_drop, "declencheur de baisse perdu");
    Require(!loaded.trigger_on_quota_reset, "declencheur de reset perdu");
    Require(!loaded.debug_menu_enabled, "mode debug perdu");
    Require(loaded.vibration.enabled && loaded.pulse.enabled && loaded.wave.enabled, "cumul perdu");
    Require(loaded.vibration.vertical_enabled, "axe vertical perdu");
    Require(loaded.pulse.repetitions == WidgetMotionPulseRepetitions::Three, "repetitions perdues");
    Require(loaded.wave.direction == WidgetMotionWaveDirection::Radial, "direction perdue");
    Require(loaded.wave.wave_count == 5, "nombre d'ondes perdu");
    DeleteFileW(path.c_str());
}

// ----------------------------------------------------------------------------
// Verifie que le mode debug est actif pendant une migration sans nouvelle cle.
// ----------------------------------------------------------------------------
void TestDebugDefaultDuringMigration() {
    const std::wstring path = TemporaryIniPath();
    DeleteFileW(path.c_str());
    const WidgetMotionEffectsSettings loaded = LoadWidgetMotionEffectsSettings(
        path,
        WidgetVibrationSettings{}
    );
    Require(loaded.debug_menu_enabled, "mode debug absent par defaut");
}

// ----------------------------------------------------------------------------
// Verifie les enveloppes temporelles de Vibration et Pulse.
// ----------------------------------------------------------------------------
void TestMotionCurves() {
    Require(WidgetMotionVibrationEnvelope(0.0, 100) == 1.0, "depart Vibration incorrect");
    Require(WidgetMotionVibrationEnvelope(1.0, 0) == 0.0, "fin Vibration non nulle");
    Require(
        WidgetMotionVibrationEnvelope(0.5, 100) < WidgetMotionVibrationEnvelope(0.5, 0),
        "amortissement Vibration sans effet"
    );
    Require(
        WidgetMotionPulseEnvelope(0.22, WidgetMotionPulseRepetitions::One, 70) > 0.9,
        "pic Pulse unique absent"
    );
    Require(
        WidgetMotionPulseEnvelope(0.5, WidgetMotionPulseRepetitions::Two, 70) < 0.001,
        "separation des deux pulses absente"
    );
    Require(
        WidgetMotionPulseEnvelope(0.75, WidgetMotionPulseRepetitions::Two, 70)
            < WidgetMotionPulseEnvelope(0.25, WidgetMotionPulseRepetitions::Two, 70),
        "attenuation des repetitions Pulse absente"
    );
    Require(
        std::abs(WidgetMotionPulseCycleProgress(0.625, WidgetMotionPulseRepetitions::Two) - 0.25)
            < 0.001,
        "progression locale Pulse incorrecte"
    );
}

// ----------------------------------------------------------------------------
// Verifie que Pulse transmet son front lumineux au renderer.
// ----------------------------------------------------------------------------
void TestPulseCompositionState() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.vibration.enabled = false;
    settings.pulse.enabled = true;
    settings.pulse.repetitions = WidgetMotionPulseRepetitions::Two;
    settings.pulse.extent_percent = 65;

    const WidgetVibrationGlassEffectState state = WidgetVibrationGlassEffect{}.Evaluate(
        settings,
        GlassEffectMode::Off,
        1.0,
        0.0,
        0.625,
        1.0
    );
    Require(state.pulse_active, "front lumineux Pulse inactif");
    Require(state.border_pulse > 0.0F, "halo Pulse absent");
    Require(std::fabs(state.pulse_phase - 0.25F) < 0.001F, "phase Pulse non transmise");
    Require(std::fabs(state.pulse_extent - 0.65F) < 0.001F, "etendue Pulse non transmise");
}

// ----------------------------------------------------------------------------
// Produit une frame Pulse Direct2D dans un bitmap memoire.
//
// Parametres :
// - phase : progression du front lumineux a rendre.
//
// Retour :
// - pixels BGRA de la frame obtenue.
// ----------------------------------------------------------------------------
std::vector<uint8_t> RenderPulseFrameForTest(float phase) {
    // Largeur du bitmap de validation Pulse.
    constexpr int kPulseTestWidth = 180;

    // Hauteur du bitmap de validation Pulse.
    constexpr int kPulseTestHeight = 120;

    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = kPulseTestWidth;
    bitmap_info.bmiHeader.biHeight = -kPulseTestHeight;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    void* bitmap_pixels = nullptr;
    HDC memory_dc = CreateCompatibleDC(nullptr);
    Require(memory_dc != nullptr, "contexte memoire Pulse indisponible");
    HBITMAP bitmap = CreateDIBSection(
        memory_dc,
        &bitmap_info,
        DIB_RGB_COLORS,
        &bitmap_pixels,
        nullptr,
        0
    );
    Require(bitmap != nullptr && bitmap_pixels != nullptr, "bitmap Pulse indisponible");
    HGDIOBJ previous_bitmap = SelectObject(memory_dc, bitmap);

    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    Require(
        SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())),
        "factory Direct2D Pulse indisponible"
    );
    const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> render_target;
    Require(
        SUCCEEDED(factory->CreateDCRenderTarget(&properties, render_target.GetAddressOf())),
        "cible Direct2D Pulse indisponible"
    );
    const RECT target_rect{0, 0, kPulseTestWidth, kPulseTestHeight};
    Require(SUCCEEDED(render_target->BindDC(memory_dc, &target_rect)), "liaison DC Pulse impossible");

    WidgetVibrationGlassEffectState state{};
    state.border_pulse = 0.55F;
    state.pulse_extent = 0.75F;
    state.pulse_active = true;
    state.pulse_phase = phase;
    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    DrawWidgetMotionPulse(
        render_target.Get(),
        D2D1::RectF(10.0F, 10.0F, 170.0F, 110.0F),
        D2D1::ColorF(0.15F, 0.85F, 0.45F, 1.0F),
        state
    );
    Require(SUCCEEDED(render_target->EndDraw()), "rendu Direct2D Pulse impossible");

    const auto* first_pixel = static_cast<const uint8_t*>(bitmap_pixels);
    const size_t byte_count = static_cast<size_t>(kPulseTestWidth) * kPulseTestHeight * 4U;
    std::vector<uint8_t> pixels(first_pixel, first_pixel + byte_count);
    SelectObject(memory_dc, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
    return pixels;
}

// ----------------------------------------------------------------------------
// Verifie que le halo est visible et que son front se propage entre deux frames.
// ----------------------------------------------------------------------------
void TestPulseRendering() {
    const std::vector<uint8_t> early = RenderPulseFrameForTest(0.20F);
    const std::vector<uint8_t> late = RenderPulseFrameForTest(0.72F);
    const size_t lit_bytes = static_cast<size_t>(std::count_if(
        early.begin(),
        early.end(),
        [](uint8_t value) { return value != 0U; }
    ));
    Require(lit_bytes > 1000U, "halo Pulse trop faible ou absent");
    Require(early != late, "front lumineux Pulse statique");
}

// ----------------------------------------------------------------------------
// Verifie les axes, le dephasage et les timelines de durees differentes.
// ----------------------------------------------------------------------------
void TestOffsetsAndTimelines() {
    WidgetMotionVibrationSettings settings{};
    settings.horizontal_enabled = true;
    settings.vertical_enabled = false;
    WidgetMotionOffset offset = EvaluateWidgetMotionOffset(0.025, 1.0, settings);
    Require(offset.x != 0 && offset.y == 0, "axe horizontal incorrect");

    settings.horizontal_enabled = false;
    settings.vertical_enabled = true;
    offset = EvaluateWidgetMotionOffset(0.0, 1.0, settings);
    Require(offset.x == 0 && offset.y != 0, "axe vertical ou dephasage incorrect");

    using Clock = WidgetVibrationAnimation::Clock;
    const Clock::time_point start{};
    WidgetVibrationAnimation short_timeline{std::chrono::milliseconds{100}};
    WidgetVibrationAnimation long_timeline{std::chrono::milliseconds{300}};
    short_timeline.Start(start);
    long_timeline.Start(start);
    const Clock::time_point middle = start + std::chrono::milliseconds{150};
    Require(!short_timeline.IsActive(middle), "timeline courte encore active");
    Require(long_timeline.IsActive(middle), "derniere timeline arretee trop tot");
    Require(!long_timeline.IsActive(start + std::chrono::milliseconds{300}), "fin de timeline non exacte");
}

// ----------------------------------------------------------------------------
// Verifie les directions et la symetrie du champ de vague pur.
// ----------------------------------------------------------------------------
void TestWaveField() {
    WidgetMotionWaveSettings settings{};
    settings.enabled = true;
    settings.direction = WidgetMotionWaveDirection::Right;
    const WidgetMotionWaveSample right = EvaluateWidgetMotionWaveField(
        50.0F, 50.0F, 100.0F, 100.0F, 0.6F, settings
    );
    Require(std::fabs(right.offset_x) > 0.001F, "vague horizontale absente");
    Require(std::fabs(right.offset_y) < 0.001F, "vague droite devie verticalement");

    settings.direction = WidgetMotionWaveDirection::Radial;
    const WidgetMotionWaveSample radial_right = EvaluateWidgetMotionWaveField(
        75.0F, 50.0F, 100.0F, 100.0F, 0.7F, settings
    );
    const WidgetMotionWaveSample radial_left = EvaluateWidgetMotionWaveField(
        25.0F, 50.0F, 100.0F, 100.0F, 0.7F, settings
    );
    Require(
        std::fabs(radial_right.offset_x + radial_left.offset_x) < 0.001F,
        "symetrie radiale incorrecte"
    );
    Require(std::fabs(radial_right.offset_y) < 0.001F, "rayon horizontal devie verticalement");
}

// ----------------------------------------------------------------------------
// Verifie que Wave deplace effectivement les pixels d'une frame capturee.
// ----------------------------------------------------------------------------
void TestWavePixelDistortion() {
    WidgetGlassEffectFrame source{};
    source.width = 96;
    source.height = 64;
    source.stride = source.width * 4U;
    source.generation = 1;
    source.pixels.resize(static_cast<size_t>(source.stride) * source.height);
    for (uint32_t y = 0; y < source.height; ++y) {
        for (uint32_t x = 0; x < source.width; ++x) {
            const size_t index = static_cast<size_t>(y) * source.stride + (x * 4U);
            source.pixels[index] = static_cast<uint8_t>((x * 7U) & 0xFFU);
            source.pixels[index + 1U] = static_cast<uint8_t>((y * 11U) & 0xFFU);
            source.pixels[index + 2U] = static_cast<uint8_t>(((x + y) * 5U) & 0xFFU);
            source.pixels[index + 3U] = 0xFFU;
        }
    }

    WidgetGlassEffectDistortionSettings distortion{};
    distortion.edge_strength = 0.0F;
    distortion.chromatic_shift = 0.0F;
    distortion.lens_chromatic_shift = 0.0F;
    WidgetGlassEffectAnimationSettings animation{};
    animation.motion_wave_enabled = true;
    animation.motion_wave_progress = 0.55F;
    animation.motion_wave_count = 2;
    animation.motion_wave_refraction = 9.0F;
    animation.motion_wave_speed = 1.0F;
    animation.motion_wave_wavelength = 28.0F;

    const WidgetGlassEffectFrame result = DistortGlassEffectFrame(source, {}, distortion, animation);
    size_t changed_bytes = 0;
    for (size_t index = 0; index < source.pixels.size(); ++index) {
        changed_bytes += source.pixels[index] != result.pixels[index] ? 1U : 0U;
    }
    Require(changed_bytes > 300U, "Wave ne deforme pas suffisamment la frame");
}

// ----------------------------------------------------------------------------
// Verifie que les trois animations Glass principales passent dans le shader.
// ----------------------------------------------------------------------------
void TestGpuGlassAnimations() {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature_level{};
    Require(
        SUCCEEDED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            device.GetAddressOf(),
            &feature_level,
            context.GetAddressOf()
        )),
        "device WARP indisponible"
    );

    // Largeur de la texture synthetique validee par le test.
    constexpr uint32_t width = 96;

    // Hauteur de la texture synthetique validee par le test.
    constexpr uint32_t height = 64;

    // Pas BGRA compact de la texture synthetique.
    constexpr uint32_t stride = width * 4U;
    std::vector<uint8_t> pixels(static_cast<size_t>(stride) * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t index = static_cast<size_t>(y) * stride + (x * 4U);
            pixels[index] = static_cast<uint8_t>((x * 7U) & 0xFFU);
            pixels[index + 1U] = static_cast<uint8_t>((y * 11U) & 0xFFU);
            pixels[index + 2U] = static_cast<uint8_t>(((x + y) * 5U) & 0xFFU);
            pixels[index + 3U] = 0xFFU;
        }
    }

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA initial_data{pixels.data(), stride, 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Require(
        SUCCEEDED(device->CreateTexture2D(&texture_desc, &initial_data, texture.GetAddressOf())),
        "texture GPU de test indisponible"
    );

    WidgetGlassEffectFrame source{};
    source.width = width;
    source.height = height;
    source.stride = stride;
    source.generation = 7;
    source.pixels = pixels;
    source.gpu_texture = texture.Get();
    source.gpu_device = device.Get();
    source.gpu_context = context.Get();

    WidgetGlassEffectGpuProcessor processor;
    WidgetGlassEffectFrame readback;
    Require(processor.Readback(source, readback), "lecture GPU de secours impossible");
    Require(readback.pixels == pixels, "lecture GPU de secours alteree");
    for (int mask = 1; mask < 8; ++mask) {
        GlassEffectSettings settings = DefaultGlassEffectSettings();
        settings.calm_water.enabled = (mask & 1) != 0;
        settings.liquid.enabled = (mask & 2) != 0;
        settings.rain.enabled = (mask & 4) != 0;
        WidgetGlassEffectAnimationSettings animation = GlassEffectAnimationSettingsForSettings(settings, 2.0F);
        WidgetGlassEffectFrame output;
        Require(
            processor.Process(source, WidgetGlassEffectDistortionSettings{}, animation, output),
            "combinaison cumulative Glass GPU impossible"
        );
        animation.time_seconds += 0.5F;
        WidgetGlassEffectFrame later_output;
        Require(
            processor.Process(source, WidgetGlassEffectDistortionSettings{}, animation, later_output),
            "seconde frame cumulative Glass GPU impossible"
        );
        size_t animated_bytes = 0;
        for (size_t index = 0; index < output.pixels.size(); ++index) {
            animated_bytes += output.pixels[index] != later_output.pixels[index] ? 1U : 0U;
        }
        Require(animated_bytes > 300U, "combinaison cumulative Glass GPU statique");

        animation.time_seconds -= 0.5F;
        const WidgetGlassEffectFrame cpu_output = DistortGlassEffectFrame(
            source,
            {},
            WidgetGlassEffectDistortionSettings{},
            animation
        );
        animation.time_seconds += 0.5F;
        const WidgetGlassEffectFrame cpu_later_output = DistortGlassEffectFrame(
            source,
            {},
            WidgetGlassEffectDistortionSettings{},
            animation
        );
        Require(cpu_output.pixels != cpu_later_output.pixels, "combinaison cumulative Glass CPU statique");
    }
}

// ----------------------------------------------------------------------------
// Verifie que Wave produit un etat de composition meme sans fond Glass actif.
// ----------------------------------------------------------------------------
void TestWaveCompositionState() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.wave.enabled = true;
    settings.wave.direction = WidgetMotionWaveDirection::Left;
    settings.wave.intensity_percent = 200;
    settings.wave.refraction_percent = 150;

    const WidgetVibrationGlassEffectState state = WidgetVibrationGlassEffect{}.Evaluate(
        settings,
        GlassEffectMode::Off,
        1.0,
        0.0,
        1.0,
        0.35
    );
    Require(state.wave_refraction > 0.0F, "Wave depend encore du fond Glass");
    Require(std::fabs(state.wave_position - 0.35F) < 0.001F, "progression Wave incorrecte");
    Require(state.wave_direction == WidgetMotionWaveDirection::Left, "direction Wave perdue");
}

// ----------------------------------------------------------------------------
// Verifie que le front vertical balaie le widget du bord droit vers la gauche.
// ----------------------------------------------------------------------------
void TestWaveRightToLeftSweep() {
    WidgetVibrationGlassEffectState wave{};
    wave.wave_direction = WidgetMotionWaveDirection::Left;
    wave.wave_wavelength = 100.0F;
    wave.wave_count = 2;
    wave.wave_damping = 0.7F;
    wave.wave_refraction = 12.0F;
    wave.wave_speed = 1.0F;

    wave.wave_position = 0.08F;
    const float early_right = EvaluateWidgetMotionWaveStripOffset(310.0F, 340.0F, wave);
    const float early_left = EvaluateWidgetMotionWaveStripOffset(40.0F, 340.0F, wave);
    Require(std::fabs(early_right) > 0.01F, "Wave n'entre pas par le bord droit");
    Require(std::fabs(early_left) < 0.01F, "Wave atteint trop tot le bord gauche");

    wave.wave_position = 0.65F;
    const float late_left = EvaluateWidgetMotionWaveStripOffset(50.0F, 340.0F, wave);
    const float late_right = EvaluateWidgetMotionWaveStripOffset(310.0F, 340.0F, wave);
    Require(std::fabs(late_left) > 0.01F, "Wave ne progresse pas vers la gauche");
    Require(std::fabs(late_right) < 0.01F, "la trainee Wave reste bloquee a droite");
}

// ----------------------------------------------------------------------------
// Mesure le cout CPU du champ Wave sur les trois tailles representatives.
// ----------------------------------------------------------------------------
void MeasureWaveFieldCost() {
    WidgetMotionWaveSettings settings{};
    settings.enabled = true;
    settings.direction = WidgetMotionWaveDirection::Radial;
    volatile float sink = 0.0F;
    const int sizes[][2]{{320, 180}, {520, 360}, {760, 560}};
    for (const auto& size : sizes) {
        const auto started_at = std::chrono::steady_clock::now();
        for (int y = 0; y < size[1]; y += 2) {
            for (int x = 0; x < size[0]; x += 2) {
                const WidgetMotionWaveSample sample = EvaluateWidgetMotionWaveField(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(size[0]),
                    static_cast<float>(size[1]),
                    0.62F,
                    settings
                );
                sink = sink + sample.offset_x + sample.offset_y;
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started_at
        );
        std::cout << "Wave " << size[0] << "x" << size[1] << ": "
            << elapsed.count() << " us (echantillonnage 2x2)\n";
    }
    Require(std::isfinite(sink), "mesure Wave non finie");
}

} // namespace

// ----------------------------------------------------------------------------
// Execute tous les tests des reglages Effets Motion.
//
// Retour :
// - 0 si tous les tests passent ; une exception termine sinon le processus.
// ----------------------------------------------------------------------------
int main() {
    try {
        TestGlassEffectSettings();
        TestNormalization();
        TestIndividualMotionResets();
        TestLegacyMigration();
        TestIniRoundTrip();
        TestDebugDefaultDuringMigration();
        TestMotionCurves();
        TestPulseCompositionState();
        TestPulseRendering();
        TestOffsetsAndTimelines();
        TestWaveField();
        TestWavePixelDistortion();
        TestGpuGlassAnimations();
        TestWaveCompositionState();
        TestWaveRightToLeftSweep();
        MeasureWaveFieldCost();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Echec WidgetMotionEffectsTests: " << error.what() << '\n';
        return 1;
    }
}

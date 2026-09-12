// ============================================================================
// Codex Glass - Implementation du renderer GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier initialise uniquement le socle D3D11 du futur pipeline GlassEffect.
// Aucun shader, capture, blur ou composite n'est execute en phase F.
// ============================================================================

#include "WidgetGlassEffectRenderer.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <memory>

using Microsoft::WRL::ComPtr;

// ----------------------------------------------------------------------------
// Implementation privee du renderer GlassEffect.
// ----------------------------------------------------------------------------
struct WidgetGlassEffectRenderer::Impl {
    // Device D3D11 utilise par le futur pipeline GlassEffect.
    ComPtr<ID3D11Device> device;

    // Contexte immediat D3D11 associe au device.
    ComPtr<ID3D11DeviceContext> context;

    // Niveau de fonctionnalite obtenu a l'initialisation.
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_10_0;
};

// ----------------------------------------------------------------------------
// Cree un renderer sans initialiser D3D11.
// ----------------------------------------------------------------------------
WidgetGlassEffectRenderer::WidgetGlassEffectRenderer() = default;

// ----------------------------------------------------------------------------
// Detruit le renderer et ses ressources D3D11.
// ----------------------------------------------------------------------------
WidgetGlassEffectRenderer::~WidgetGlassEffectRenderer() = default;

// ----------------------------------------------------------------------------
// Initialise le device D3D11 si necessaire.
//
// Retour :
// - true si le renderer est disponible.
// - false si D3D11 n'a pas pu etre initialise.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectRenderer::Initialize() {
    if (impl_ != nullptr && impl_->device) {
        if (SUCCEEDED(impl_->device->GetDeviceRemovedReason())) {
            return true;
        }
        Shutdown();
    }

    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }

    // Niveaux Direct3D testes du plus recent au plus compatible.
    constexpr std::array<D3D_FEATURE_LEVEL, 4> requested_feature_levels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    const HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        requested_feature_levels.data(),
        static_cast<UINT>(requested_feature_levels.size()),
        D3D11_SDK_VERSION,
        impl_->device.GetAddressOf(),
        &impl_->feature_level,
        impl_->context.GetAddressOf()
    );

    if (FAILED(result)) {
        Shutdown();
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
// Libere toutes les ressources D3D11 possedees.
// ----------------------------------------------------------------------------
void WidgetGlassEffectRenderer::Shutdown() {
    impl_.reset();
}

// ----------------------------------------------------------------------------
// Indique si le renderer D3D11 est pret.
//
// Retour :
// - true si Initialize a reussi.
// ----------------------------------------------------------------------------
bool WidgetGlassEffectRenderer::IsInitialized() const {
    return impl_ != nullptr
        && impl_->device
        && SUCCEEDED(impl_->device->GetDeviceRemovedReason());
}

// ----------------------------------------------------------------------------
// Retourne le device D3D11 initialise.
//
// Retour :
// - pointeur brut non possede, ou nullptr si le renderer est inactif.
// ----------------------------------------------------------------------------
ID3D11Device* WidgetGlassEffectRenderer::Device() const {
    if (impl_ == nullptr) {
        return nullptr;
    }

    return impl_->device.Get();
}

// ----------------------------------------------------------------------------
// Retourne le contexte immediat D3D11 initialise.
//
// Retour :
// - pointeur brut non possede, ou nullptr si le renderer est inactif.
// ----------------------------------------------------------------------------
ID3D11DeviceContext* WidgetGlassEffectRenderer::Context() const {
    if (impl_ == nullptr) {
        return nullptr;
    }

    return impl_->context.Get();
}

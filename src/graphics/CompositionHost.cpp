// ============================================================================
// Codex Deck - Implementation de l'hote DirectComposition
// ----------------------------------------------------------------------------
// Ce fichier cree une swap chain de composition et la relie a un contexte
// Direct2D 1.1. Le renderer reste responsable du dessin du contenu.
// ============================================================================

#include "CompositionHost.h"

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <iterator>
#include <memory>

using Microsoft::WRL::ComPtr;

struct CompositionHost::Impl {
    // Fenetre Win32 cible de la composition.
    HWND hwnd = nullptr;

    // Device D3D11 supportant BGRA pour Direct2D.
    ComPtr<ID3D11Device> d3d_device;

    // Device DXGI associe au device D3D11.
    ComPtr<IDXGIDevice> dxgi_device;

    // Factory DXGI utilisee pour creer la swap chain de composition.
    ComPtr<IDXGIFactory2> dxgi_factory;

    // Factory Direct2D 1.1.
    ComPtr<ID2D1Factory1> d2d_factory;

    // Device Direct2D lie a DXGI.
    ComPtr<ID2D1Device> d2d_device;

    // Contexte Direct2D utilise par le renderer.
    ComPtr<ID2D1DeviceContext> d2d_context;

    // Device DirectComposition principal.
    ComPtr<IDCompositionDevice> composition_device;

    // Cible DirectComposition liee a la fenetre.
    ComPtr<IDCompositionTarget> composition_target;

    // Visuel racine contenant la swap chain.
    ComPtr<IDCompositionVisual> root_visual;

    // Swap chain flip model presentee par DirectComposition.
    ComPtr<IDXGISwapChain1> swap_chain;

    // Bitmap Direct2D representant le buffer courant.
    ComPtr<ID2D1Bitmap1> target_bitmap;

    // Largeur courante de la swap chain.
    UINT width = 1;

    // Hauteur courante de la swap chain.
    UINT height = 1;

    // DPI courant applique au bitmap Direct2D.
    float dpi = 96.0F;
};

namespace {

// ----------------------------------------------------------------------------
// Construit une erreur graphique courte.
//
// Parametres :
// - code : famille de l'erreur.
// - result : HRESULT recu.
// - message : diagnostic court.
//
// Retour :
// - erreur structuree.
// ----------------------------------------------------------------------------
GraphicsError MakeError(GraphicsErrorCode code, HRESULT result, const wchar_t* message) {
    return GraphicsError{code, result, message};
}

// ----------------------------------------------------------------------------
// Retourne une dimension DXGI valide.
//
// Parametres :
// - value : dimension demandee par la fenetre.
//
// Retour :
// - dimension au moins egale a un pixel.
// ----------------------------------------------------------------------------
UINT ClampDimension(UINT value) {
    return std::max<UINT>(1, value);
}

}  // namespace

// ----------------------------------------------------------------------------
// Libere les ressources graphiques possedees.
// ----------------------------------------------------------------------------
CompositionHost::~CompositionHost() {
    delete impl_;
}

// ----------------------------------------------------------------------------
// Initialise les devices graphiques et la cible DirectComposition.
// ----------------------------------------------------------------------------
std::expected<void, GraphicsError> CompositionHost::Initialize(HWND hwnd) {
    if (impl_ == nullptr) {
        impl_ = new Impl();
    }
    if (impl_->hwnd == hwnd
        && impl_->d2d_context
        && impl_->composition_device
        && impl_->composition_target
        && impl_->root_visual) {
        return {};
    }
    impl_->hwnd = hwnd;

    UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selected_level{};
    HRESULT result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creation_flags,
        feature_levels,
        static_cast<UINT>(std::size(feature_levels)),
        D3D11_SDK_VERSION,
        impl_->d3d_device.GetAddressOf(),
        &selected_level,
        nullptr
    );
#if defined(_DEBUG)
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        creation_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creation_flags,
            feature_levels,
            static_cast<UINT>(std::size(feature_levels)),
            D3D11_SDK_VERSION,
            impl_->d3d_device.GetAddressOf(),
            &selected_level,
            nullptr
        );
    }
#endif
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D3DDeviceCreation, result, L"Creation D3D11 impossible"));
    }

    result = impl_->d3d_device.As(&impl_->dxgi_device);
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D3DDeviceCreation, result, L"Device DXGI indisponible"));
    }

    ComPtr<IDXGIAdapter> adapter;
    result = impl_->dxgi_device->GetAdapter(adapter.GetAddressOf());
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D3DDeviceCreation, result, L"Adaptateur DXGI indisponible"));
    }
    result = adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(impl_->dxgi_factory.GetAddressOf()));
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D3DDeviceCreation, result, L"Factory DXGI indisponible"));
    }

    D2D1_FACTORY_OPTIONS options{};
#if defined(_DEBUG)
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        options,
        impl_->d2d_factory.GetAddressOf()
    );
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D2DDeviceCreation, result, L"Factory Direct2D impossible"));
    }

    result = impl_->d2d_factory->CreateDevice(impl_->dxgi_device.Get(), impl_->d2d_device.GetAddressOf());
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D2DDeviceCreation, result, L"Device Direct2D impossible"));
    }
    result = impl_->d2d_device->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        impl_->d2d_context.GetAddressOf()
    );
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::D2DDeviceCreation, result, L"Contexte Direct2D impossible"));
    }

    result = DCompositionCreateDevice(
        impl_->dxgi_device.Get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(impl_->composition_device.GetAddressOf())
    );
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::CompositionDeviceCreation, result, L"Device DirectComposition impossible"));
    }
    result = impl_->composition_device->CreateTargetForHwnd(
        hwnd,
        TRUE,
        impl_->composition_target.GetAddressOf()
    );
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::CompositionTargetCreation, result, L"Cible DirectComposition impossible"));
    }
    result = impl_->composition_device->CreateVisual(impl_->root_visual.GetAddressOf());
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::CompositionTargetCreation, result, L"Visuel DirectComposition impossible"));
    }
    impl_->composition_target->SetRoot(impl_->root_visual.Get());
    return {};
}

// ----------------------------------------------------------------------------
// Adapte la swap chain a la taille cliente courante.
// ----------------------------------------------------------------------------
void CompositionHost::Resize(UINT width, UINT height, float dpi) {
    if (impl_ == nullptr || !impl_->dxgi_factory) {
        return;
    }

    impl_->width = ClampDimension(width);
    impl_->height = ClampDimension(height);
    impl_->dpi = dpi > 0.0F ? dpi : 96.0F;
    impl_->target_bitmap.Reset();
    if (impl_->d2d_context) {
        impl_->d2d_context->SetTarget(nullptr);
    }

    if (impl_->swap_chain) {
        impl_->swap_chain->ResizeBuffers(2, impl_->width, impl_->height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = impl_->width;
    desc.Height = impl_->height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    if (FAILED(impl_->dxgi_factory->CreateSwapChainForComposition(
            impl_->d3d_device.Get(),
            &desc,
            nullptr,
            impl_->swap_chain.GetAddressOf()
        ))) {
        return;
    }
    impl_->root_visual->SetContent(impl_->swap_chain.Get());
    impl_->composition_device->Commit();
}

// ----------------------------------------------------------------------------
// Prepare le contexte Direct2D pour un dessin.
// ----------------------------------------------------------------------------
std::expected<ID2D1DeviceContext*, GraphicsError> CompositionHost::BeginDraw() {
    if (impl_ == nullptr || !impl_->d2d_context) {
        return std::unexpected(MakeError(GraphicsErrorCode::D2DDeviceCreation, E_FAIL, L"Composition non initialisee"));
    }
    if (!impl_->swap_chain) {
        Resize(impl_->width, impl_->height, impl_->dpi);
    }
    if (!impl_->swap_chain) {
        return std::unexpected(MakeError(GraphicsErrorCode::SwapChainCreation, E_FAIL, L"Swap chain indisponible"));
    }
    if (!impl_->target_bitmap) {
        ComPtr<IDXGISurface> surface;
        HRESULT result = impl_->swap_chain->GetBuffer(0, __uuidof(IDXGISurface), reinterpret_cast<void**>(surface.GetAddressOf()));
        if (FAILED(result)) {
            return std::unexpected(MakeError(GraphicsErrorCode::SurfaceBinding, result, L"Buffer DXGI indisponible"));
        }

        const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            impl_->dpi,
            impl_->dpi
        );
        result = impl_->d2d_context->CreateBitmapFromDxgiSurface(
            surface.Get(),
            &properties,
            impl_->target_bitmap.GetAddressOf()
        );
        if (FAILED(result)) {
            return std::unexpected(MakeError(GraphicsErrorCode::SurfaceBinding, result, L"Bitmap Direct2D impossible"));
        }
    }

    impl_->d2d_context->SetTarget(impl_->target_bitmap.Get());
    impl_->d2d_context->BeginDraw();
    return impl_->d2d_context.Get();
}

// ----------------------------------------------------------------------------
// Termine le dessin et presente la swap chain.
// ----------------------------------------------------------------------------
std::expected<void, GraphicsError> CompositionHost::EndDraw() {
    if (impl_ == nullptr || !impl_->d2d_context || !impl_->swap_chain) {
        return std::unexpected(MakeError(GraphicsErrorCode::PresentFailed, E_FAIL, L"Composition non initialisee"));
    }

    HRESULT result = impl_->d2d_context->EndDraw();
    if (FAILED(result)) {
        impl_->target_bitmap.Reset();
        return std::unexpected(MakeError(GraphicsErrorCode::SurfaceBinding, result, L"Dessin Direct2D incomplet"));
    }
    result = impl_->swap_chain->Present(1, 0);
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::PresentFailed, result, L"Presentation DXGI impossible"));
    }
    result = impl_->composition_device->Commit();
    if (FAILED(result)) {
        return std::unexpected(MakeError(GraphicsErrorCode::PresentFailed, result, L"Commit DirectComposition impossible"));
    }
    return {};
}

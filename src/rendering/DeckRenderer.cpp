// ============================================================================
// Codex Deck - Implementation du rendu Direct2D minimal
// ----------------------------------------------------------------------------
// Ce fichier cree un render target HWND temporaire pour le bootstrap. Le plan
// DirectComposition remplacera cette cible sans toucher a DeckApp.
// ============================================================================

#include "DeckRenderer.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <memory>

using Microsoft::WRL::ComPtr;

struct DeckRenderer::Impl {
    // Factory Direct2D partagee par les ressources de rendu.
    ComPtr<ID2D1Factory> d2d_factory;

    // Factory DirectWrite utilisee pour les formats de texte.
    ComPtr<IDWriteFactory> dwrite_factory;

    // Cible HWND Direct2D du bootstrap.
    ComPtr<ID2D1HwndRenderTarget> render_target;

    // Brosse du fond principal.
    ComPtr<ID2D1SolidColorBrush> background_brush;

    // Brosse du titre.
    ComPtr<ID2D1SolidColorBrush> title_brush;

    // Brosse du sous-titre.
    ComPtr<ID2D1SolidColorBrush> subtitle_brush;

    // Format du titre principal.
    ComPtr<IDWriteTextFormat> title_format;

    // Format du sous-titre.
    ComPtr<IDWriteTextFormat> subtitle_format;
};

namespace {

// Taille du texte du titre en DIPs.
constexpr float kDeckTitleTextSize = 34.0F;

// Taille du texte secondaire en DIPs.
constexpr float kDeckSubtitleTextSize = 16.0F;

// Marge gauche du contenu de bootstrap en DIPs.
constexpr float kDeckContentLeft = 48.0F;

// Marge haute du contenu de bootstrap en DIPs.
constexpr float kDeckContentTop = 48.0F;

// Hauteur reservee au titre en DIPs.
constexpr float kDeckTitleHeight = 46.0F;

// Hauteur reservee au sous-titre en DIPs.
constexpr float kDeckSubtitleHeight = 28.0F;

// ----------------------------------------------------------------------------
// Retourne la taille cliente de la fenetre cible.
//
// Parametres :
// - hwnd : fenetre a mesurer.
//
// Retour :
// - taille Direct2D en pixels.
// ----------------------------------------------------------------------------
D2D1_SIZE_U ClientPixelSize(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    return D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(0, client.right - client.left)),
        static_cast<UINT32>(std::max<LONG>(0, client.bottom - client.top))
    );
}

}  // namespace

// ----------------------------------------------------------------------------
// Initialise les factories et les ressources liees a la fenetre.
// ----------------------------------------------------------------------------
bool DeckRenderer::Initialize(HWND hwnd) {
    if (impl_ == nullptr) {
        impl_ = new Impl();
    }
    if (!impl_->d2d_factory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, impl_->d2d_factory.GetAddressOf()))) {
        return false;
    }
    if (!impl_->dwrite_factory && FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(impl_->dwrite_factory.GetAddressOf())
        ))) {
        return false;
    }
    if (!impl_->render_target) {
        const D2D1_SIZE_U size = ClientPixelSize(hwnd);
        const D2D1_RENDER_TARGET_PROPERTIES target_properties = D2D1::RenderTargetProperties();
        const D2D1_HWND_RENDER_TARGET_PROPERTIES hwnd_properties = D2D1::HwndRenderTargetProperties(hwnd, size);
        if (FAILED(impl_->d2d_factory->CreateHwndRenderTarget(
                target_properties,
                hwnd_properties,
                impl_->render_target.GetAddressOf()
            ))) {
            return false;
        }
    }
    if (!impl_->background_brush) {
        impl_->render_target->CreateSolidColorBrush(
            D2D1::ColorF(0.07F, 0.08F, 0.10F, 1.0F),
            impl_->background_brush.GetAddressOf()
        );
        impl_->render_target->CreateSolidColorBrush(
            D2D1::ColorF(0.92F, 0.95F, 0.98F, 1.0F),
            impl_->title_brush.GetAddressOf()
        );
        impl_->render_target->CreateSolidColorBrush(
            D2D1::ColorF(0.56F, 0.62F, 0.70F, 1.0F),
            impl_->subtitle_brush.GetAddressOf()
        );
    }
    if (!impl_->title_format) {
        impl_->dwrite_factory->CreateTextFormat(
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            kDeckTitleTextSize,
            L"",
            impl_->title_format.GetAddressOf()
        );
        impl_->dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            kDeckSubtitleTextSize,
            L"",
            impl_->subtitle_format.GetAddressOf()
        );
    }
    return impl_->background_brush && impl_->title_brush && impl_->subtitle_brush
        && impl_->title_format && impl_->subtitle_format;
}

// ----------------------------------------------------------------------------
// Redimensionne la cible Direct2D pour suivre le client Win32.
// ----------------------------------------------------------------------------
void DeckRenderer::Resize(HWND hwnd) {
    if (impl_ != nullptr && impl_->render_target) {
        impl_->render_target->Resize(ClientPixelSize(hwnd));
    }
}

// ----------------------------------------------------------------------------
// Dessine l'etat visuel courant.
// ----------------------------------------------------------------------------
void DeckRenderer::Render(HWND hwnd, const DeckVisualState& state) {
    if (!Initialize(hwnd)) {
        return;
    }

    const D2D1_SIZE_F size = impl_->render_target->GetSize();
    impl_->render_target->BeginDraw();
    impl_->render_target->Clear(D2D1::ColorF(0.07F, 0.08F, 0.10F, 1.0F));
    impl_->render_target->FillRectangle(D2D1::RectF(0.0F, 0.0F, size.width, size.height), impl_->background_brush.Get());
    impl_->render_target->DrawTextW(
        state.title.c_str(),
        static_cast<UINT32>(state.title.size()),
        impl_->title_format.Get(),
        D2D1::RectF(kDeckContentLeft, kDeckContentTop, size.width - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight),
        impl_->title_brush.Get()
    );
    impl_->render_target->DrawTextW(
        state.subtitle.c_str(),
        static_cast<UINT32>(state.subtitle.size()),
        impl_->subtitle_format.Get(),
        D2D1::RectF(kDeckContentLeft, kDeckContentTop + kDeckTitleHeight, size.width - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight + kDeckSubtitleHeight),
        impl_->subtitle_brush.Get()
    );

    if (impl_->render_target->EndDraw() == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

// ----------------------------------------------------------------------------
// Libere les ressources graphiques dependantes du device.
// ----------------------------------------------------------------------------
void DeckRenderer::DiscardDeviceResources() {
    if (impl_ == nullptr) {
        return;
    }
    impl_->subtitle_format.Reset();
    impl_->title_format.Reset();
    impl_->subtitle_brush.Reset();
    impl_->title_brush.Reset();
    impl_->background_brush.Reset();
    impl_->render_target.Reset();
}

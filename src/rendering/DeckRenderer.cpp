// ============================================================================
// Codex Deck - Implementation du rendu DirectComposition minimal
// ----------------------------------------------------------------------------
// Ce fichier dessine la coquille visuelle avec Direct2D sur la surface fournie
// par CompositionHost. Il ne gere ni fenetre, ni protocole Codex, ni stockage.
// ============================================================================

#include "DeckRenderer.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

struct DeckRenderer::Impl {
    // Hote DirectComposition proprietaire de la surface de dessin.
    CompositionHost composition;

    // Factory DirectWrite utilisee pour les formats de texte.
    ComPtr<IDWriteFactory> dwrite_factory;

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

// DPI Win32 standard utilise en repli.
constexpr float kDefaultDpi = 96.0F;

// ----------------------------------------------------------------------------
// Retourne la taille cliente de la fenetre cible.
//
// Parametres :
// - hwnd : fenetre a mesurer.
//
// Retour :
// - taille en pixels, bornee a un pixel par axe.
// ----------------------------------------------------------------------------
D2D1_SIZE_U ClientPixelSize(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    return D2D1::SizeU(
        static_cast<UINT32>(std::max<LONG>(1, client.right - client.left)),
        static_cast<UINT32>(std::max<LONG>(1, client.bottom - client.top))
    );
}

// ----------------------------------------------------------------------------
// Retourne le DPI courant de la fenetre.
//
// Parametres :
// - hwnd : fenetre Win32 cible.
//
// Retour :
// - DPI positif en points par pouce.
// ----------------------------------------------------------------------------
float WindowDpi(HWND hwnd) {
    const UINT dpi = GetDpiForWindow(hwnd);
    return dpi == 0 ? kDefaultDpi : static_cast<float>(dpi);
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree un renderer sans allouer encore de ressources natives.
// ----------------------------------------------------------------------------
DeckRenderer::DeckRenderer() = default;

// ----------------------------------------------------------------------------
// Libere les ressources de rendu opaques.
// ----------------------------------------------------------------------------
DeckRenderer::~DeckRenderer() = default;

// ----------------------------------------------------------------------------
// Initialise les factories et les ressources liees a la fenetre.
// ----------------------------------------------------------------------------
bool DeckRenderer::Initialize(HWND hwnd) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    if (!impl_->composition.Initialize(hwnd).has_value()) {
        return false;
    }
    Resize(hwnd);
    if (!impl_->dwrite_factory && FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(impl_->dwrite_factory.GetAddressOf())
        ))) {
        return false;
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
    return impl_->title_format && impl_->subtitle_format;
}

// ----------------------------------------------------------------------------
// Redimensionne la cible DirectComposition pour suivre le client Win32.
// ----------------------------------------------------------------------------
void DeckRenderer::Resize(HWND hwnd) {
    if (impl_ == nullptr) {
        return;
    }
    const D2D1_SIZE_U size = ClientPixelSize(hwnd);
    impl_->composition.Resize(size.width, size.height, WindowDpi(hwnd));
}

// ----------------------------------------------------------------------------
// Dessine l'etat visuel courant.
// ----------------------------------------------------------------------------
void DeckRenderer::Render(HWND hwnd, const DeckVisualState& state, const ThemePalette& palette) {
    if (!Initialize(hwnd)) {
        return;
    }

    auto context_result = impl_->composition.BeginDraw();
    if (!context_result.has_value()) {
        return;
    }
    ID2D1DeviceContext* context = *context_result;
    if (!impl_->background_brush) {
        context->CreateSolidColorBrush(
            palette.window_background,
            impl_->background_brush.GetAddressOf()
        );
        context->CreateSolidColorBrush(
            palette.text,
            impl_->title_brush.GetAddressOf()
        );
        context->CreateSolidColorBrush(
            palette.text_muted,
            impl_->subtitle_brush.GetAddressOf()
        );
    }

    const D2D1_SIZE_F size = context->GetSize();
    impl_->background_brush->SetColor(palette.window_background);
    impl_->title_brush->SetColor(palette.text);
    impl_->subtitle_brush->SetColor(palette.text_muted);
    context->Clear(palette.window_background);
    context->FillRectangle(D2D1::RectF(0.0F, 0.0F, size.width, size.height), impl_->background_brush.Get());
    context->DrawTextW(
        state.title.c_str(),
        static_cast<UINT32>(state.title.size()),
        impl_->title_format.Get(),
        D2D1::RectF(kDeckContentLeft, kDeckContentTop, size.width - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight),
        impl_->title_brush.Get()
    );
    context->DrawTextW(
        state.subtitle.c_str(),
        static_cast<UINT32>(state.subtitle.size()),
        impl_->subtitle_format.Get(),
        D2D1::RectF(kDeckContentLeft, kDeckContentTop + kDeckTitleHeight, size.width - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight + kDeckSubtitleHeight),
        impl_->subtitle_brush.Get()
    );
    if (!impl_->composition.EndDraw().has_value()) {
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
}

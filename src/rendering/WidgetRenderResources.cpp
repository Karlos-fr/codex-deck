// ============================================================================
// Codex Glass - Implementation des ressources natives du rendu
// ----------------------------------------------------------------------------
// Ce fichier gere les factories, formats, brosses et render target Direct2D /
// DirectWrite. Le dessin metier reste orchestre par WidgetRendering.cpp.
// ============================================================================

#include "WidgetRenderResources.h"

#include "WidgetRenderConstants.h"
#include "WidgetRenderPalette.h"

#include <algorithm>

namespace {

// Proportion d'eclaircissement appliquee au fond des cartes KPI minimales.
constexpr float kMinimalCardLightening = 0.10F;

// Opacite minimale du voile clair des cartes KPI minimales.
constexpr float kMinimalCardMinimumOpacity = 0.18F;

// Part de l'opacite utilisateur ajoutee au voile des cartes KPI minimales.
constexpr float kMinimalCardOpacityScale = 0.16F;

// Opacite de base de l'ombre des cartes KPI minimales.
constexpr float kMinimalCardShadowOpacity = 0.16F;

// ----------------------------------------------------------------------------
// Convertit un rectangle Win32 en taille Direct2D.
//
// Parametres :
// - rect : rectangle Win32 a convertir.
//
// Retour :
// - taille Direct2D en pixels.
// ----------------------------------------------------------------------------
D2D1_SIZE_U SizeFromRect(const RECT& rect) {
    return D2D1::SizeU(
        static_cast<UINT32>(std::max(0L, rect.right - rect.left)),
        static_cast<UINT32>(std::max(0L, rect.bottom - rect.top))
    );
}

// ----------------------------------------------------------------------------
// Retourne le DPI courant de la fenetre pour synchroniser le render target.
//
// Parametres :
// - hwnd : fenetre associee au render target.
//
// Retour :
// - DPI courant, ou 96 en secours.
// ----------------------------------------------------------------------------
float DpiForRenderTarget(HWND hwnd) {
    if (hwnd != nullptr) {
        const UINT dpi = GetDpiForWindow(hwnd);
        if (dpi != 0) {
            return static_cast<float>(dpi);
        }
    }

    return kReferenceDpi;
}

}

// ----------------------------------------------------------------------------
// Initialise les factories Direct2D et DirectWrite independantes de la fenetre.
//
// Retour :
// - true si les factories sont disponibles.
// - false si l'initialisation a echoue.
// ----------------------------------------------------------------------------
bool WidgetRenderResources::Initialize() {
    HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory.GetAddressOf())
    );
    if (FAILED(result)) {
        return false;
    }

    return CreateTextFormats();
}

// ----------------------------------------------------------------------------
// Cree les formats de texte DirectWrite reutilises par le rendu.
//
// Retour :
// - true si tous les formats ont ete crees.
// - false si DirectWrite a signale une erreur.
// ----------------------------------------------------------------------------
bool WidgetRenderResources::CreateTextFormats() {
    HRESULT result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kTitleTextSize,
        L"fr-fr",
        title_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    title_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    title_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    title_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kTitleTextSize,
        L"fr-fr",
        minimal_title_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    minimal_title_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    minimal_title_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    minimal_title_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kBodyTextSize,
        L"fr-fr",
        body_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    body_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    body_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kCaptionTextSize,
        L"fr-fr",
        caption_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kCaptionTextSize,
        L"fr-fr",
        provider_summary_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    provider_summary_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    provider_summary_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    provider_summary_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kBodyTextSize,
        L"fr-fr",
        usage_value_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    usage_value_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    usage_value_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    usage_value_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kStatusTextSize,
        L"fr-fr",
        status_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    status_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    status_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    status_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kStatusTextSize,
        L"fr-fr",
        refreshing_status_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    refreshing_status_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    refreshing_status_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    refreshing_status_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kColorPanelTextSize,
        L"fr-fr",
        color_panel_label_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    color_panel_label_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    color_panel_label_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    color_panel_label_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kColorPanelTextSize,
        L"fr-fr",
        color_panel_button_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    color_panel_button_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    color_panel_button_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    color_panel_button_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kMinimalLabelTextSize,
        L"fr-fr",
        minimal_label_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    minimal_label_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    minimal_label_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    minimal_label_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    result = dwrite_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        kMinimalValueTextSize,
        L"fr-fr",
        minimal_value_text_format.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    minimal_value_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    minimal_value_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    minimal_value_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    return true;
}

// ----------------------------------------------------------------------------
// Libere les ressources Direct2D dependant de la fenetre.
// ----------------------------------------------------------------------------
void WidgetRenderResources::DiscardDeviceResources() {
    history_curve_brush.Reset();
    weekly_remaining_accent_brush.Reset();
    five_hour_remaining_accent_brush.Reset();
    progress_background_brush.Reset();
    muted_text_brush.Reset();
    body_text_brush.Reset();
    title_text_brush.Reset();
    minimal_card_shadow_brush.Reset();
    minimal_card_background_brush.Reset();
    border_brush.Reset();
    panel_background_brush.Reset();
    window_background_brush.Reset();
    render_target.Reset();
}

// ----------------------------------------------------------------------------
// Cree les ressources Direct2D dependant du render target de la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a laquelle associer le render target.
// - settings : reglages visuels courants.
//
// Retour :
// - true si toutes les ressources sont disponibles.
// - false si Direct2D a signale une erreur.
// ----------------------------------------------------------------------------
bool WidgetRenderResources::CreateDeviceResources(HWND hwnd, const AppSettings& settings) {
    if (render_target) {
        return true;
    }

    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);

    const D2D1_SIZE_U size = SizeFromRect(client_rect);
    HRESULT result = d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        render_target.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    const float dpi = DpiForRenderTarget(hwnd);
    render_target->SetDpi(dpi, dpi);

    const Palette palette = ActivePalette(settings);

    result = render_target->CreateSolidColorBrush(
        palette.window_background,
        window_background_brush.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    const float background_opacity = static_cast<float>(std::clamp(
        settings.background_opacity,
        kMinimumBackgroundOpacity,
        kMaximumBackgroundOpacity
    ));
    D2D1_COLOR_F panel_background = palette.panel_background;
    panel_background.a = background_opacity;

    result = render_target->CreateSolidColorBrush(
        panel_background,
        panel_background_brush.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.border, border_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    D2D1_COLOR_F minimal_card_background = palette.panel_background;
    minimal_card_background.r += (1.0F - minimal_card_background.r) * kMinimalCardLightening;
    minimal_card_background.g += (1.0F - minimal_card_background.g) * kMinimalCardLightening;
    minimal_card_background.b += (1.0F - minimal_card_background.b) * kMinimalCardLightening;
    minimal_card_background.a = kMinimalCardMinimumOpacity + (background_opacity * kMinimalCardOpacityScale);
    result = render_target->CreateSolidColorBrush(
        minimal_card_background,
        minimal_card_background_brush.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    const D2D1_COLOR_F minimal_card_shadow = D2D1::ColorF(
        0.0F,
        0.0F,
        0.0F,
        kMinimalCardShadowOpacity * background_opacity
    );
    result = render_target->CreateSolidColorBrush(
        minimal_card_shadow,
        minimal_card_shadow_brush.GetAddressOf()
    );
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.title_text, title_text_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.body_text, body_text_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.muted_text, muted_text_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.progress_background, progress_background_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.remaining_accent, five_hour_remaining_accent_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.remaining_accent, weekly_remaining_accent_brush.GetAddressOf());
    if (FAILED(result)) {
        return false;
    }

    result = render_target->CreateSolidColorBrush(palette.history_curve, history_curve_brush.GetAddressOf());
    return SUCCEEDED(result);
}

// ----------------------------------------------------------------------------
// Redimensionne le render target Direct2D lorsque la fenetre change.
//
// Parametres :
// - hwnd : handle de la fenetre redimensionnee.
// ----------------------------------------------------------------------------
void WidgetRenderResources::Resize(HWND hwnd) {
    if (!render_target) {
        return;
    }

    RECT client_rect{};
    GetClientRect(hwnd, &client_rect);
    render_target->Resize(SizeFromRect(client_rect));
    const float dpi = DpiForRenderTarget(hwnd);
    render_target->SetDpi(dpi, dpi);
}

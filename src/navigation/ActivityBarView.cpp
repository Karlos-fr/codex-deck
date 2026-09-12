// ============================================================================
// Codex Deck - Implementation de la barre d'activite
// ----------------------------------------------------------------------------
// Ce fichier rend des compteurs DirectWrite compacts pour les etats globaux du
// Tree, sans carte ni dependance au stockage.
// ============================================================================

#include "ActivityBarView.h"

#include <wrl/client.h>

#include <string>

using Microsoft::WRL::ComPtr;

namespace {

// Taille du texte de la barre d'activite.
constexpr float kActivityBarTextSize = 12.0F;

// Marge horizontale de la barre.
constexpr float kActivityBarPaddingX = 12.0F;

// ----------------------------------------------------------------------------
// Construit le texte compact des compteurs.
//
// Parametres :
// - counts : compteurs source.
//
// Retour :
// - texte affichable.
// ----------------------------------------------------------------------------
std::wstring ActivityText(const ActivityCounts& counts) {
    return L"Working " + std::to_wstring(counts.working)
        + L"   Attention " + std::to_wstring(counts.needs_attention)
        + L"   Completed today " + std::to_wstring(counts.completed_today)
        + L"   Ctrl+K";
}

}  // namespace

// ----------------------------------------------------------------------------
// Ressources opaques de la vue.
// ----------------------------------------------------------------------------
struct ActivityBarResources {
    // Factory DirectWrite partagee.
    ComPtr<IDWriteFactory> dwrite_factory;

    // Format de texte compact.
    ComPtr<IDWriteTextFormat> text_format;

    // Brosse de fond.
    ComPtr<ID2D1SolidColorBrush> background_brush;

    // Brosse de texte.
    ComPtr<ID2D1SolidColorBrush> text_brush;
};

namespace {
// Ressources partagees simples tant que la barre ne possede pas encore d'etat.
ActivityBarResources g_activity_bar_resources;
}

// ----------------------------------------------------------------------------
// Cree une vue sans ressources natives.
// ----------------------------------------------------------------------------
ActivityBarView::ActivityBarView() = default;

// ----------------------------------------------------------------------------
// Libere les ressources opaques.
// ----------------------------------------------------------------------------
ActivityBarView::~ActivityBarView() = default;

// ----------------------------------------------------------------------------
// Dessine les compteurs d'activite.
// ----------------------------------------------------------------------------
void ActivityBarView::Render(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& bounds,
    const ActivityCounts& counts,
    SessionFilter active_filter,
    const ThemePalette& palette
) {
    (void)active_filter;
    if (dc == nullptr) {
        return;
    }
    if (!g_activity_bar_resources.dwrite_factory && FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(g_activity_bar_resources.dwrite_factory.GetAddressOf())
        ))) {
        return;
    }
    if (!g_activity_bar_resources.text_format) {
        g_activity_bar_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            kActivityBarTextSize,
            L"",
            g_activity_bar_resources.text_format.GetAddressOf()
        );
    }
    if (!g_activity_bar_resources.background_brush) {
        dc->CreateSolidColorBrush(palette.window_background, g_activity_bar_resources.background_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.text_muted, g_activity_bar_resources.text_brush.GetAddressOf());
    }
    if (!g_activity_bar_resources.text_format || !g_activity_bar_resources.background_brush || !g_activity_bar_resources.text_brush) {
        return;
    }

    g_activity_bar_resources.background_brush->SetColor(palette.window_background);
    g_activity_bar_resources.text_brush->SetColor(palette.text_muted);
    dc->FillRectangle(bounds, g_activity_bar_resources.background_brush.Get());

    const std::wstring text = ActivityText(counts);
    dc->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        g_activity_bar_resources.text_format.Get(),
        D2D1::RectF(bounds.left + kActivityBarPaddingX, bounds.top + 6.0F, bounds.right - kActivityBarPaddingX, bounds.bottom),
        g_activity_bar_resources.text_brush.Get()
    );
}

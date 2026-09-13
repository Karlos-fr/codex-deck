// ============================================================================
// Codex Deck - Implementation de la barre d'activite
// ----------------------------------------------------------------------------
// Ce fichier rend des compteurs DirectWrite compacts pour les etats globaux du
// Tree, sans carte ni dependance au stockage.
// ============================================================================

#include "ActivityBarView.h"

#include <wrl/client.h>

#include <array>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {

// Taille du texte de la barre d'activite.
constexpr float kActivityBarTextSize = 13.0F;

// Marge horizontale de la barre.
constexpr float kActivityBarPaddingX = 14.0F;

// Hauteur d'un chip d'activite.
constexpr float kChipHeight = 26.0F;

// Rayon d'un chip d'activite.
constexpr float kChipRadius = 5.0F;

// Decrit un chip d'activite.
struct ActivityChip {
    // Filtre associe.
    SessionFilter filter;

    // Libelle affiche.
    const wchar_t* label;

    // Valeur affichee.
    std::size_t value;
};

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

    // Brosse de chip.
    ComPtr<ID2D1SolidColorBrush> chip_brush;

    // Brosse d'accent.
    ComPtr<ID2D1SolidColorBrush> accent_brush;

    // Brosse de bordure.
    ComPtr<ID2D1SolidColorBrush> border_brush;

    // Brosse du texte sur accent.
    ComPtr<ID2D1SolidColorBrush> accent_text_brush;
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
        dc->CreateSolidColorBrush(palette.surface, g_activity_bar_resources.background_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.text, g_activity_bar_resources.text_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.surface_hover, g_activity_bar_resources.chip_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.accent, g_activity_bar_resources.accent_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.border, g_activity_bar_resources.border_brush.GetAddressOf());
        dc->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), g_activity_bar_resources.accent_text_brush.GetAddressOf());
    }
    if (!g_activity_bar_resources.text_format
        || !g_activity_bar_resources.background_brush
        || !g_activity_bar_resources.text_brush
        || !g_activity_bar_resources.chip_brush
        || !g_activity_bar_resources.accent_brush
        || !g_activity_bar_resources.border_brush
        || !g_activity_bar_resources.accent_text_brush) {
        return;
    }

    g_activity_bar_resources.background_brush->SetColor(palette.window_background);
    g_activity_bar_resources.text_brush->SetColor(palette.text);
    g_activity_bar_resources.chip_brush->SetColor(palette.surface);
    g_activity_bar_resources.accent_brush->SetColor(palette.accent);
    g_activity_bar_resources.border_brush->SetColor(palette.border);
    g_activity_bar_resources.accent_text_brush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    dc->FillRectangle(bounds, g_activity_bar_resources.background_brush.Get());
    dc->DrawLine(
        D2D1::Point2F(bounds.left, bounds.bottom - 0.5F),
        D2D1::Point2F(bounds.right, bounds.bottom - 0.5F),
        g_activity_bar_resources.border_brush.Get(),
        1.0F
    );

    const std::array<ActivityChip, 3> chips{{
        {SessionFilter::Working, L"Working", counts.working},
        {SessionFilter::NeedsAttention, L"Attention", counts.needs_attention},
        {SessionFilter::CompletedToday, L"Done today", counts.completed_today},
    }};

    float x = bounds.left + kActivityBarPaddingX;
    const float y = bounds.top + (bounds.bottom - bounds.top - kChipHeight) * 0.5F;
    for (const ActivityChip& chip : chips) {
        const float width = chip.filter == SessionFilter::CompletedToday ? 118.0F : 98.0F;
        const D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + kChipHeight);
        const bool active = chip.filter == active_filter;
        dc->FillRoundedRectangle(
            D2D1::RoundedRect(rect, kChipRadius, kChipRadius),
            active ? g_activity_bar_resources.accent_brush.Get() : g_activity_bar_resources.chip_brush.Get()
        );
        const std::wstring text = std::wstring(chip.label) + L" " + std::to_wstring(chip.value);
        dc->DrawTextW(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            g_activity_bar_resources.text_format.Get(),
            D2D1::RectF(rect.left + 10.0F, rect.top + 5.0F, rect.right - 10.0F, rect.bottom),
            active ? g_activity_bar_resources.accent_text_brush.Get() : g_activity_bar_resources.text_brush.Get()
        );
        x += width + 8.0F;
    }

    const std::wstring text = L"Ctrl+K";
    dc->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        g_activity_bar_resources.text_format.Get(),
        D2D1::RectF(bounds.right - 72.0F, bounds.top + 12.0F, bounds.right - kActivityBarPaddingX, bounds.bottom),
        g_activity_bar_resources.text_brush.Get()
    );
}

// ============================================================================
// Codex Deck - Implementation de la vue Command Palette
// ----------------------------------------------------------------------------
// Ce fichier rend une palette compacte avec focus visuel et selection clavier,
// en reutilisant la surface composee par le renderer principal.
// ============================================================================

#include "CommandPaletteView.h"

#include <wrl/client.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

// Largeur maximale de la palette.
constexpr float kPaletteWidth = 620.0F;

// Hauteur de la zone de requete.
constexpr float kQueryHeight = 48.0F;

// Hauteur d'une ligne de resultat.
constexpr float kEntryHeight = 36.0F;

// Nombre maximal de lignes rendues.
constexpr std::size_t kVisibleEntries = 8;

// Rayon de coins de l'overlay.
constexpr float kPaletteRadius = 6.0F;

// Marge interne horizontale.
constexpr float kPalettePaddingX = 16.0F;

// ----------------------------------------------------------------------------
// Calcule le rectangle centre de palette.
//
// Parametres :
// - bounds : rectangle de fenetre.
// - entry_count : nombre d'entrees a rendre.
//
// Retour :
// - rectangle de palette.
// ----------------------------------------------------------------------------
D2D1_RECT_F PaletteRect(const D2D1_RECT_F& bounds, std::size_t entry_count) {
    const float width = std::min(kPaletteWidth, std::max(320.0F, bounds.right - bounds.left - 80.0F));
    const float height = kQueryHeight + static_cast<float>(std::min(entry_count, kVisibleEntries)) * kEntryHeight + 16.0F;
    const float left = bounds.left + ((bounds.right - bounds.left) - width) * 0.5F;
    return D2D1::RectF(left, bounds.top + 72.0F, left + width, bounds.top + 72.0F + height);
}

}  // namespace

// ----------------------------------------------------------------------------
// Ressources opaques de palette.
// ----------------------------------------------------------------------------
struct CommandPaletteResources {
    // Factory DirectWrite partagee.
    ComPtr<IDWriteFactory> dwrite_factory;

    // Format de requete.
    ComPtr<IDWriteTextFormat> query_format;

    // Format de resultat.
    ComPtr<IDWriteTextFormat> entry_format;

    // Brosse de fond.
    ComPtr<ID2D1SolidColorBrush> background_brush;

    // Brosse de selection.
    ComPtr<ID2D1SolidColorBrush> selection_brush;

    // Brosse de texte.
    ComPtr<ID2D1SolidColorBrush> text_brush;

    // Brosse de texte secondaire.
    ComPtr<ID2D1SolidColorBrush> muted_brush;
};

namespace {
// Ressources partagees tant que la palette reste une vue stateless.
CommandPaletteResources g_palette_resources;
}

// ----------------------------------------------------------------------------
// Cree une vue sans ressources natives.
// ----------------------------------------------------------------------------
CommandPaletteView::CommandPaletteView() = default;

// ----------------------------------------------------------------------------
// Libere les ressources opaques.
// ----------------------------------------------------------------------------
CommandPaletteView::~CommandPaletteView() = default;

// ----------------------------------------------------------------------------
// Dessine l'overlay et les entrees visibles.
// ----------------------------------------------------------------------------
void CommandPaletteView::Render(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& bounds,
    std::wstring_view query,
    std::span<const PaletteEntry> entries,
    std::size_t selected_index,
    const ThemePalette& palette
) {
    if (dc == nullptr) {
        return;
    }
    if (!g_palette_resources.dwrite_factory && FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(g_palette_resources.dwrite_factory.GetAddressOf())
        ))) {
        return;
    }
    if (!g_palette_resources.query_format) {
        g_palette_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            16.0F,
            L"",
            g_palette_resources.query_format.GetAddressOf()
        );
        g_palette_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            13.0F,
            L"",
            g_palette_resources.entry_format.GetAddressOf()
        );
    }
    if (!g_palette_resources.background_brush) {
        dc->CreateSolidColorBrush(palette.surface, g_palette_resources.background_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.surface_hover, g_palette_resources.selection_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.text, g_palette_resources.text_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.text_muted, g_palette_resources.muted_brush.GetAddressOf());
    }
    if (!g_palette_resources.query_format || !g_palette_resources.entry_format || !g_palette_resources.background_brush) {
        return;
    }

    g_palette_resources.background_brush->SetColor(palette.surface);
    g_palette_resources.selection_brush->SetColor(palette.surface_hover);
    g_palette_resources.text_brush->SetColor(palette.text);
    g_palette_resources.muted_brush->SetColor(palette.text_muted);

    const D2D1_RECT_F rect = PaletteRect(bounds, entries.size());
    dc->FillRoundedRectangle(D2D1::RoundedRect(rect, kPaletteRadius, kPaletteRadius), g_palette_resources.background_brush.Get());

    const std::wstring query_text = query.empty() ? L"Search commands" : std::wstring(query);
    dc->DrawTextW(
        query_text.c_str(),
        static_cast<UINT32>(query_text.size()),
        g_palette_resources.query_format.Get(),
        D2D1::RectF(rect.left + kPalettePaddingX, rect.top + 12.0F, rect.right - kPalettePaddingX, rect.top + kQueryHeight),
        g_palette_resources.text_brush.Get()
    );

    const std::size_t visible = std::min(entries.size(), kVisibleEntries);
    for (std::size_t index = 0; index < visible; ++index) {
        const float y = rect.top + kQueryHeight + static_cast<float>(index) * kEntryHeight;
        const D2D1_RECT_F row_rect = D2D1::RectF(rect.left + 6.0F, y, rect.right - 6.0F, y + kEntryHeight);
        if (index == selected_index) {
            dc->FillRoundedRectangle(D2D1::RoundedRect(row_rect, 4.0F, 4.0F), g_palette_resources.selection_brush.Get());
        }
        const PaletteEntry& entry = entries[index];
        dc->DrawTextW(
            entry.title.c_str(),
            static_cast<UINT32>(entry.title.size()),
            g_palette_resources.entry_format.Get(),
            D2D1::RectF(row_rect.left + 10.0F, row_rect.top + 8.0F, row_rect.right - 120.0F, row_rect.bottom),
            g_palette_resources.text_brush.Get()
        );
        dc->DrawTextW(
            entry.subtitle.c_str(),
            static_cast<UINT32>(entry.subtitle.size()),
            g_palette_resources.entry_format.Get(),
            D2D1::RectF(row_rect.right - 110.0F, row_rect.top + 8.0F, row_rect.right - 10.0F, row_rect.bottom),
            g_palette_resources.muted_brush.Get()
        );
    }
}

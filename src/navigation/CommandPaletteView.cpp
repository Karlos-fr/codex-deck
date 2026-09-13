// ============================================================================
// Codex Deck - Implementation de la vue Command Palette
// ----------------------------------------------------------------------------
// Ce fichier rend une palette compacte avec un champ d'edition lisible, un
// caret et une fenetre de resultats suivant la selection clavier.
// ============================================================================

#include "CommandPaletteView.h"

#include <wrl/client.h>

#include <algorithm>
#include <cwchar>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {

// Largeur maximale de la palette.
constexpr float kPaletteWidth = 680.0F;
// Hauteur de l'en-tete de mode.
constexpr float kHeaderHeight = 40.0F;
// Hauteur du champ de saisie.
constexpr float kQueryHeight = 52.0F;
// Hauteur d'une ligne de resultat.
constexpr float kEntryHeight = 42.0F;
// Nombre maximal de lignes rendues.
constexpr std::size_t kVisibleEntries = 8;
// Rayon des coins de la palette.
constexpr float kPaletteRadius = 8.0F;
// Marge interne horizontale.
constexpr float kPalettePaddingX = 12.0F;
// Marge entre le panneau et le champ de saisie.
constexpr float kPanelInset = 10.0F;
// Decalage du texte apres l'icone de recherche.
constexpr float kQueryTextInset = 42.0F;

// Retourne le premier resultat visible autour de la selection.
std::size_t FirstVisibleEntry(std::size_t entry_count, std::size_t selected_index) {
    if (entry_count <= kVisibleEntries || selected_index < kVisibleEntries) {
        return 0;
    }
    return std::min(selected_index - kVisibleEntries + 1, entry_count - kVisibleEntries);
}

// Calcule le rectangle centre de palette.
D2D1_RECT_F PaletteRect(const D2D1_RECT_F& bounds, std::size_t entry_count) {
    const float available_width = std::max(1.0F, bounds.right - bounds.left - 40.0F);
    const float width = std::min(kPaletteWidth, available_width);
    const std::size_t visible_count = std::min(entry_count, kVisibleEntries);
    const float results_height = visible_count == 0 ? 58.0F : static_cast<float>(visible_count) * kEntryHeight;
    const float height = kHeaderHeight + kQueryHeight + results_height + kPanelInset * 2.0F;
    const float left = bounds.left + ((bounds.right - bounds.left) - width) * 0.5F;
    const float preferred_top = bounds.top + 70.0F;
    const float top = std::max(bounds.top + 16.0F, std::min(preferred_top, bounds.bottom - height - 16.0F));
    return D2D1::RectF(left, top, left + width, top + height);
}

}  // namespace

// Ressources graphiques opaques partagees par la vue.
struct CommandPaletteResources {
    // Factory DirectWrite partagee.
    ComPtr<IDWriteFactory> dwrite_factory;
    // Format du libelle de mode.
    ComPtr<IDWriteTextFormat> header_format;
    // Format de requete.
    ComPtr<IDWriteTextFormat> query_format;
    // Format de resultat.
    ComPtr<IDWriteTextFormat> entry_format;
    // Format des metadonnees.
    ComPtr<IDWriteTextFormat> metadata_format;
    // Brosse d'assombrissement de l'arriere-plan.
    ComPtr<ID2D1SolidColorBrush> scrim_brush;
    // Brosse de fond du panneau.
    ComPtr<ID2D1SolidColorBrush> panel_brush;
    // Brosse du champ de saisie.
    ComPtr<ID2D1SolidColorBrush> input_brush;
    // Brosse de selection.
    ComPtr<ID2D1SolidColorBrush> selection_brush;
    // Brosse de bordure.
    ComPtr<ID2D1SolidColorBrush> border_brush;
    // Brosse de texte.
    ComPtr<ID2D1SolidColorBrush> text_brush;
    // Brosse de texte secondaire.
    ComPtr<ID2D1SolidColorBrush> muted_brush;
    // Brosse d'accent et de caret.
    ComPtr<ID2D1SolidColorBrush> accent_brush;
};

namespace {
// Ressources partagees tant que la palette reste une vue stateless.
CommandPaletteResources g_palette_resources;

// Cree les formats de texte au premier rendu.
bool EnsureTextResources() {
    if (!g_palette_resources.dwrite_factory && FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(g_palette_resources.dwrite_factory.GetAddressOf())
        ))) {
        return false;
    }
    if (!g_palette_resources.header_format) {
        g_palette_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0F, L"",
            g_palette_resources.header_format.GetAddressOf()
        );
        g_palette_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 17.0F, L"",
            g_palette_resources.query_format.GetAddressOf()
        );
        g_palette_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0F, L"",
            g_palette_resources.entry_format.GetAddressOf()
        );
        g_palette_resources.dwrite_factory->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0F, L"",
            g_palette_resources.metadata_format.GetAddressOf()
        );
        if (g_palette_resources.query_format) {
            g_palette_resources.query_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        if (g_palette_resources.entry_format) {
            g_palette_resources.entry_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }
    return g_palette_resources.header_format && g_palette_resources.query_format
        && g_palette_resources.entry_format && g_palette_resources.metadata_format;
}

// Cree les brosses liees au contexte de dessin au premier rendu.
bool EnsureBrushResources(ID2D1DeviceContext* dc, const ThemePalette& palette) {
    if (!g_palette_resources.panel_brush) {
        dc->CreateSolidColorBrush(palette.window_background, g_palette_resources.scrim_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.surface, g_palette_resources.panel_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.window_background, g_palette_resources.input_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.surface_hover, g_palette_resources.selection_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.border, g_palette_resources.border_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.text, g_palette_resources.text_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.text_muted, g_palette_resources.muted_brush.GetAddressOf());
        dc->CreateSolidColorBrush(palette.accent, g_palette_resources.accent_brush.GetAddressOf());
    }
    return g_palette_resources.scrim_brush && g_palette_resources.panel_brush
        && g_palette_resources.input_brush && g_palette_resources.selection_brush
        && g_palette_resources.border_brush && g_palette_resources.text_brush
        && g_palette_resources.muted_brush && g_palette_resources.accent_brush;
}

}  // namespace

// Cree une vue sans ressources natives.
CommandPaletteView::CommandPaletteView() = default;

// Libere les ressources opaques.
CommandPaletteView::~CommandPaletteView() = default;

// Dessine l'overlay et les entrees visibles.
void CommandPaletteView::Render(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& bounds,
    std::wstring_view query,
    std::span<const PaletteEntry> entries,
    std::size_t selected_index,
    std::size_t cursor_index,
    CommandPaletteMode mode,
    bool caret_visible,
    const ThemePalette& palette
) {
    if (dc == nullptr || !EnsureTextResources() || !EnsureBrushResources(dc, palette)) {
        return;
    }

    g_palette_resources.scrim_brush->SetColor(palette.window_background);
    g_palette_resources.scrim_brush->SetOpacity(0.74F);
    g_palette_resources.panel_brush->SetColor(palette.surface);
    g_palette_resources.input_brush->SetColor(palette.window_background);
    g_palette_resources.selection_brush->SetColor(palette.surface_hover);
    g_palette_resources.border_brush->SetColor(palette.border);
    g_palette_resources.text_brush->SetColor(palette.text);
    g_palette_resources.muted_brush->SetColor(palette.text_muted);
    g_palette_resources.accent_brush->SetColor(palette.accent);

    dc->FillRectangle(bounds, g_palette_resources.scrim_brush.Get());
    const D2D1_RECT_F rect = PaletteRect(bounds, entries.size());
    const D2D1_ROUNDED_RECT panel = D2D1::RoundedRect(rect, kPaletteRadius, kPaletteRadius);
    dc->FillRoundedRectangle(panel, g_palette_resources.panel_brush.Get());
    dc->DrawRoundedRectangle(panel, g_palette_resources.border_brush.Get(), 1.0F);

    const wchar_t* header = mode == CommandPaletteMode::Search ? L"SEARCH" : L"COMMAND PALETTE";
    dc->DrawTextW(
        header, static_cast<UINT32>(std::wcslen(header)), g_palette_resources.header_format.Get(),
        D2D1::RectF(rect.left + 14.0F, rect.top + 11.0F, rect.right - 14.0F, rect.top + kHeaderHeight),
        g_palette_resources.muted_brush.Get()
    );

    const D2D1_RECT_F input_rect = D2D1::RectF(
        rect.left + kPanelInset, rect.top + kHeaderHeight,
        rect.right - kPanelInset, rect.top + kHeaderHeight + kQueryHeight - 4.0F
    );
    const D2D1_ROUNDED_RECT input = D2D1::RoundedRect(input_rect, 6.0F, 6.0F);
    dc->FillRoundedRectangle(input, g_palette_resources.input_brush.Get());
    dc->DrawRoundedRectangle(input, g_palette_resources.accent_brush.Get(), 1.25F);

    const D2D1_POINT_2F icon_center = D2D1::Point2F(input_rect.left + 19.0F, input_rect.top + 22.0F);
    dc->DrawEllipse(D2D1::Ellipse(icon_center, 5.5F, 5.5F), g_palette_resources.muted_brush.Get(), 1.5F);
    dc->DrawLine(
        D2D1::Point2F(icon_center.x + 4.0F, icon_center.y + 4.0F),
        D2D1::Point2F(icon_center.x + 8.5F, icon_center.y + 8.5F),
        g_palette_resources.muted_brush.Get(), 1.5F
    );

    const float query_left = input_rect.left + kQueryTextInset;
    const D2D1_RECT_F query_rect = D2D1::RectF(query_left, input_rect.top + 11.0F, input_rect.right - 12.0F, input_rect.bottom - 5.0F);
    const std::wstring placeholder = mode == CommandPaletteMode::Search
        ? L"Search sessions and projects"
        : L"Type a command or session";
    const std::wstring displayed_query = query.empty() ? placeholder : std::wstring(query);
    dc->PushAxisAlignedClip(query_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    dc->DrawTextW(
        displayed_query.c_str(), static_cast<UINT32>(displayed_query.size()),
        g_palette_resources.query_format.Get(), query_rect,
        query.empty() ? g_palette_resources.muted_brush.Get() : g_palette_resources.text_brush.Get()
    );

    if (caret_visible) {
        float caret_x = query_left;
        if (!query.empty()) {
            ComPtr<IDWriteTextLayout> layout;
            if (SUCCEEDED(g_palette_resources.dwrite_factory->CreateTextLayout(
                    query.data(), static_cast<UINT32>(query.size()), g_palette_resources.query_format.Get(),
                    std::max(1.0F, query_rect.right - query_rect.left), kQueryHeight,
                    layout.GetAddressOf()
                ))) {
                FLOAT x = 0.0F;
                FLOAT y = 0.0F;
                DWRITE_HIT_TEST_METRICS metrics{};
                layout->HitTestTextPosition(
                    static_cast<UINT32>(std::min(cursor_index, query.size())), FALSE, &x, &y, &metrics
                );
                caret_x += x;
            }
        }
        dc->DrawLine(
            D2D1::Point2F(caret_x, input_rect.top + 11.0F),
            D2D1::Point2F(caret_x, input_rect.bottom - 10.0F),
            g_palette_resources.accent_brush.Get(), 1.5F
        );
    }
    dc->PopAxisAlignedClip();

    const float results_top = rect.top + kHeaderHeight + kQueryHeight + 2.0F;
    if (entries.empty()) {
        const wchar_t* empty_text = query.empty() && mode == CommandPaletteMode::Search
            ? L"Start typing to search"
            : L"No matching results";
        dc->DrawTextW(
            empty_text, static_cast<UINT32>(std::wcslen(empty_text)), g_palette_resources.entry_format.Get(),
            D2D1::RectF(rect.left + 18.0F, results_top + 16.0F, rect.right - 18.0F, rect.bottom),
            g_palette_resources.muted_brush.Get()
        );
        return;
    }

    const std::size_t first = FirstVisibleEntry(entries.size(), selected_index);
    const std::size_t last = std::min(entries.size(), first + kVisibleEntries);
    for (std::size_t index = first; index < last; ++index) {
        const float y = results_top + static_cast<float>(index - first) * kEntryHeight;
        const D2D1_RECT_F row_rect = D2D1::RectF(rect.left + 8.0F, y, rect.right - 8.0F, y + kEntryHeight - 2.0F);
        if (index == selected_index) {
            dc->FillRoundedRectangle(D2D1::RoundedRect(row_rect, 6.0F, 6.0F), g_palette_resources.selection_brush.Get());
        }

        const PaletteEntry& entry = entries[index];
        const D2D1_RECT_F title_rect = D2D1::RectF(row_rect.left + kPalettePaddingX, row_rect.top + 10.0F, row_rect.right - 118.0F, row_rect.bottom);
        dc->PushAxisAlignedClip(title_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        dc->DrawTextW(
            entry.title.c_str(), static_cast<UINT32>(entry.title.size()), g_palette_resources.entry_format.Get(),
            title_rect, g_palette_resources.text_brush.Get()
        );
        dc->PopAxisAlignedClip();
        dc->DrawTextW(
            entry.subtitle.c_str(), static_cast<UINT32>(entry.subtitle.size()), g_palette_resources.metadata_format.Get(),
            D2D1::RectF(row_rect.right - 106.0F, row_rect.top + 13.0F, row_rect.right - 12.0F, row_rect.bottom),
            g_palette_resources.muted_brush.Get()
        );
    }
}

// Retourne l'entree touchee dans la fenetre actuellement visible.
std::optional<std::size_t> CommandPaletteView::HitTestEntry(
    const D2D1_RECT_F& bounds,
    std::size_t entry_count,
    std::size_t selected_index,
    float x,
    float y
) const {
    if (entry_count == 0) {
        return std::nullopt;
    }
    const D2D1_RECT_F rect = PaletteRect(bounds, entry_count);
    const float results_top = rect.top + kHeaderHeight + kQueryHeight + 2.0F;
    if (x < rect.left + 8.0F || x > rect.right - 8.0F || y < results_top) {
        return std::nullopt;
    }
    const std::size_t visible_index = static_cast<std::size_t>((y - results_top) / kEntryHeight);
    if (visible_index >= kVisibleEntries) {
        return std::nullopt;
    }
    const std::size_t index = FirstVisibleEntry(entry_count, selected_index) + visible_index;
    return index < entry_count ? std::optional<std::size_t>{index} : std::nullopt;
}

// Indique si un point touche le champ de saisie de la palette.
bool CommandPaletteView::IsPointOnQuery(
    const D2D1_RECT_F& bounds,
    std::size_t entry_count,
    float x,
    float y
) const {
    const D2D1_RECT_F rect = PaletteRect(bounds, entry_count);
    const D2D1_RECT_F input = D2D1::RectF(
        rect.left + kPanelInset, rect.top + kHeaderHeight,
        rect.right - kPanelInset, rect.top + kHeaderHeight + kQueryHeight - 4.0F
    );
    return x >= input.left && x <= input.right && y >= input.top && y <= input.bottom;
}

// Indique si un point appartient au panneau de palette.
bool CommandPaletteView::Contains(
    const D2D1_RECT_F& bounds,
    std::size_t entry_count,
    float x,
    float y
) const {
    const D2D1_RECT_F rect = PaletteRect(bounds, entry_count);
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

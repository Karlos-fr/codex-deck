// ============================================================================
// Codex Deck - Implementation du rendu DirectComposition minimal
// ----------------------------------------------------------------------------
// Ce fichier dessine la coquille visuelle avec Direct2D sur la surface fournie
// par CompositionHost. Il ne gere ni fenetre, ni protocole Codex, ni stockage.
// ============================================================================

#include "DeckRenderer.h"

#include "../navigation/ProjectTreeModel.h"
#include "../navigation/TreeHitTesting.h"
#include "../navigation/ProjectTreeView.h"
#include "../ui/ScrollState.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <string>

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

    // Vue Tree virtualisee.
    ProjectTreeView tree_view;

    // Lignes synthetiques de debug pour valider le rendu volumineux.
    std::vector<TreeRow> debug_tree_rows;

    // Catalogue synthetique conserve pour reconstruire les lignes.
    SessionCatalogSnapshot debug_catalog;

    // Etat de scroll synthetique.
    ScrollState tree_scroll;

    // Etat de deploiement synthetique.
    ProjectTreeState tree_state;

    // Ligne selectionnee au clavier.
    std::size_t selected_tree_row = 0;

    // ------------------------------------------------------------------------
    // Reconstruit les lignes synthetiques depuis l'etat courant.
    // ------------------------------------------------------------------------
    void RebuildDebugRows();
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

// Largeur initiale du Tree en DIPs.
constexpr float kTreeDebugWidth = 300.0F;

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

// ----------------------------------------------------------------------------
// Cree un catalogue synthetique volumineux pour le mode debug interne.
//
// Retour :
// - catalogue de test.
// ----------------------------------------------------------------------------
SessionCatalogSnapshot BuildDebugCatalog() {
    SessionCatalogSnapshot catalog{};
    constexpr int kProjectCount = 500;
    constexpr int kSessionCount = 10'000;

    for (int project_index = 0; project_index < kProjectCount; ++project_index) {
        Project project{};
        project.id = project_index + 1;
        project.name = "Project " + std::to_string(project_index + 1);
        catalog.projects.push_back(std::move(project));
    }
    for (int session_index = 0; session_index < kSessionCount; ++session_index) {
        SessionRecord session{};
        session.codex.id = "debug-" + std::to_string(session_index + 1);
        session.codex.name = "Session " + std::to_string(session_index + 1);
        session.codex.cwd = "D:\\Debug\\Project" + std::to_string((session_index % kProjectCount) + 1);
        session.codex.updated_at = kSessionCount - session_index;
        session.project_id = (session_index % kProjectCount) + 1;
        session.status = session_index % 11 == 0 ? SessionStatus::Working : SessionStatus::Idle;
        catalog.sessions.push_back(std::move(session));
    }
    return catalog;
}

// ----------------------------------------------------------------------------
// Execute l'action principale d'une ligne.
//
// Parametres :
// - state : etat du Tree.
// - row : ligne active.
// ----------------------------------------------------------------------------
void ActivateTreeRow(ProjectTreeState& state, const TreeRow& row) {
    if (row.kind == TreeRowKind::Project && row.project_id) {
        if (state.expanded_projects.contains(*row.project_id)) {
            state.expanded_projects.erase(*row.project_id);
        } else {
            state.expanded_projects.insert(*row.project_id);
        }
    } else if (row.kind == TreeRowKind::UnassignedHeader) {
        state.unassigned_expanded = !state.unassigned_expanded;
    } else if (row.kind == TreeRowKind::Session && row.thread_id) {
        state.selected_thread = row.thread_id;
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// Reconstruit les lignes synthetiques depuis l'etat courant.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildDebugRows() {
    debug_tree_rows = BuildProjectTreeRows(debug_catalog, tree_state);
    if (!debug_tree_rows.empty() && selected_tree_row >= debug_tree_rows.size()) {
        selected_tree_row = debug_tree_rows.size() - 1;
    }
}

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
    if (impl_->debug_tree_rows.empty()) {
        constexpr int kProjectCount = 500;
        for (int project_index = 0; project_index < kProjectCount; ++project_index) {
            impl_->tree_state.expanded_projects.insert(project_index + 1);
        }
        impl_->debug_catalog = BuildDebugCatalog();
        impl_->RebuildDebugRows();
    }
    impl_->background_brush->SetColor(palette.window_background);
    impl_->title_brush->SetColor(palette.text);
    impl_->subtitle_brush->SetColor(palette.text_muted);
    context->Clear(palette.window_background);
    context->FillRectangle(D2D1::RectF(0.0F, 0.0F, size.width, size.height), impl_->background_brush.Get());
    impl_->tree_scroll.viewport_extent = size.height;
    impl_->tree_scroll.content_extent = static_cast<float>(impl_->debug_tree_rows.size()) * impl_->tree_view.RowHeight();
    impl_->tree_scroll.Clamp();
    impl_->tree_view.Render(
        context,
        D2D1::RectF(0.0F, 0.0F, std::min(kTreeDebugWidth, size.width), size.height),
        impl_->debug_tree_rows,
        impl_->tree_scroll,
        palette
    );
    context->DrawTextW(
        state.title.c_str(),
        static_cast<UINT32>(state.title.size()),
        impl_->title_format.Get(),
        D2D1::RectF(kTreeDebugWidth + kDeckContentLeft, kDeckContentTop, size.width - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight),
        impl_->title_brush.Get()
    );
    context->DrawTextW(
        state.subtitle.c_str(),
        static_cast<UINT32>(state.subtitle.size()),
        impl_->subtitle_format.Get(),
        D2D1::RectF(kTreeDebugWidth + kDeckContentLeft, kDeckContentTop + kDeckTitleHeight, size.width - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight + kDeckSubtitleHeight),
        impl_->subtitle_brush.Get()
    );
    if (!impl_->composition.EndDraw().has_value()) {
        DiscardDeviceResources();
    }
}

// ----------------------------------------------------------------------------
// Traite une molette verticale pour la zone Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnMouseWheel(int delta) {
    if (impl_ == nullptr) {
        return;
    }
    constexpr float kWheelLineHeight = 30.0F;
    constexpr float kWheelLinesPerNotch = 3.0F;
    impl_->tree_scroll.ScrollBy(-static_cast<float>(delta) / WHEEL_DELTA * kWheelLineHeight * kWheelLinesPerNotch);
}

// ----------------------------------------------------------------------------
// Traite un clic pointeur pour la zone Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerDown(float x, float y) {
    if (impl_ == nullptr || impl_->debug_tree_rows.empty()) {
        return;
    }
    const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, 0.0F, kTreeDebugWidth, impl_->tree_scroll.viewport_extent);
    const auto hit = HitTestTreeRow(D2D1::Point2F(x, y), tree_bounds, impl_->tree_scroll, impl_->tree_view.RowHeight(), impl_->debug_tree_rows.size());
    if (!hit) {
        return;
    }
    impl_->selected_tree_row = *hit;
    impl_->tree_view.ActivateRow(impl_->debug_tree_rows[*hit]);
    ActivateTreeRow(impl_->tree_state, impl_->debug_tree_rows[*hit]);
    impl_->RebuildDebugRows();
}

// ----------------------------------------------------------------------------
// Traite une touche clavier de navigation Tree.
// ----------------------------------------------------------------------------
bool DeckRenderer::OnKeyDown(WPARAM virtual_key) {
    if (impl_ == nullptr || impl_->debug_tree_rows.empty()) {
        return false;
    }

    switch (virtual_key) {
    case VK_UP:
        if (impl_->selected_tree_row > 0) {
            --impl_->selected_tree_row;
            impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        }
        return true;
    case VK_DOWN:
        if (impl_->selected_tree_row + 1 < impl_->debug_tree_rows.size()) {
            ++impl_->selected_tree_row;
            impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        }
        return true;
    case VK_HOME:
        impl_->selected_tree_row = 0;
        impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        return true;
    case VK_END:
        impl_->selected_tree_row = impl_->debug_tree_rows.empty() ? 0 : impl_->debug_tree_rows.size() - 1;
        impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        return true;
    case VK_RETURN:
    case VK_LEFT:
    case VK_RIGHT:
        impl_->tree_view.ActivateRow(impl_->debug_tree_rows[impl_->selected_tree_row]);
        ActivateTreeRow(impl_->tree_state, impl_->debug_tree_rows[impl_->selected_tree_row]);
        impl_->RebuildDebugRows();
        return true;
    default:
        return false;
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

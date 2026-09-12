// ============================================================================
// Codex Deck - Implementation de la vue Tree
// ----------------------------------------------------------------------------
// Ce fichier rend uniquement les lignes visibles d'un arbre aplati et garde un
// petit cache de layouts DirectWrite borne en memoire.
// ============================================================================

#include "ProjectTreeView.h"

#include "../ui/VirtualListLayout.h"

#include <wrl/client.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>

using Microsoft::WRL::ComPtr;

namespace {

// Hauteur fixe d'une ligne Tree.
constexpr float kTreeRowHeight = 30.0F;

// Marge horizontale interne.
constexpr float kTreePaddingX = 12.0F;

// Largeur d'une indentation.
constexpr float kTreeIndent = 18.0F;

// Taille du point de statut.
constexpr float kStatusDotRadius = 3.5F;

// Taille maximale du cache de layouts texte.
constexpr std::size_t kLayoutCacheLimit = 256;

// Revision de theme initiale pour le cache de layouts.
constexpr std::uint64_t kThemeRevision = 1;

// ----------------------------------------------------------------------------
// Cle de cache de layout texte.
// ----------------------------------------------------------------------------
struct LayoutKey {
    // Identifiant stable de ligne.
    std::string stable_id;

    // Largeur entiere disponible.
    int width = 0;

    // Revision de theme.
    std::uint64_t theme_revision = 0;

    // Revision de texte.
    std::uint64_t text_revision = 0;

    // ------------------------------------------------------------------------
    // Compare deux cles pour stockage ordonne.
    //
    // Parametres :
    // - other : autre cle.
    //
    // Retour :
    // - true si cette cle precede l'autre.
    // ------------------------------------------------------------------------
    bool operator<(const LayoutKey& other) const {
        return std::tie(stable_id, width, theme_revision, text_revision)
            < std::tie(other.stable_id, other.width, other.theme_revision, other.text_revision);
    }

    // ------------------------------------------------------------------------
    // Compare deux cles pour le LRU.
    //
    // Parametres :
    // - other : autre cle.
    //
    // Retour :
    // - true si les cles sont identiques.
    // ------------------------------------------------------------------------
    bool operator==(const LayoutKey& other) const {
        return stable_id == other.stable_id
            && width == other.width
            && theme_revision == other.theme_revision
            && text_revision == other.text_revision;
    }
};

// ----------------------------------------------------------------------------
// Retourne une couleur de statut.
//
// Parametres :
// - status : statut de session.
// - palette : palette courante.
//
// Retour :
// - couleur semantique.
// ----------------------------------------------------------------------------
D2D1_COLOR_F StatusColor(SessionStatus status, const ThemePalette& palette) {
    switch (status) {
    case SessionStatus::Working:
        return palette.accent;
    case SessionStatus::NeedsAttention:
        return palette.warning;
    case SessionStatus::Completed:
        return palette.success;
    case SessionStatus::Error:
        return palette.error;
    case SessionStatus::Idle:
    default:
        return palette.text_muted;
    }
}

// ----------------------------------------------------------------------------
// Retourne un entier borne depuis une largeur.
//
// Parametres :
// - width : largeur en DIPs.
//
// Retour :
// - largeur entiere non negative.
// ----------------------------------------------------------------------------
int CacheWidth(float width) {
    return std::max(0, static_cast<int>(width));
}

}  // namespace

struct ProjectTreeView::Impl {
    // Factory DirectWrite partagee.
    ComPtr<IDWriteFactory> dwrite_factory;

    // Format principal.
    ComPtr<IDWriteTextFormat> primary_format;

    // Format secondaire.
    ComPtr<IDWriteTextFormat> secondary_format;

    // Brosse de fond.
    ComPtr<ID2D1SolidColorBrush> surface_brush;

    // Brosse de survol/selection.
    ComPtr<ID2D1SolidColorBrush> selection_brush;

    // Brosse de texte principal.
    ComPtr<ID2D1SolidColorBrush> text_brush;

    // Brosse de texte secondaire.
    ComPtr<ID2D1SolidColorBrush> muted_brush;

    // Brosse de statut.
    ComPtr<ID2D1SolidColorBrush> status_brush;

    // Ordre LRU du cache.
    std::list<LayoutKey> lru;

    // Cache de layouts texte.
    std::map<LayoutKey, ComPtr<IDWriteTextLayout>> layouts;

    // Callback de selection.
    SelectThreadHandler select_thread;

    // Callback de bascule projet.
    ToggleProjectHandler toggle_project;

    // Callback More.
    OpenMoreHandler open_more;

    // ------------------------------------------------------------------------
    // Initialise les ressources partagees.
    //
    // Parametres :
    // - dc : contexte Direct2D.
    //
    // Retour :
    // - true si les ressources sont pretes.
    // ------------------------------------------------------------------------
    bool EnsureResources(ID2D1DeviceContext* dc) {
        if (!dwrite_factory && FAILED(DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwrite_factory.GetAddressOf())
        ))) {
            return false;
        }
        if (!primary_format) {
            dwrite_factory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                13.0F,
                L"",
                primary_format.GetAddressOf()
            );
            dwrite_factory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                11.0F,
                L"",
                secondary_format.GetAddressOf()
            );
        }
        if (!surface_brush) {
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), surface_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), selection_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), text_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), muted_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), status_brush.GetAddressOf());
        }
        return primary_format && secondary_format && surface_brush && selection_brush && text_brush && muted_brush && status_brush;
    }

    // ------------------------------------------------------------------------
    // Retourne un layout depuis le cache borne.
    //
    // Parametres :
    // - row : ligne source.
    // - width : largeur disponible.
    //
    // Retour :
    // - layout DirectWrite ou nullptr.
    // ------------------------------------------------------------------------
    IDWriteTextLayout* LayoutFor(const TreeRow& row, float width) {
        const LayoutKey key{row.stable_id, CacheWidth(width), kThemeRevision, 0};
        if (auto found = layouts.find(key); found != layouts.end()) {
            lru.remove(key);
            lru.push_front(key);
            return found->second.Get();
        }

        ComPtr<IDWriteTextLayout> layout;
        dwrite_factory->CreateTextLayout(
            row.primary_text.c_str(),
            static_cast<UINT32>(row.primary_text.size()),
            primary_format.Get(),
            std::max(1.0F, width),
            kTreeRowHeight,
            layout.GetAddressOf()
        );
        if (!layout) {
            return nullptr;
        }

        lru.push_front(key);
        layouts.emplace(key, layout);
        while (layouts.size() > kLayoutCacheLimit && !lru.empty()) {
            layouts.erase(lru.back());
            lru.pop_back();
        }
        return layout.Get();
    }
};

// ----------------------------------------------------------------------------
// Cree une vue sans ressources.
// ----------------------------------------------------------------------------
ProjectTreeView::ProjectTreeView()
    : impl_(std::make_unique<Impl>()) {
}

// ----------------------------------------------------------------------------
// Libere les ressources opaques.
// ----------------------------------------------------------------------------
ProjectTreeView::~ProjectTreeView() = default;

// ----------------------------------------------------------------------------
// Installe les callbacks d'interaction.
// ----------------------------------------------------------------------------
void ProjectTreeView::SetHandlers(
    SelectThreadHandler select_thread,
    ToggleProjectHandler toggle_project,
    OpenMoreHandler open_more
) {
    impl_->select_thread = std::move(select_thread);
    impl_->toggle_project = std::move(toggle_project);
    impl_->open_more = std::move(open_more);
}

// ----------------------------------------------------------------------------
// Dessine les lignes visibles.
// ----------------------------------------------------------------------------
void ProjectTreeView::Render(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& bounds,
    std::span<const TreeRow> rows,
    const ScrollState& scroll,
    const ThemePalette& palette
) {
    if (dc == nullptr || !impl_->EnsureResources(dc)) {
        return;
    }

    impl_->surface_brush->SetColor(palette.surface);
    impl_->selection_brush->SetColor(palette.surface_hover);
    impl_->text_brush->SetColor(palette.text);
    impl_->muted_brush->SetColor(palette.text_muted);
    dc->FillRectangle(bounds, impl_->surface_brush.Get());

    const VisibleRange range = ComputeVisibleRange(rows.size(), kTreeRowHeight, scroll.offset, bounds.bottom - bounds.top, 2);
    for (std::size_t index = range.first; index < range.last; ++index) {
        const TreeRow& row = rows[index];
        const float y = bounds.top + static_cast<float>(index) * kTreeRowHeight - scroll.offset;
        const D2D1_RECT_F row_rect = D2D1::RectF(bounds.left, y, bounds.right, y + kTreeRowHeight);
        const float x = bounds.left + kTreePaddingX + static_cast<float>(row.depth) * kTreeIndent;
        if (row.kind == TreeRowKind::Project) {
            dc->DrawTextW(L">", 1, impl_->primary_format.Get(), D2D1::RectF(x, row_rect.top + 7.0F, x + 12.0F, row_rect.bottom), impl_->muted_brush.Get());
        }
        if (row.kind == TreeRowKind::Session) {
            impl_->status_brush->SetColor(StatusColor(row.status, palette));
            dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + kStatusDotRadius, row_rect.top + 15.0F), kStatusDotRadius, kStatusDotRadius), impl_->status_brush.Get());
        }

        const float text_left = row.kind == TreeRowKind::Session ? x + 14.0F : x + 18.0F;
        if (IDWriteTextLayout* layout = impl_->LayoutFor(row, bounds.right - text_left - kTreePaddingX)) {
            dc->DrawTextLayout(D2D1::Point2F(text_left, row_rect.top + 6.0F), layout, impl_->text_brush.Get());
        }
    }
}

// ----------------------------------------------------------------------------
// Active une ligne et appelle le callback correspondant.
// ----------------------------------------------------------------------------
void ProjectTreeView::ActivateRow(const TreeRow& row) {
    if (row.kind == TreeRowKind::Session && row.thread_id && impl_->select_thread) {
        impl_->select_thread(*row.thread_id);
    } else if (row.kind == TreeRowKind::Project && row.project_id && impl_->toggle_project) {
        impl_->toggle_project(*row.project_id);
    } else if (row.kind == TreeRowKind::More && row.project_id && impl_->open_more) {
        impl_->open_more(*row.project_id);
    }
}

// ----------------------------------------------------------------------------
// Retourne la hauteur fixe de ligne.
// ----------------------------------------------------------------------------
float ProjectTreeView::RowHeight() const {
    return kTreeRowHeight;
}

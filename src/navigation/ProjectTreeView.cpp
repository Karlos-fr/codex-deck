// ============================================================================
// Codex Deck - Implementation de la vue Tree
// ----------------------------------------------------------------------------
// Ce fichier rend uniquement les lignes visibles d'un arbre aplati et garde un
// petit cache de layouts DirectWrite borne en memoire.
// ============================================================================

#include "ProjectTreeView.h"

#include "../ui/ScrollbarGeometry.h"
#include "../ui/TreeVisualAnimation.h"
#include "../ui/VirtualListLayout.h"

#include <wrl/client.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <functional>

using Microsoft::WRL::ComPtr;

namespace {

// Hauteur fixe d'une ligne Tree.
constexpr float kTreeRowHeight = 30.0F;

// Marge horizontale interne.
constexpr float kTreePaddingX = 8.0F;

// Largeur minimale de scrollbar recue en entree.
constexpr float kMinimumScrollbarWidth = 4.0F;

// Largeur d'une indentation.
constexpr float kTreeIndent = 16.0F;

// Taille du point de statut.
constexpr float kStatusDotRadius = 2.6F;

// Decalage vertical du layout DirectWrite dans la ligne centree.
constexpr float kTextTopPadding = 0.0F;

// Marge horizontale de la selection active.
constexpr float kSelectionInsetX = 6.0F;

// Marge verticale de la selection active.
constexpr float kSelectionInsetY = 3.0F;

// Decalage optique du fond pour laisser respirer les jambages inferieurs.
constexpr float kSelectionOffsetY = 4.0F;

// Rayon de la selection active.
constexpr float kSelectionRadius = 6.0F;

// Largeur du fondu fixe sur les titres tronques.
constexpr float kTitleFadeWidth = 28.0F;

// Hauteur du fondu vertical aux bords du Tree.
constexpr float kVerticalFadeHeight = 18.0F;

// Hauteur minimale du pouce de scrollbar.
constexpr float kMinimumScrollbarThumbHeight = 32.0F;

// Taille maximale du cache de layouts texte.
constexpr std::size_t kLayoutCacheLimit = 256;

// Largeur de mesure qui evite la troncature des titres usuels.
constexpr float kNaturalTextLayoutWidth = 16384.0F;

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

// ----------------------------------------------------------------------------
// Calcule une revision stable a partir du texte de ligne.
//
// Parametres :
// - text : texte source.
//
// Retour :
// - revision non cryptographique pour invalider le layout.
// ----------------------------------------------------------------------------
std::uint64_t TextRevision(std::wstring_view text) {
    return static_cast<std::uint64_t>(std::hash<std::wstring_view>{}(text));
}

// ----------------------------------------------------------------------------
// Retourne une couleur avec une opacite remplacee.
//
// Parametres :
// - color : couleur source.
// - alpha : nouvelle opacite.
//
// Retour :
// - couleur avec alpha ajuste.
// ----------------------------------------------------------------------------
D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha) {
    color.a = alpha;
    return color;
}

// ----------------------------------------------------------------------------
// Dessine un fondu horizontal a droite d'un titre tronque.
//
// Parametres :
// - dc : contexte Direct2D cible.
// - fade_rect : rectangle du fondu.
// - base_color : couleur de fond a retrouver en bout de fondu.
// ----------------------------------------------------------------------------
void DrawRightFade(ID2D1DeviceContext* dc, const D2D1_RECT_F& fade_rect, D2D1_COLOR_F base_color) {
    if (fade_rect.right <= fade_rect.left || dc == nullptr) {
        return;
    }
    D2D1_GRADIENT_STOP stops[2] = {
        D2D1::GradientStop(0.0F, WithAlpha(base_color, 0.0F)),
        D2D1::GradientStop(1.0F, WithAlpha(base_color, base_color.a))
    };
    ComPtr<ID2D1GradientStopCollection> stop_collection;
    dc->CreateGradientStopCollection(stops, 2, stop_collection.GetAddressOf());
    if (!stop_collection) {
        return;
    }
    ComPtr<ID2D1LinearGradientBrush> fade_brush;
    dc->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(fade_rect.left, fade_rect.top),
            D2D1::Point2F(fade_rect.right, fade_rect.top)
        ),
        stop_collection.Get(),
        fade_brush.GetAddressOf()
    );
    if (fade_brush) {
        dc->FillRectangle(fade_rect, fade_brush.Get());
    }
}

// ----------------------------------------------------------------------------
// Dessine un fondu vertical sur un bord du Tree.
//
// Parametres :
// - dc : contexte Direct2D cible.
// - fade_rect : rectangle du fondu.
// - base_color : couleur de fond a retrouver sur le bord.
// - top_edge : true pour un fondu haut, false pour un fondu bas.
// ----------------------------------------------------------------------------
void DrawVerticalFade(ID2D1DeviceContext* dc, const D2D1_RECT_F& fade_rect, D2D1_COLOR_F base_color, bool top_edge) {
    if (fade_rect.bottom <= fade_rect.top || dc == nullptr) {
        return;
    }
    D2D1_GRADIENT_STOP stops[2] = {
        D2D1::GradientStop(0.0F, top_edge ? WithAlpha(base_color, base_color.a) : WithAlpha(base_color, 0.0F)),
        D2D1::GradientStop(1.0F, top_edge ? WithAlpha(base_color, 0.0F) : WithAlpha(base_color, base_color.a))
    };
    ComPtr<ID2D1GradientStopCollection> stop_collection;
    dc->CreateGradientStopCollection(stops, 2, stop_collection.GetAddressOf());
    if (!stop_collection) {
        return;
    }
    ComPtr<ID2D1LinearGradientBrush> fade_brush;
    dc->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(fade_rect.left, fade_rect.top),
            D2D1::Point2F(fade_rect.left, fade_rect.bottom)
        ),
        stop_collection.Get(),
        fade_brush.GetAddressOf()
    );
    if (fade_brush) {
        dc->FillRectangle(fade_rect, fade_brush.Get());
    }
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

    // Brosse de bordure.
    ComPtr<ID2D1SolidColorBrush> border_brush;

    // Brosse de scrollbar.
    ComPtr<ID2D1SolidColorBrush> scrollbar_brush;

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
                L"Segoe UI Variable Text",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                18.125F,
                L"",
                primary_format.GetAddressOf()
            );
            dwrite_factory->CreateTextFormat(
                L"Segoe UI Variable Text",
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0F,
                L"",
                secondary_format.GetAddressOf()
            );
            primary_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            secondary_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            primary_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            secondary_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (!surface_brush) {
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), surface_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), selection_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), text_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), muted_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), status_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), border_brush.GetAddressOf());
            dc->CreateSolidColorBrush(D2D1::ColorF(0, 0), scrollbar_brush.GetAddressOf());
        }
        return primary_format && secondary_format && surface_brush && selection_brush && text_brush && muted_brush && status_brush && border_brush && scrollbar_brush;
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
        const LayoutKey key{row.stable_id, CacheWidth(width), kThemeRevision, TextRevision(row.primary_text)};
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
        DWRITE_TRIMMING trimming{};
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        layout->SetTrimming(&trimming, nullptr);

        lru.push_front(key);
        layouts.emplace(key, layout);
        while (layouts.size() > kLayoutCacheLimit && !lru.empty()) {
            layouts.erase(lru.back());
            lru.pop_back();
        }
        return layout.Get();
    }

    // ------------------------------------------------------------------------
    // Mesure la largeur naturelle d'un titre sans troncature visuelle.
    //
    // Parametres :
    // - row : ligne dont le titre doit etre mesure.
    //
    // Retour :
    // - largeur naturelle du texte.
    // ------------------------------------------------------------------------
    float NaturalTextWidth(const TreeRow& row) {
        IDWriteTextLayout* layout = LayoutFor(row, kNaturalTextLayoutWidth);
        if (layout == nullptr) {
            return 0.0F;
        }
        DWRITE_TEXT_METRICS metrics{};
        layout->GetMetrics(&metrics);
        return metrics.widthIncludingTrailingWhitespace;
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
    const std::optional<CodexThreadId>& selected_thread,
    std::optional<std::size_t> selected_row,
    std::optional<std::size_t> hovered_row,
    float scrollbar_opacity,
    float scrollbar_width,
    std::map<CodexThreadId, MarqueeAnimationState>& title_marquee_animations,
    const ThemePalette& palette
) {
    if (dc == nullptr || !impl_->EnsureResources(dc)) {
        return;
    }

    impl_->surface_brush->SetColor(palette.surface);
    impl_->selection_brush->SetColor(palette.surface_hover);
    impl_->text_brush->SetColor(palette.text);
    impl_->muted_brush->SetColor(palette.text_muted);
    impl_->border_brush->SetColor(palette.border);
    impl_->scrollbar_brush->SetColor(palette.text_muted);
    dc->FillRectangle(bounds, impl_->surface_brush.Get());
    dc->PushAxisAlignedClip(bounds, D2D1_ANTIALIAS_MODE_ALIASED);

    const VisibleRange range = ComputeVisibleRange(rows.size(), kTreeRowHeight, scroll.offset, bounds.bottom - bounds.top, 2);
    for (std::size_t index = range.first; index < range.last; ++index) {
        const TreeRow& row = rows[index];
        const float y = bounds.top + static_cast<float>(index) * kTreeRowHeight - scroll.offset;
        const D2D1_RECT_F row_rect = D2D1::RectF(bounds.left, y, bounds.right, y + kTreeRowHeight);
        const float effective_scrollbar_width = std::max(kMinimumScrollbarWidth, scrollbar_width);
        const float x = bounds.left + kTreePaddingX + static_cast<float>(row.depth) * kTreeIndent;
        const bool selected = (selected_row && *selected_row == index)
            || (row.thread_id && selected_thread && *row.thread_id == *selected_thread);
        const bool hovered_session = hovered_row && *hovered_row == index && row.kind == TreeRowKind::Session;
        D2D1_RECT_F selection = D2D1::RectF(0.0F, 0.0F, 0.0F, 0.0F);
        if (selected) {
            selection = D2D1::RectF(
                bounds.left + kSelectionInsetX,
                row_rect.top + kSelectionInsetY + kSelectionOffsetY,
                bounds.right - kSelectionInsetX - effective_scrollbar_width,
                row_rect.bottom - kSelectionInsetY + kSelectionOffsetY
            );
            impl_->selection_brush->SetColor(palette.surface_hover);
            dc->FillRoundedRectangle(D2D1::RoundedRect(selection, kSelectionRadius, kSelectionRadius), impl_->selection_brush.Get());
        } else if (hovered_session) {
            const D2D1_RECT_F hover = D2D1::RectF(
                bounds.left + kSelectionInsetX,
                row_rect.top + kSelectionInsetY + kSelectionOffsetY,
                bounds.right - kSelectionInsetX - effective_scrollbar_width,
                row_rect.bottom - kSelectionInsetY + kSelectionOffsetY
            );
            impl_->selection_brush->SetColor(palette.surface_hover);
            impl_->selection_brush->SetOpacity(0.42F);
            dc->FillRoundedRectangle(D2D1::RoundedRect(hover, kSelectionRadius, kSelectionRadius), impl_->selection_brush.Get());
            impl_->selection_brush->SetOpacity(1.0F);
        }
        if (row.kind == TreeRowKind::Project || row.kind == TreeRowKind::UnassignedHeader) {
            const float cy = row_rect.top + kTreeRowHeight * 0.5F;
            impl_->muted_brush->SetColor(palette.text_muted);
            if (row.expanded) {
                dc->DrawLine(D2D1::Point2F(x + 1.0F, cy - 2.0F), D2D1::Point2F(x + 5.0F, cy + 2.0F), impl_->muted_brush.Get(), 1.25F);
                dc->DrawLine(D2D1::Point2F(x + 5.0F, cy + 2.0F), D2D1::Point2F(x + 9.0F, cy - 2.0F), impl_->muted_brush.Get(), 1.25F);
            } else {
                dc->DrawLine(D2D1::Point2F(x + 3.0F, cy - 5.0F), D2D1::Point2F(x + 8.0F, cy), impl_->muted_brush.Get(), 1.25F);
                dc->DrawLine(D2D1::Point2F(x + 8.0F, cy), D2D1::Point2F(x + 3.0F, cy + 5.0F), impl_->muted_brush.Get(), 1.25F);
            }
        }
        if (row.kind == TreeRowKind::Session) {
            if (row.status != SessionStatus::Idle) {
                impl_->status_brush->SetColor(StatusColor(row.status, palette));
                dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + kStatusDotRadius, row_rect.top + kTreeRowHeight * 0.5F), kStatusDotRadius, kStatusDotRadius), impl_->status_brush.Get());
            }
        }

        const float text_left = row.kind == TreeRowKind::Session ? x + 8.0F : x + 16.0F;
        const float text_right = selected
            ? std::max(text_left + 1.0F, selection.right - 6.0F)
            : bounds.right - kTreePaddingX - effective_scrollbar_width - 2.0F;
        const float text_width = std::max(1.0F, text_right - text_left);
        const float natural_text_width = impl_->NaturalTextWidth(row);
        const bool text_truncated = natural_text_width > text_width + 1.0F;
        const bool selected_session = row.kind == TreeRowKind::Session
            && row.thread_id
            && selected_thread
            && *row.thread_id == *selected_thread;
        auto marquee = row.thread_id ? title_marquee_animations.find(*row.thread_id) : title_marquee_animations.end();
        if (marquee != title_marquee_animations.end()) {
            marquee->second.maximum_offset = std::max(0.0F, natural_text_width - text_width);
        }
        const float marquee_offset_value = marquee != title_marquee_animations.end() ? marquee->second.visual.value : 0.0F;
        const bool marquee_active = text_truncated && (selected_session || hovered_session || marquee_offset_value > 0.0F);
        const float layout_width = ComputeMarqueeLayoutWidth(text_width, natural_text_width, marquee_active);
        if (IDWriteTextLayout* layout = impl_->LayoutFor(row, layout_width)) {
            const float marquee_offset = marquee_active
                ? ComputeMarqueeOffset(text_width, natural_text_width, marquee_offset_value)
                : 0.0F;
            const D2D1_RECT_F text_clip = D2D1::RectF(text_left, row_rect.top, text_right, row_rect.bottom);
            dc->PushAxisAlignedClip(text_clip, D2D1_ANTIALIAS_MODE_ALIASED);
            dc->DrawTextLayout(D2D1::Point2F(text_left - marquee_offset, row_rect.top + kTextTopPadding), layout, row.kind == TreeRowKind::More ? impl_->muted_brush.Get() : impl_->text_brush.Get());
            dc->PopAxisAlignedClip();
            if (text_truncated && !marquee_active) {
                DrawRightFade(
                    dc,
                    D2D1::RectF(std::max(text_left, text_right - kTitleFadeWidth), row_rect.top, text_right, row_rect.bottom),
                    palette.surface
                );
            }
        }
    }
    dc->PopAxisAlignedClip();

    if (scroll.offset > 0.5F) {
        DrawVerticalFade(
            dc,
            D2D1::RectF(bounds.left, bounds.top, bounds.right, std::min(bounds.bottom, bounds.top + kVerticalFadeHeight)),
            palette.surface,
            true
        );
    }
    if (scroll.offset + scroll.viewport_extent < scroll.content_extent - 0.5F) {
        DrawVerticalFade(
            dc,
            D2D1::RectF(bounds.left, std::max(bounds.top, bounds.bottom - kVerticalFadeHeight), bounds.right, bounds.bottom),
            palette.surface,
            false
        );
    }

    if (scrollbar_opacity > 0.01F && scroll.content_extent > scroll.viewport_extent && scroll.viewport_extent > 0.0F) {
        ScrollbarInput input{};
        input.top = bounds.top;
        input.bottom = bounds.bottom;
        input.right = bounds.right;
        input.content_extent = scroll.content_extent;
        input.viewport_extent = scroll.viewport_extent;
        input.scroll_offset = scroll.offset;
        input.width = std::max(kMinimumScrollbarWidth, scrollbar_width);
        input.minimum_thumb_height = kMinimumScrollbarThumbHeight;
        const ScrollbarMetrics metrics = ComputeVerticalScrollbar(input);
        const D2D1_RECT_F thumb = D2D1::RectF(metrics.left, metrics.thumb_top, metrics.right, metrics.thumb_bottom);
        impl_->scrollbar_brush->SetOpacity(std::clamp(scrollbar_opacity, 0.0F, 1.0F) * 0.55F);
        dc->FillRoundedRectangle(D2D1::RoundedRect(thumb, 3.0F, 3.0F), impl_->scrollbar_brush.Get());
        impl_->scrollbar_brush->SetOpacity(1.0F);
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

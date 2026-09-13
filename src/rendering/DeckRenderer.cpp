// ============================================================================
// Codex Deck - Implementation du rendu DirectComposition minimal
// ----------------------------------------------------------------------------
// Ce fichier dessine la coquille visuelle avec Direct2D sur la surface fournie
// par CompositionHost. Il ne gere ni fenetre, ni protocole Codex, ni stockage.
// ============================================================================

#include "DeckRenderer.h"

#include "../input/KeyboardShortcuts.h"
#include "../navigation/ActivityBarModel.h"
#include "../navigation/ActivityBarView.h"
#include "../navigation/CommandPaletteModel.h"
#include "../navigation/CommandPaletteView.h"
#include "../navigation/ProjectTreeModel.h"
#include "../navigation/TreeHitTesting.h"
#include "../navigation/ProjectTreeView.h"
#include "../navigation/TreeDragController.h"
#include "../ui/MainLayout.h"
#include "../ui/ScrollState.h"
#include "../ui/ScrollbarGeometry.h"
#include "../ui/TreeVisualAnimation.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <string>
#include <cwctype>

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

    // Format du titre Workbench.
    ComPtr<IDWriteTextFormat> workbench_title_format;

    // Vue Tree virtualisee.
    ProjectTreeView tree_view;

    // Vue de barre d'activite.
    ActivityBarView activity_bar_view;

    // Vue de Command Palette.
    CommandPaletteView command_palette_view;

    // Controleur de drag interne du Tree.
    TreeDragController tree_drag;

    // Lignes courantes construites pour le Tree.
    std::vector<TreeRow> tree_rows;

    // Catalogue courant conserve pour reconstruire les lignes.
    SessionCatalogSnapshot render_catalog;

    // Revision du catalogue deja integree.
    std::uint64_t render_catalog_revision = 0;

    // Etat de scroll du Tree.
    ScrollState tree_scroll;

    // Etat de deploiement du Tree.
    ProjectTreeState tree_state;

    // Filtre global courant.
    SessionFilter active_filter = SessionFilter::All;

    // Ligne selectionnee au clavier.
    std::size_t selected_tree_row = 0;

    // Indique si la selection de ligne doit etre visible.
    bool tree_row_focus_visible = false;

    // Indique si la Command Palette capture actuellement le clavier.
    bool command_palette_open = false;

    // Requete courante de la Command Palette.
    std::wstring command_palette_query;

    // Entrees scorees visibles dans la Command Palette.
    std::vector<PaletteEntry> command_palette_entries;

    // Entree de palette selectionnee.
    std::size_t command_palette_selection = 0;

    // Largeur courante du Tree.
    float tree_width = 300.0F;

    // Indique qu'un redimensionnement de Tree est en cours.
    bool resizing_tree = false;

    // Indique si le pointeur survole actuellement le Tree.
    bool tree_hovered = false;

    // Ligne de session actuellement survolee dans le Tree.
    std::optional<std::size_t> hovered_tree_row;

    // Opacite courante de la scrollbar Tree.
    float tree_scrollbar_opacity = 0.0F;

    // Largeur visuelle courante de la scrollbar Tree.
    float tree_scrollbar_width = 8.0F;

    // Indique si le pointeur survole le pouce plein de scrollbar Tree.
    bool tree_scrollbar_hovered = false;

    // Indique si le pouce de scrollbar Tree est glisse.
    bool dragging_tree_scrollbar = false;

    // Decalage pointeur -> haut du pouce pendant le drag.
    float tree_scrollbar_drag_grab_y = 0.0F;

    // Etats de marquee conserves pour les sessions selectionnees ou survolees.
    std::map<CodexThreadId, MarqueeAnimationState> title_marquee_animations;

    // Dernier instant utilise pour rendre les animations independantes des frames.
    std::chrono::steady_clock::time_point last_animation_tick = std::chrono::steady_clock::now();

    // Indique qu'un renommage inline synthetique est en cours.
    bool rename_active = false;

    // Thread renomme en cours.
    std::optional<CodexThreadId> rename_thread;

    // Titre avant edition pour restauration locale.
    std::string rename_original_title;

    // Titre edite localement.
    std::wstring rename_buffer;

    // ------------------------------------------------------------------------
    // Reconstruit les lignes depuis l'etat courant.
    // ------------------------------------------------------------------------
    void RebuildTreeRows();

    // ------------------------------------------------------------------------
    // Reconstruit les entrees de Command Palette depuis la requete courante.
    // ------------------------------------------------------------------------
    void RebuildCommandPalette();

    // ------------------------------------------------------------------------
    // Execute une commande applicative sur l'etat synthetique local.
    //
    // Parametres :
    // - command : type de commande a traiter.
    // ------------------------------------------------------------------------
    void ExecuteCommand(DeckCommandKind command);

    // ------------------------------------------------------------------------
    // Calcule les metriques de scrollbar Tree.
    //
    // Parametres :
    // - tree_bounds : rectangle courant du Tree.
    //
    // Retour :
    // - metriques de scrollbar.
    // ------------------------------------------------------------------------
    ScrollbarMetrics TreeScrollbarMetrics(const D2D1_RECT_F& tree_bounds) const;

};

namespace {

// Taille du texte du titre en DIPs.
constexpr float kDeckTitleTextSize = 34.0F;

// Taille du titre Workbench compact.
constexpr float kWorkbenchTitleTextSize = 28.0F;

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

// Hauteur compacte de la barre d'activite.
constexpr float kActivityBarHeight = 42.0F;

// Hauteur reservee au composer futur.
constexpr float kComposerPlaceholderHeight = 120.0F;

// Largeur de zone de hit du separateur.
constexpr float kSplitterHitWidth = 8.0F;

// Largeur fine de la scrollbar du Tree.
constexpr float kTreeScrollbarRestWidth = 8.0F;

// Largeur de survol de la scrollbar du Tree.
constexpr float kTreeScrollbarHoverWidth = 12.0F;

// Pas d'animation de largeur de scrollbar par frame.
constexpr float kScrollbarWidthStep = 0.7F;

// Duree caracteristique de l'acceleration et de la deceleration du marquee.
constexpr float kTitleMarqueeSmoothTimeSeconds = 0.55F;

// Projection de mouvement conservee pendant le freinage a la sortie.
constexpr float kTitleMarqueeCoastProjectionSeconds = 0.16F;

// Vitesse maximale fixe du marquee, independante de la longueur du titre.
constexpr float kTitleMarqueeMaximumSpeed = 44.0F;

// Duree maximale prise en compte pour eviter un saut apres un blocage UI.
constexpr float kMaximumAnimationDeltaSeconds = 0.05F;

// Pas de disparition de scrollbar par frame.
constexpr float kScrollbarFadeStep = 0.08F;

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
    } else if (row.kind == TreeRowKind::More && row.project_id) {
        const std::size_t current = state.visible_sessions_by_project.contains(*row.project_id)
            ? state.visible_sessions_by_project[*row.project_id]
            : 8;
        state.visible_sessions_by_project[*row.project_id] = current + std::max<std::size_t>(row.hidden_count, 24);
    }
}

// ----------------------------------------------------------------------------
// Bascule un filtre global.
//
// Parametres :
// - current : filtre courant.
// - requested : filtre demande.
//
// Retour :
// - All si le filtre etait deja actif, sinon le filtre demande.
// ----------------------------------------------------------------------------
SessionFilter ToggleFilter(SessionFilter current, SessionFilter requested) {
    return current == requested ? SessionFilter::All : requested;
}

// ----------------------------------------------------------------------------
// Traduit une position horizontale de barre en filtre.
//
// Parametres :
// - x : position horizontale en DIPs.
//
// Retour :
// - filtre clique.
// ----------------------------------------------------------------------------
std::optional<SessionFilter> ActivityFilterAt(float x) {
    if (x < 120.0F) {
        return SessionFilter::Working;
    }
    if (x < 250.0F) {
        return SessionFilter::NeedsAttention;
    }
    if (x < 430.0F) {
        return SessionFilter::CompletedToday;
    }
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Indique si Ctrl est actuellement enfonce.
//
// Retour :
// - true lorsque la touche Ctrl gauche ou droite est active.
// ----------------------------------------------------------------------------
bool IsControlDown() {
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

// ----------------------------------------------------------------------------
// Indique si Shift est actuellement enfonce.
//
// Retour :
// - true lorsque Shift est actif.
// ----------------------------------------------------------------------------
bool IsShiftDown() {
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}

// ----------------------------------------------------------------------------
// Convertit un rectangle de layout vers Direct2D.
//
// Parametres :
// - rect : rectangle generique.
//
// Retour :
// - rectangle Direct2D.
// ----------------------------------------------------------------------------
D2D1_RECT_F ToD2DRect(const LayoutRect& rect) {
    return D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom);
}

// ----------------------------------------------------------------------------
// Calcule si une position est sur la zone de redimensionnement.
//
// Parametres :
// - x : position horizontale.
// - tree_width : largeur courante.
//
// Retour :
// - true si le pointeur touche le separateur.
// ----------------------------------------------------------------------------
bool IsSplitterHit(float x, float tree_width) {
    return std::abs(x - tree_width) <= kSplitterHitWidth * 0.5F;
}

// ----------------------------------------------------------------------------
// Convertit un texte wide ASCII vers une chaine etroite.
//
// Parametres :
// - text : texte source.
//
// Retour :
// - texte ASCII de repli.
// ----------------------------------------------------------------------------
std::string NarrowAscii(std::wstring_view text) {
    std::string narrow;
    narrow.reserve(text.size());
    for (const wchar_t character : text) {
        narrow.push_back(character <= 0x7F ? static_cast<char>(character) : '?');
    }
    return narrow;
}

}  // namespace

// ----------------------------------------------------------------------------
// Reconstruit les lignes depuis l'etat courant.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildTreeRows() {
    SessionCatalogSnapshot visible_catalog = render_catalog;
    visible_catalog.sessions = FilterSessions(render_catalog.sessions, active_filter);
    tree_rows = BuildProjectTreeRows(visible_catalog, tree_state);
    if (!tree_rows.empty() && selected_tree_row >= tree_rows.size()) {
        selected_tree_row = tree_rows.size() - 1;
    }
}

// ----------------------------------------------------------------------------
// Reconstruit les entrees de Command Palette depuis la requete courante.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildCommandPalette() {
    command_palette_entries = BuildCommandPaletteEntries(render_catalog, command_palette_query);
    if (!command_palette_entries.empty() && command_palette_selection >= command_palette_entries.size()) {
        command_palette_selection = command_palette_entries.size() - 1;
    } else if (command_palette_entries.empty()) {
        command_palette_selection = 0;
    }
}

// ----------------------------------------------------------------------------
// Execute une commande applicative sur l'etat courant local.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::ExecuteCommand(DeckCommandKind command) {
    if (tree_rows.empty()) {
        return;
    }

    switch (command) {
    case DeckCommandKind::OpenCommandPalette:
    case DeckCommandKind::OpenSearch:
        command_palette_open = true;
        command_palette_query.clear();
        command_palette_selection = 0;
        RebuildCommandPalette();
        break;
    case DeckCommandKind::RenameThread: {
        const TreeRow& row = tree_rows[selected_tree_row];
        if (row.kind != TreeRowKind::Session || !row.thread_id) {
            break;
        }
        rename_active = true;
        rename_thread = row.thread_id;
        rename_original_title = NarrowAscii(row.primary_text);
        rename_buffer = row.primary_text;
        break;
    }
    case DeckCommandKind::ArchiveThread: {
        const TreeRow& row = tree_rows[selected_tree_row];
        if (row.kind != TreeRowKind::Session || !row.thread_id) {
            break;
        }
        for (SessionRecord& session : render_catalog.sessions) {
            if (session.codex.id == *row.thread_id) {
                session.codex.archived = true;
                break;
            }
        }
        RebuildTreeRows();
        break;
    }
    default:
        break;
    }
}

// ----------------------------------------------------------------------------
// Calcule les metriques de scrollbar Tree.
// ----------------------------------------------------------------------------
ScrollbarMetrics DeckRenderer::Impl::TreeScrollbarMetrics(const D2D1_RECT_F& tree_bounds) const {
    ScrollbarInput input{};
    input.top = tree_bounds.top;
    input.bottom = tree_bounds.bottom;
    input.right = tree_bounds.right;
    input.content_extent = tree_scroll.content_extent;
    input.viewport_extent = tree_scroll.viewport_extent;
    input.scroll_offset = tree_scroll.offset;
    input.width = std::max(kTreeScrollbarRestWidth, tree_scrollbar_width);
    input.minimum_thumb_height = 32.0F;
    return ComputeVerticalScrollbar(input);
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
            L"Segoe UI Variable Display",
            nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            kWorkbenchTitleTextSize,
            L"",
            impl_->workbench_title_format.GetAddressOf()
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
        DWRITE_TRIMMING trimming{};
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        impl_->title_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        impl_->title_format->SetTrimming(&trimming, nullptr);
        impl_->workbench_title_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        impl_->workbench_title_format->SetTrimming(&trimming, nullptr);
        impl_->subtitle_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        impl_->subtitle_format->SetTrimming(&trimming, nullptr);
    }
    return impl_->title_format && impl_->subtitle_format && impl_->workbench_title_format;
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
void DeckRenderer::Render(
    HWND hwnd,
    const DeckVisualState& state,
    const ThemePalette& palette,
    std::shared_ptr<const SessionCatalogSnapshot> catalog
) {
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
    const MainLayoutRects layout = ComputeMainLayout(
        SizeF{size.width, size.height},
        impl_->tree_width,
        kActivityBarHeight,
        kComposerPlaceholderHeight
    );
    impl_->tree_width = layout.tree.right - layout.tree.left;
    if (catalog && catalog->revision != impl_->render_catalog_revision) {
        impl_->render_catalog = *catalog;
        impl_->render_catalog_revision = catalog->revision;
        for (const Project& project : impl_->render_catalog.projects) {
            impl_->tree_state.expanded_projects.insert(project.id);
        }
        impl_->RebuildTreeRows();
    } else if (impl_->tree_rows.empty()) {
        impl_->RebuildTreeRows();
    }
    impl_->background_brush->SetColor(palette.window_background);
    impl_->title_brush->SetColor(palette.text);
    impl_->subtitle_brush->SetColor(palette.text_muted);
    context->Clear(palette.window_background);
    context->FillRectangle(D2D1::RectF(0.0F, 0.0F, size.width, size.height), impl_->background_brush.Get());
    impl_->tree_scroll.viewport_extent = std::max(0.0F, layout.tree.bottom - layout.tree.top);
    impl_->tree_scroll.content_extent = static_cast<float>(impl_->tree_rows.size()) * impl_->tree_view.RowHeight();
    impl_->tree_scroll.Clamp();
    const ActivityCounts counts = BuildActivityCounts(impl_->render_catalog.sessions);
    impl_->activity_bar_view.Render(
        context,
        ToD2DRect(layout.activity_bar),
        counts,
        impl_->active_filter,
        palette
    );
    impl_->tree_view.Render(
        context,
        ToD2DRect(layout.tree),
        impl_->tree_rows,
        impl_->tree_scroll,
        impl_->tree_state.selected_thread,
        impl_->tree_row_focus_visible ? std::optional<std::size_t>{impl_->selected_tree_row} : std::nullopt,
        impl_->hovered_tree_row,
        impl_->tree_scrollbar_opacity,
        impl_->tree_scrollbar_width,
        impl_->title_marquee_animations,
        palette
    );
    const float splitter_center = (layout.splitter.left + layout.splitter.right) * 0.5F;
    impl_->subtitle_brush->SetOpacity(0.52F);
    context->DrawLine(
        D2D1::Point2F(splitter_center, layout.splitter.top),
        D2D1::Point2F(splitter_center, layout.splitter.bottom),
        impl_->subtitle_brush.Get(),
        1.0F
    );
    const float handle_center_y = (layout.splitter.top + layout.splitter.bottom) * 0.5F;
    for (int index = -1; index <= 1; ++index) {
        const float y = handle_center_y + static_cast<float>(index) * 7.0F;
        context->DrawLine(
            D2D1::Point2F(splitter_center - 2.0F, y),
            D2D1::Point2F(splitter_center + 2.0F, y),
            impl_->subtitle_brush.Get(),
            1.25F
        );
    }
    impl_->subtitle_brush->SetOpacity(1.0F);
    const std::wstring workbench_title = impl_->tree_state.selected_thread ? L"Session" : L"Select a session";
    const std::wstring workbench_subtitle = impl_->tree_state.selected_thread
        ? std::wstring(impl_->tree_state.selected_thread->begin(), impl_->tree_state.selected_thread->end())
        : state.subtitle;
    context->DrawTextW(
        workbench_title.c_str(),
        static_cast<UINT32>(workbench_title.size()),
        impl_->workbench_title_format.Get(),
        D2D1::RectF(layout.workbench.left + kDeckContentLeft, kDeckContentTop, layout.workbench.right - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight),
        impl_->title_brush.Get()
    );
    context->DrawTextW(
        workbench_subtitle.c_str(),
        static_cast<UINT32>(workbench_subtitle.size()),
        impl_->subtitle_format.Get(),
        D2D1::RectF(layout.workbench.left + kDeckContentLeft, kDeckContentTop + kDeckTitleHeight, layout.workbench.right - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight + kDeckSubtitleHeight),
        impl_->subtitle_brush.Get()
    );
    if (impl_->command_palette_open) {
        impl_->command_palette_view.Render(
            context,
            D2D1::RectF(0.0F, 0.0F, size.width, size.height),
            impl_->command_palette_query,
            impl_->command_palette_entries,
            impl_->command_palette_selection,
            palette
        );
    }
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
    impl_->tree_scrollbar_opacity = 1.0F;
    impl_->tree_scroll.ScrollBy(-static_cast<float>(delta) / WHEEL_DELTA * kWheelLineHeight * kWheelLinesPerNotch);
}

// ----------------------------------------------------------------------------
// Traite un clic pointeur pour la zone Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerDown(float x, float y) {
    if (impl_ == nullptr) {
        return;
    }
    if (y < kActivityBarHeight) {
        if (const auto filter = ActivityFilterAt(x)) {
            impl_->active_filter = ToggleFilter(impl_->active_filter, *filter);
            impl_->selected_tree_row = 0;
            impl_->tree_scroll.offset = 0.0F;
            impl_->RebuildTreeRows();
        }
        return;
    }
    const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, kActivityBarHeight, impl_->tree_width, kActivityBarHeight + impl_->tree_scroll.viewport_extent);
    const ScrollbarMetrics scrollbar = impl_->TreeScrollbarMetrics(tree_bounds);
    if (HitTestVerticalScrollbarTrack(scrollbar, x, y)) {
        impl_->dragging_tree_scrollbar = true;
        if (HitTestVerticalScrollbar(scrollbar, x, y)) {
            impl_->tree_scrollbar_drag_grab_y = y - scrollbar.thumb_top;
        } else {
            impl_->tree_scrollbar_drag_grab_y = (scrollbar.thumb_bottom - scrollbar.thumb_top) * 0.5F;
            impl_->tree_scroll.offset = ScrollOffsetFromThumbTop(scrollbar, y - impl_->tree_scrollbar_drag_grab_y);
            impl_->tree_scroll.Clamp();
        }
        impl_->tree_scrollbar_opacity = 1.0F;
        impl_->tree_drag.Cancel();
        return;
    }
    if (IsSplitterHit(x, impl_->tree_width)) {
        impl_->resizing_tree = true;
        impl_->tree_drag.Cancel();
        return;
    }
    if (impl_->tree_rows.empty()) {
        return;
    }
    const auto hit = HitTestTreeRow(D2D1::Point2F(x, y), tree_bounds, impl_->tree_scroll, impl_->tree_view.RowHeight(), impl_->tree_rows.size());
    if (!hit) {
        return;
    }
    impl_->selected_tree_row = *hit;
    impl_->tree_row_focus_visible = true;
    impl_->tree_drag.Begin(impl_->tree_rows[*hit], TreeDragPoint{x, y});
    impl_->tree_view.ActivateRow(impl_->tree_rows[*hit]);
    ActivateTreeRow(impl_->tree_state, impl_->tree_rows[*hit]);
    impl_->RebuildTreeRows();
}

// ----------------------------------------------------------------------------
// Traite un mouvement pointeur pour le drag interne Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerMove(float x, float y) {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->resizing_tree) {
        impl_->hovered_tree_row.reset();
        impl_->tree_scrollbar_hovered = false;
        impl_->tree_width = ClampTreeWidth(x);
        impl_->tree_scrollbar_opacity = 1.0F;
        return;
    }
    if (impl_->dragging_tree_scrollbar) {
        impl_->hovered_tree_row.reset();
        const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, kActivityBarHeight, impl_->tree_width, kActivityBarHeight + impl_->tree_scroll.viewport_extent);
        const ScrollbarMetrics scrollbar = impl_->TreeScrollbarMetrics(tree_bounds);
        impl_->tree_scroll.offset = ScrollOffsetFromThumbTop(scrollbar, y - impl_->tree_scrollbar_drag_grab_y);
        impl_->tree_scroll.Clamp();
        impl_->tree_scrollbar_opacity = 1.0F;
        return;
    }
    const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, kActivityBarHeight, impl_->tree_width, kActivityBarHeight + impl_->tree_scroll.viewport_extent);
    impl_->tree_hovered = x >= 0.0F && x <= impl_->tree_width && y >= kActivityBarHeight;
    const ScrollbarMetrics scrollbar = impl_->TreeScrollbarMetrics(tree_bounds);
    impl_->tree_scrollbar_hovered = HitTestVerticalScrollbar(scrollbar, x, y);
    if (impl_->tree_hovered) {
        impl_->tree_scrollbar_opacity = 1.0F;
    }
    if (impl_->tree_rows.empty()) {
        impl_->hovered_tree_row.reset();
        return;
    }
    const auto hit = HitTestVerticalScrollbarTrack(scrollbar, x, y)
        ? std::nullopt
        : HitTestTreeRow(D2D1::Point2F(x, y), tree_bounds, impl_->tree_scroll, impl_->tree_view.RowHeight(), impl_->tree_rows.size());
    impl_->hovered_tree_row = hit && impl_->tree_rows[*hit].kind == TreeRowKind::Session
        ? hit
        : std::nullopt;
    const TreeRow* target = hit ? &impl_->tree_rows[*hit] : nullptr;
    impl_->tree_drag.Update(TreeDragPoint{x, y}, target);
}

// ----------------------------------------------------------------------------
// Signale que le pointeur a quitte la fenetre.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerLeave() {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->dragging_tree_scrollbar || impl_->resizing_tree) {
        return;
    }
    impl_->tree_hovered = false;
    impl_->hovered_tree_row.reset();
    impl_->tree_scrollbar_hovered = false;
}

// ----------------------------------------------------------------------------
// Termine un clic ou drag pointeur pour le Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerUp(float x, float y) {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->dragging_tree_scrollbar) {
        OnPointerMove(x, y);
        impl_->dragging_tree_scrollbar = false;
        return;
    }
    OnPointerMove(x, y);
    impl_->resizing_tree = false;
    const auto assignment = impl_->tree_drag.Drop();
    if (!assignment) {
        return;
    }
    for (SessionRecord& session : impl_->render_catalog.sessions) {
        if (session.codex.id == assignment->thread_id) {
            session.project_id = assignment->project_id;
            break;
        }
    }
    impl_->RebuildTreeRows();
}

// ----------------------------------------------------------------------------
// Indique si un point client touche le separateur redimensionnable.
// ----------------------------------------------------------------------------
bool DeckRenderer::IsPointOnSplitter(float x, float y) const {
    if (impl_ == nullptr || y < kActivityBarHeight) {
        return false;
    }
    const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, kActivityBarHeight, impl_->tree_width, kActivityBarHeight + impl_->tree_scroll.viewport_extent);
    if (HitTestVerticalScrollbarTrack(impl_->TreeScrollbarMetrics(tree_bounds), x, y)) {
        return false;
    }
    return impl_->resizing_tree || IsSplitterHit(x, impl_->tree_width);
}

// ----------------------------------------------------------------------------
// Avance les animations legeres du renderer.
// ----------------------------------------------------------------------------
bool DeckRenderer::AdvanceAnimations() {
    if (impl_ == nullptr) {
        return false;
    }
    const auto animation_tick = std::chrono::steady_clock::now();
    const float animation_delta_seconds = std::clamp(
        std::chrono::duration<float>(animation_tick - impl_->last_animation_tick).count(),
        0.0F,
        kMaximumAnimationDeltaSeconds
    );
    impl_->last_animation_tick = animation_tick;
    const float target_scrollbar_width = (impl_->tree_scrollbar_hovered || impl_->dragging_tree_scrollbar)
        ? kTreeScrollbarHoverWidth
        : kTreeScrollbarRestWidth;
    const float previous_scrollbar_width = impl_->tree_scrollbar_width;
    impl_->tree_scrollbar_width = ApproachVisualValue(
        impl_->tree_scrollbar_width,
        target_scrollbar_width,
        kScrollbarWidthStep
    );
    std::optional<CodexThreadId> hovered_thread;
    if (impl_->hovered_tree_row && *impl_->hovered_tree_row < impl_->tree_rows.size()) {
        hovered_thread = impl_->tree_rows[*impl_->hovered_tree_row].thread_id;
    }
    if (impl_->tree_state.selected_thread) {
        impl_->title_marquee_animations.try_emplace(*impl_->tree_state.selected_thread);
    }
    if (hovered_thread) {
        impl_->title_marquee_animations.try_emplace(*hovered_thread);
    }

    bool marquee_animating = false;
    for (auto animation = impl_->title_marquee_animations.begin(); animation != impl_->title_marquee_animations.end();) {
        const bool active = (impl_->tree_state.selected_thread && animation->first == *impl_->tree_state.selected_thread)
            || (hovered_thread && animation->first == *hovered_thread);
        const MarqueeAnimationState previous = animation->second;
        animation->second = AdvanceMarqueeAnimation(
            animation->second,
            active,
            animation_delta_seconds,
            kTitleMarqueeSmoothTimeSeconds,
            kTitleMarqueeCoastProjectionSeconds,
            kTitleMarqueeMaximumSpeed
        );
        if (!active
            && !animation->second.coasting
            && animation->second.visual.value == 0.0F
            && animation->second.visual.velocity == 0.0F) {
            animation = impl_->title_marquee_animations.erase(animation);
            continue;
        }
        marquee_animating = marquee_animating
            || animation->second.visual.value != previous.visual.value
            || animation->second.visual.velocity != previous.visual.velocity
            || animation->second.coasting != previous.coasting;
        ++animation;
    }
    if (!impl_->tree_hovered && !impl_->resizing_tree && !impl_->dragging_tree_scrollbar && impl_->tree_scrollbar_opacity > 0.0F) {
        impl_->tree_scrollbar_opacity = std::max(0.0F, impl_->tree_scrollbar_opacity - kScrollbarFadeStep);
        return impl_->tree_scrollbar_opacity > 0.0F || previous_scrollbar_width != impl_->tree_scrollbar_width || marquee_animating;
    }
    return impl_->tree_hovered
        || impl_->resizing_tree
        || impl_->dragging_tree_scrollbar
        || impl_->tree_scrollbar_opacity > 0.0F
        || previous_scrollbar_width != impl_->tree_scrollbar_width
        || marquee_animating;
}

// ----------------------------------------------------------------------------
// Traite une touche clavier de navigation Tree.
// ----------------------------------------------------------------------------
bool DeckRenderer::OnKeyDown(WPARAM virtual_key) {
    if (impl_ == nullptr || impl_->tree_rows.empty()) {
        return false;
    }

    if (impl_->rename_active) {
        if (virtual_key == VK_ESCAPE) {
            impl_->rename_active = false;
            impl_->rename_thread.reset();
            impl_->rename_buffer.clear();
            return true;
        }
        if (virtual_key == VK_BACK) {
            if (!impl_->rename_buffer.empty()) {
                impl_->rename_buffer.pop_back();
            }
            return true;
        }
        if (virtual_key == VK_RETURN) {
            if (impl_->rename_thread) {
                for (SessionRecord& session : impl_->render_catalog.sessions) {
                    if (session.codex.id == *impl_->rename_thread) {
                        session.codex.name = NarrowAscii(impl_->rename_buffer);
                        break;
                    }
                }
                impl_->RebuildTreeRows();
            }
            impl_->rename_active = false;
            impl_->rename_thread.reset();
            impl_->rename_buffer.clear();
            return true;
        }
        return true;
    }

    if (const auto command = TranslateShortcut(KeyChord{virtual_key, IsControlDown(), IsShiftDown()})) {
        impl_->ExecuteCommand(*command);
        return true;
    }

    if (impl_->command_palette_open) {
        switch (virtual_key) {
        case VK_ESCAPE:
            impl_->command_palette_open = false;
            return true;
        case VK_UP:
            if (impl_->command_palette_selection > 0) {
                --impl_->command_palette_selection;
            }
            return true;
        case VK_DOWN:
            if (impl_->command_palette_selection + 1 < impl_->command_palette_entries.size()) {
                ++impl_->command_palette_selection;
            }
            return true;
        case VK_BACK:
            if (!impl_->command_palette_query.empty()) {
                impl_->command_palette_query.pop_back();
                impl_->RebuildCommandPalette();
            }
            return true;
        case VK_RETURN:
            impl_->command_palette_open = false;
            return true;
        default:
            return true;
        }
    }

    switch (virtual_key) {
    case VK_UP:
        impl_->tree_row_focus_visible = true;
        if (impl_->selected_tree_row > 0) {
            --impl_->selected_tree_row;
            impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        }
        return true;
    case VK_DOWN:
        impl_->tree_row_focus_visible = true;
        if (impl_->selected_tree_row + 1 < impl_->tree_rows.size()) {
            ++impl_->selected_tree_row;
            impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        }
        return true;
    case VK_HOME:
        impl_->tree_row_focus_visible = true;
        impl_->selected_tree_row = 0;
        impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        return true;
    case VK_END:
        impl_->tree_row_focus_visible = true;
        impl_->selected_tree_row = impl_->tree_rows.empty() ? 0 : impl_->tree_rows.size() - 1;
        impl_->tree_scroll.EnsureVisible(impl_->selected_tree_row, impl_->tree_view.RowHeight());
        return true;
    case VK_RETURN:
    case VK_LEFT:
    case VK_RIGHT:
        impl_->tree_row_focus_visible = true;
        impl_->tree_view.ActivateRow(impl_->tree_rows[impl_->selected_tree_row]);
        ActivateTreeRow(impl_->tree_state, impl_->tree_rows[impl_->selected_tree_row]);
        impl_->RebuildTreeRows();
        return true;
    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Traite un caractere texte pour la Command Palette.
// ----------------------------------------------------------------------------
bool DeckRenderer::OnChar(wchar_t character) {
    if (impl_ == nullptr) {
        return false;
    }
    if (impl_->rename_active) {
        if (character == L'\b' || character == L'\r' || character == L'\n' || character == L'\t') {
            return true;
        }
        if (std::iswcntrl(character) != 0) {
            return true;
        }
        impl_->rename_buffer.push_back(character);
        return true;
    }
    if (!impl_->command_palette_open) {
        return false;
    }
    if (character == L'\b' || character == L'\r' || character == L'\n' || character == L'\t') {
        return true;
    }
    if (std::iswcntrl(character) != 0) {
        return true;
    }
    impl_->command_palette_query.push_back(character);
    impl_->RebuildCommandPalette();
    return true;
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

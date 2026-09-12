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

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
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

    // Vue Tree virtualisee.
    ProjectTreeView tree_view;

    // Vue de barre d'activite.
    ActivityBarView activity_bar_view;

    // Vue de Command Palette.
    CommandPaletteView command_palette_view;

    // Controleur de drag interne du Tree.
    TreeDragController tree_drag;

    // Lignes synthetiques de debug pour valider le rendu volumineux.
    std::vector<TreeRow> debug_tree_rows;

    // Catalogue synthetique conserve pour reconstruire les lignes.
    SessionCatalogSnapshot debug_catalog;

    // Etat de scroll synthetique.
    ScrollState tree_scroll;

    // Etat de deploiement synthetique.
    ProjectTreeState tree_state;

    // Filtre global courant.
    SessionFilter active_filter = SessionFilter::All;

    // Ligne selectionnee au clavier.
    std::size_t selected_tree_row = 0;

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

    // Indique qu'un renommage inline synthetique est en cours.
    bool rename_active = false;

    // Thread renomme en cours.
    std::optional<CodexThreadId> rename_thread;

    // Titre avant edition pour restauration locale.
    std::string rename_original_title;

    // Titre edite localement.
    std::wstring rename_buffer;

    // ------------------------------------------------------------------------
    // Reconstruit les lignes synthetiques depuis l'etat courant.
    // ------------------------------------------------------------------------
    void RebuildDebugRows();

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

// Hauteur compacte de la barre d'activite.
constexpr float kActivityBarHeight = 42.0F;

// Hauteur reservee au composer futur.
constexpr float kComposerPlaceholderHeight = 120.0F;

// Largeur de zone de hit du separateur.
constexpr float kSplitterHitWidth = 8.0F;

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
// Reconstruit les lignes synthetiques depuis l'etat courant.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildDebugRows() {
    SessionCatalogSnapshot visible_catalog = debug_catalog;
    visible_catalog.sessions = FilterSessions(debug_catalog.sessions, active_filter);
    debug_tree_rows = BuildProjectTreeRows(visible_catalog, tree_state);
    if (!debug_tree_rows.empty() && selected_tree_row >= debug_tree_rows.size()) {
        selected_tree_row = debug_tree_rows.size() - 1;
    }
}

// ----------------------------------------------------------------------------
// Reconstruit les entrees de Command Palette depuis la requete courante.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildCommandPalette() {
    command_palette_entries = BuildCommandPaletteEntries(debug_catalog, command_palette_query);
    if (!command_palette_entries.empty() && command_palette_selection >= command_palette_entries.size()) {
        command_palette_selection = command_palette_entries.size() - 1;
    } else if (command_palette_entries.empty()) {
        command_palette_selection = 0;
    }
}

// ----------------------------------------------------------------------------
// Execute une commande applicative sur l'etat synthetique local.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::ExecuteCommand(DeckCommandKind command) {
    if (debug_tree_rows.empty()) {
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
        const TreeRow& row = debug_tree_rows[selected_tree_row];
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
        const TreeRow& row = debug_tree_rows[selected_tree_row];
        if (row.kind != TreeRowKind::Session || !row.thread_id) {
            break;
        }
        for (SessionRecord& session : debug_catalog.sessions) {
            if (session.codex.id == *row.thread_id) {
                session.codex.archived = true;
                break;
            }
        }
        RebuildDebugRows();
        break;
    }
    default:
        break;
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
    const MainLayoutRects layout = ComputeMainLayout(
        SizeF{size.width, size.height},
        impl_->tree_width,
        kActivityBarHeight,
        kComposerPlaceholderHeight
    );
    impl_->tree_width = layout.tree.right - layout.tree.left;
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
    impl_->tree_scroll.viewport_extent = std::max(0.0F, layout.tree.bottom - layout.tree.top);
    impl_->tree_scroll.content_extent = static_cast<float>(impl_->debug_tree_rows.size()) * impl_->tree_view.RowHeight();
    impl_->tree_scroll.Clamp();
    const ActivityCounts counts = BuildActivityCounts(impl_->debug_catalog.sessions);
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
        impl_->debug_tree_rows,
        impl_->tree_scroll,
        palette
    );
    context->FillRectangle(ToD2DRect(layout.splitter), impl_->subtitle_brush.Get());
    const std::wstring workbench_title = impl_->tree_state.selected_thread
        ? L"Session: " + std::wstring(impl_->tree_state.selected_thread->begin(), impl_->tree_state.selected_thread->end())
        : L"Select a session";
    context->DrawTextW(
        workbench_title.c_str(),
        static_cast<UINT32>(workbench_title.size()),
        impl_->title_format.Get(),
        D2D1::RectF(layout.workbench.left + kDeckContentLeft, kDeckContentTop, layout.workbench.right - kDeckContentLeft, kDeckContentTop + kDeckTitleHeight),
        impl_->title_brush.Get()
    );
    context->DrawTextW(
        state.subtitle.c_str(),
        static_cast<UINT32>(state.subtitle.size()),
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
    impl_->tree_scroll.ScrollBy(-static_cast<float>(delta) / WHEEL_DELTA * kWheelLineHeight * kWheelLinesPerNotch);
}

// ----------------------------------------------------------------------------
// Traite un clic pointeur pour la zone Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerDown(float x, float y) {
    if (impl_ == nullptr || impl_->debug_tree_rows.empty()) {
        return;
    }
    if (y < kActivityBarHeight) {
        if (const auto filter = ActivityFilterAt(x)) {
            impl_->active_filter = ToggleFilter(impl_->active_filter, *filter);
            impl_->selected_tree_row = 0;
            impl_->tree_scroll.offset = 0.0F;
            impl_->RebuildDebugRows();
        }
        return;
    }
    if (IsSplitterHit(x, impl_->tree_width)) {
        impl_->resizing_tree = true;
        impl_->tree_drag.Cancel();
        return;
    }

    const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, kActivityBarHeight, impl_->tree_width, kActivityBarHeight + impl_->tree_scroll.viewport_extent);
    const auto hit = HitTestTreeRow(D2D1::Point2F(x, y), tree_bounds, impl_->tree_scroll, impl_->tree_view.RowHeight(), impl_->debug_tree_rows.size());
    if (!hit) {
        return;
    }
    impl_->selected_tree_row = *hit;
    impl_->tree_drag.Begin(impl_->debug_tree_rows[*hit], TreeDragPoint{x, y});
    impl_->tree_view.ActivateRow(impl_->debug_tree_rows[*hit]);
    ActivateTreeRow(impl_->tree_state, impl_->debug_tree_rows[*hit]);
    impl_->RebuildDebugRows();
}

// ----------------------------------------------------------------------------
// Traite un mouvement pointeur pour le drag interne Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerMove(float x, float y) {
    if (impl_ == nullptr || impl_->debug_tree_rows.empty()) {
        return;
    }
    if (impl_->resizing_tree) {
        impl_->tree_width = ClampTreeWidth(x);
        return;
    }
    const D2D1_RECT_F tree_bounds = D2D1::RectF(0.0F, kActivityBarHeight, impl_->tree_width, kActivityBarHeight + impl_->tree_scroll.viewport_extent);
    const auto hit = HitTestTreeRow(D2D1::Point2F(x, y), tree_bounds, impl_->tree_scroll, impl_->tree_view.RowHeight(), impl_->debug_tree_rows.size());
    const TreeRow* target = hit ? &impl_->debug_tree_rows[*hit] : nullptr;
    impl_->tree_drag.Update(TreeDragPoint{x, y}, target);
}

// ----------------------------------------------------------------------------
// Termine un clic ou drag pointeur pour le Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnPointerUp(float x, float y) {
    if (impl_ == nullptr) {
        return;
    }
    OnPointerMove(x, y);
    impl_->resizing_tree = false;
    const auto assignment = impl_->tree_drag.Drop();
    if (!assignment) {
        return;
    }
    for (SessionRecord& session : impl_->debug_catalog.sessions) {
        if (session.codex.id == assignment->thread_id) {
            session.project_id = assignment->project_id;
            break;
        }
    }
    impl_->RebuildDebugRows();
}

// ----------------------------------------------------------------------------
// Traite une touche clavier de navigation Tree.
// ----------------------------------------------------------------------------
bool DeckRenderer::OnKeyDown(WPARAM virtual_key) {
    if (impl_ == nullptr || impl_->debug_tree_rows.empty()) {
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
                for (SessionRecord& session : impl_->debug_catalog.sessions) {
                    if (session.codex.id == *impl_->rename_thread) {
                        session.codex.name = NarrowAscii(impl_->rename_buffer);
                        break;
                    }
                }
                impl_->RebuildDebugRows();
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

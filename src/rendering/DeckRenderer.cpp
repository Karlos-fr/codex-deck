// ============================================================================
// Codex Deck - Implementation du rendu DirectComposition minimal
// ----------------------------------------------------------------------------
// Ce fichier dessine la coquille visuelle avec Direct2D sur la surface fournie
// par CompositionHost. Il ne gere ni fenetre, ni protocole Codex, ni stockage.
// ============================================================================

#include "DeckRenderer.h"

#include "WidgetGlassElementLenses.h"
#include "WidgetRenderGlassEffect.h"
#include "../glass/WidgetGlassEffect.h"
#include "../input/KeyboardShortcuts.h"
#include "../navigation/ActivityBarModel.h"
#include "../navigation/ActivityBarView.h"
#include "../navigation/CommandPaletteModel.h"
#include "../navigation/CommandPaletteView.h"
#include "../navigation/ProjectTreeModel.h"
#include "../navigation/TreeHitTesting.h"
#include "../navigation/ProjectTreeView.h"
#include "../navigation/TreeDragController.h"
#include "../projects/ProjectEditorModel.h"
#include "../projects/ProjectEditorView.h"
#include "../ui/MainLayout.h"
#include "../ui/ScrollState.h"
#include "../ui/ScrollbarGeometry.h"
#include "../ui/TreeVisualAnimation.h"
#include "../sessions/NewSessionOverlayModel.h"
#include "../sessions/NewSessionOverlayView.h"

#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <cwctype>

using Microsoft::WRL::ComPtr;

// Geometries mises en cache pour le cadre arrondi du Tree.
struct RoundedTreeFrameGeometry {
    // Masque qui retire l'ancien angle droit du fond.
    ComPtr<ID2D1PathGeometry> corner_mask;

    // Trace continu du bord superieur et du separateur.
    ComPtr<ID2D1PathGeometry> frame;

    // Rectangle utilise lors de la derniere construction.
    D2D1_RECT_F bounds{};

    // Rayon utilise lors de la derniere construction.
    float radius = -1.0F;
};

// Requete immutable transmise au worker de recherche volumineuse.
struct PaletteSearchRequest {
    // Snapshot de catalogue isole du thread UI.
    SessionCatalogSnapshot catalog;
    // Requete texte associee.
    std::wstring query;
    // Mode de recherche demande.
    CommandPaletteMode mode = CommandPaletteMode::Commands;
    // Generation monotone permettant d'ecarter un resultat obsolete.
    std::uint64_t generation = 0;
};

// Resultat calcule par le worker et pret a etre publie sur le thread UI.
struct PaletteSearchResult {
    // Entrees triees retournees par le matcher.
    std::vector<PaletteEntry> entries;
    // Generation de la requete source.
    std::uint64_t generation = 0;
};

struct DeckRenderer::Impl {
    // Cree l'etat et demarre le worker persistant de recherche.
    Impl();

    // Arrete le worker avant de liberer l'etat partage.
    ~Impl();

    // Hote DirectComposition proprietaire de la surface de dessin.
    CompositionHost composition;

    // Runtime de capture du bureau utilise par le Workbench Glass.
    WidgetGlassEffect glass_effect;

    // Cache de composition Direct2D/GPU du fond Glass.
    WidgetRenderGlassEffectCache glass_cache;

    // Reglages visuels et animes courants du Workbench Glass.
    DeckGlassSettings glass_settings{};

    // Factory DirectWrite utilisee pour les formats de texte.
    ComPtr<IDWriteFactory> dwrite_factory;

    // Brosse du fond principal.
    ComPtr<ID2D1SolidColorBrush> background_brush;

    // Brosse du titre.
    ComPtr<ID2D1SolidColorBrush> title_brush;

    // Brosse du sous-titre.
    ComPtr<ID2D1SolidColorBrush> subtitle_brush;

    // Geometries du cadre de navigation recalculees seulement au resize.
    RoundedTreeFrameGeometry rounded_tree_frame;

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

    // Vue de creation globale de session.
    NewSessionOverlayView new_session_view;

    // Etat optionnel du formulaire de creation.
    std::optional<NewSessionOverlayModel> new_session_overlay;

    // Vue de creation, renommage et suppression d'un projet logique.
    ProjectEditorView project_editor_view;

    // Etat optionnel de l'editeur de projet.
    std::optional<ProjectEditorModel> project_editor;

    // Catalogue de modeles connu du renderer.
    std::vector<CodexModelInfo> available_models;

    // Threads archives conserves hors du catalogue actif.
    std::vector<CodexThreadSummary> archive_threads;

    // Handler applicatif des commandes de cycle de vie.
    DeckCommandHandler command_handler;

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

    // Position du caret dans la requete de palette.
    std::size_t command_palette_cursor = 0;

    // Mode courant distinguant recherche et commandes.
    CommandPaletteMode command_palette_mode = CommandPaletteMode::Commands;

    // Instant de redemarrage du clignotement du caret.
    std::chrono::steady_clock::time_point command_palette_caret_epoch = std::chrono::steady_clock::now();

    // Taille logique du dernier rendu utilisee par le hit testing d'overlay.
    D2D1_SIZE_F viewport_size = D2D1::SizeF(1.0F, 1.0F);

    // Fenetre a invalider lorsqu'un resultat asynchrone est disponible.
    std::atomic<HWND> render_window = nullptr;

    // Worker persistant reserve au scoring des gros catalogues.
    std::jthread command_palette_worker;

    // Protege requete et resultat echanges avec le worker.
    std::mutex command_palette_worker_mutex;

    // Reveille le worker lorsqu'une requete plus recente arrive.
    std::condition_variable_any command_palette_worker_condition;

    // Derniere requete restant a calculer.
    std::optional<PaletteSearchRequest> pending_palette_request;

    // Dernier resultat attendant sa publication UI.
    std::optional<PaletteSearchResult> pending_palette_result;

    // Generation courante de recherche.
    std::atomic<std::uint64_t> command_palette_generation = 0;

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

    // Opacite animee des guides reliant categories et sessions.
    float tree_indent_guide_opacity = 0.0F;

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
    // Publie sur le thread UI un resultat produit par le worker.
    // ------------------------------------------------------------------------
    void ApplyPendingCommandPaletteResult();

    // ------------------------------------------------------------------------
    // Execute une commande applicative sur l'etat synthetique local.
    //
    // Parametres :
    // - command : commande parametree a traiter.
    // ------------------------------------------------------------------------
    void ExecuteCommand(const DeckCommand& command);

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

// Rayon du raccord entre le bord superieur du Tree et son separateur.
constexpr float kNavigationCornerRadius = 8.0F;

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

// Volume a partir duquel le scoring quitte le thread UI.
constexpr std::size_t kPaletteWorkerThreshold = 2000;

// Pas de disparition de scrollbar par frame.
constexpr float kScrollbarFadeStep = 0.08F;

// Pas de fondu des guides de filiation par frame.
constexpr float kIndentGuideFadeStep = 0.075F;

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
    if (x >= 14.0F && x < 112.0F) {
        return SessionFilter::Working;
    }
    if (x < 218.0F) {
        return SessionFilter::NeedsAttention;
    }
    if (x < 344.0F) {
        return SessionFilter::CompletedToday;
    }
    if (x < 444.0F) {
        return SessionFilter::Favorites;
    }
    if (x < 544.0F) {
        return SessionFilter::Archived;
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
// Dessine le cadre arrondi du Tree et masque son ancien angle droit.
//
// Parametres :
// - dc : contexte Direct2D cible.
// - tree_bounds : rectangle courant du Tree.
// - corner_radius : rayon du raccord superieur droit.
// - outside_brush : brosse du fond exterieur utilisee pour masquer le coin.
// - border_brush : brosse du cadre et du separateur.
// ----------------------------------------------------------------------------
void DrawRoundedTreeFrame(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& tree_bounds,
    float corner_radius,
    ID2D1Brush* outside_brush,
    ID2D1Brush* border_brush,
    RoundedTreeFrameGeometry& geometry
) {
    if (dc == nullptr || outside_brush == nullptr || border_brush == nullptr) {
        return;
    }
    const float radius = std::clamp(
        corner_radius,
        0.0F,
        std::min(tree_bounds.right - tree_bounds.left, tree_bounds.bottom - tree_bounds.top)
    );
    ComPtr<ID2D1Factory> factory;
    dc->GetFactory(factory.GetAddressOf());
    if (!factory || radius <= 0.0F) {
        return;
    }

    const bool geometry_current = geometry.corner_mask
        && geometry.frame
        && geometry.bounds.left == tree_bounds.left
        && geometry.bounds.top == tree_bounds.top
        && geometry.bounds.right == tree_bounds.right
        && geometry.bounds.bottom == tree_bounds.bottom
        && geometry.radius == radius;
    if (!geometry_current) {
        geometry.corner_mask.Reset();
        geometry.frame.Reset();

        ComPtr<ID2D1GeometrySink> mask_sink;
        if (FAILED(factory->CreatePathGeometry(geometry.corner_mask.GetAddressOf()))
            || FAILED(geometry.corner_mask->Open(mask_sink.GetAddressOf()))) {
            return;
        }
        mask_sink->BeginFigure(
            D2D1::Point2F(tree_bounds.right - radius, tree_bounds.top),
            D2D1_FIGURE_BEGIN_FILLED
        );
        mask_sink->AddLine(D2D1::Point2F(tree_bounds.right, tree_bounds.top));
        mask_sink->AddLine(D2D1::Point2F(tree_bounds.right, tree_bounds.top + radius));
        mask_sink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(tree_bounds.right - radius, tree_bounds.top),
            D2D1::SizeF(radius, radius),
            0.0F,
            D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL
        ));
        mask_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        if (FAILED(mask_sink->Close())) {
            geometry.corner_mask.Reset();
            return;
        }

        ComPtr<ID2D1GeometrySink> frame_sink;
        if (FAILED(factory->CreatePathGeometry(geometry.frame.GetAddressOf()))
            || FAILED(geometry.frame->Open(frame_sink.GetAddressOf()))) {
            geometry.corner_mask.Reset();
            return;
        }
        frame_sink->BeginFigure(
            D2D1::Point2F(tree_bounds.left, tree_bounds.top),
            D2D1_FIGURE_BEGIN_HOLLOW
        );
        frame_sink->AddLine(D2D1::Point2F(tree_bounds.right - radius, tree_bounds.top));
        frame_sink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(tree_bounds.right, tree_bounds.top + radius),
            D2D1::SizeF(radius, radius),
            0.0F,
            D2D1_SWEEP_DIRECTION_CLOCKWISE,
            D2D1_ARC_SIZE_SMALL
        ));
        frame_sink->AddLine(D2D1::Point2F(tree_bounds.right, tree_bounds.bottom));
        frame_sink->EndFigure(D2D1_FIGURE_END_OPEN);
        if (FAILED(frame_sink->Close())) {
            geometry.corner_mask.Reset();
            geometry.frame.Reset();
            return;
        }
        geometry.bounds = tree_bounds;
        geometry.radius = radius;
    }

    dc->FillGeometry(geometry.corner_mask.Get(), outside_brush);
    dc->DrawGeometry(geometry.frame.Get(), border_brush, 1.0F);
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

// Convertit un texte UTF-16 en UTF-8 pour les requetes Codex.
std::string NarrowUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(0, size)), '\0');
    if (size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    }
    return result;
}

// Convertit un texte UTF-8 en UTF-16 pour les overlays natifs.
std::wstring WidenUtf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(text.begin(), text.end());
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// Retourne le projet associe a la selection courante.
template <typename State>
std::optional<ProjectId> CurrentProjectId(const State& state) {
    if (!state.tree_rows.empty() && state.selected_tree_row < state.tree_rows.size()) {
        const TreeRow& row = state.tree_rows[state.selected_tree_row];
        if (row.project_id) {
            return row.project_id;
        }
    }
    if (state.tree_state.selected_thread) {
        for (const SessionRecord& session : state.render_catalog.sessions) {
            if (session.codex.id == *state.tree_state.selected_thread) {
                return session.project_id;
            }
        }
    }
    return std::nullopt;
}

// Retourne la racine principale d'un projet.
template <typename State>
std::optional<std::filesystem::path> ProjectRoot(const State& state, std::optional<ProjectId> project_id) {
    if (!project_id) {
        return std::nullopt;
    }
    for (const Project& project : state.render_catalog.projects) {
        if (project.id == *project_id && !project.roots.empty()) {
            return project.roots.front();
        }
    }
    return std::nullopt;
}

}  // namespace

// Cree l'etat et demarre le worker persistant de recherche.
DeckRenderer::Impl::Impl() {
    command_palette_worker = std::jthread([this](std::stop_token stop_token) {
        std::unique_lock lock(command_palette_worker_mutex);
        while (!stop_token.stop_requested()) {
            command_palette_worker_condition.wait(lock, stop_token, [this] {
                return pending_palette_request.has_value();
            });
            if (stop_token.stop_requested()) {
                break;
            }
            PaletteSearchRequest request = std::move(*pending_palette_request);
            pending_palette_request.reset();
            lock.unlock();
            std::vector<PaletteEntry> entries = BuildCommandPaletteEntries(
                request.catalog,
                request.query,
                request.mode
            );
            lock.lock();
            if (request.generation == command_palette_generation.load()) {
                pending_palette_result = PaletteSearchResult{std::move(entries), request.generation};
                if (const HWND hwnd = render_window.load()) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
        }
    });
}

// Arrete le worker avant de liberer l'etat partage.
DeckRenderer::Impl::~Impl() {
    command_palette_worker.request_stop();
    command_palette_worker_condition.notify_all();
    if (command_palette_worker.joinable()) {
        command_palette_worker.join();
    }
}

// ----------------------------------------------------------------------------
// Reconstruit les lignes depuis l'etat courant.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildTreeRows() {
    SessionCatalogSnapshot visible_catalog = render_catalog;
    if (active_filter == SessionFilter::Archived) {
        visible_catalog.projects.clear();
        visible_catalog.sessions.clear();
        for (const CodexThreadSummary& thread : archive_threads) {
            SessionRecord record{};
            record.codex = thread;
            record.present_in_codex = true;
            visible_catalog.sessions.push_back(std::move(record));
        }
        tree_state.unassigned_expanded = true;
    } else {
        visible_catalog.sessions = FilterSessions(render_catalog.sessions, active_filter);
    }
    tree_rows = BuildProjectTreeRows(visible_catalog, tree_state);
    if (!tree_rows.empty() && selected_tree_row >= tree_rows.size()) {
        selected_tree_row = tree_rows.size() - 1;
    }
}

// ----------------------------------------------------------------------------
// Reconstruit les entrees de Command Palette depuis la requete courante.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::RebuildCommandPalette() {
    ++command_palette_generation;
    const std::size_t candidate_count = render_catalog.projects.size() + render_catalog.sessions.size();
    if (candidate_count > kPaletteWorkerThreshold) {
        {
            std::lock_guard lock(command_palette_worker_mutex);
            pending_palette_request = PaletteSearchRequest{
                render_catalog,
                command_palette_query,
                command_palette_mode,
                command_palette_generation.load(),
            };
            pending_palette_result.reset();
        }
        command_palette_entries.clear();
        command_palette_selection = 0;
        command_palette_worker_condition.notify_one();
        return;
    }

    {
        std::lock_guard lock(command_palette_worker_mutex);
        pending_palette_request.reset();
        pending_palette_result.reset();
    }
    command_palette_entries = BuildCommandPaletteEntries(render_catalog, command_palette_query, command_palette_mode);
    if (!command_palette_entries.empty() && command_palette_selection >= command_palette_entries.size()) {
        command_palette_selection = command_palette_entries.size() - 1;
    } else if (command_palette_entries.empty()) {
        command_palette_selection = 0;
    }
}

// Publie sur le thread UI un resultat produit par le worker.
void DeckRenderer::Impl::ApplyPendingCommandPaletteResult() {
    std::optional<PaletteSearchResult> result;
    {
        std::lock_guard lock(command_palette_worker_mutex);
        if (pending_palette_result && pending_palette_result->generation == command_palette_generation.load()) {
            result = std::move(pending_palette_result);
            pending_palette_result.reset();
        }
    }
    if (!result) {
        return;
    }
    command_palette_entries = std::move(result->entries);
    command_palette_selection = command_palette_entries.empty()
        ? 0
        : std::min(command_palette_selection, command_palette_entries.size() - 1);
}

// ----------------------------------------------------------------------------
// Execute une commande applicative sur l'etat courant local.
// ----------------------------------------------------------------------------
void DeckRenderer::Impl::ExecuteCommand(const DeckCommand& command) {
    switch (command.kind) {
    case DeckCommandKind::OpenCommandPalette:
        command_palette_mode = CommandPaletteMode::Commands;
        command_palette_open = true;
        command_palette_query.clear();
        command_palette_cursor = 0;
        command_palette_selection = 0;
        command_palette_caret_epoch = std::chrono::steady_clock::now();
        RebuildCommandPalette();
        break;
    case DeckCommandKind::OpenSearch:
        command_palette_mode = CommandPaletteMode::Search;
        command_palette_open = true;
        command_palette_query.clear();
        command_palette_cursor = 0;
        command_palette_selection = 0;
        command_palette_caret_epoch = std::chrono::steady_clock::now();
        RebuildCommandPalette();
        break;
    case DeckCommandKind::NewSession: {
        command_palette_open = false;
        new_session_overlay = BuildNewSessionOverlayModel(render_catalog.projects, CurrentProjectId(*this));
        new_session_overlay->models = available_models;
        break;
    }
    case DeckCommandKind::NewSessionInCurrentProject: {
        const std::optional<ProjectId> project_id = command.project_id ? command.project_id : CurrentProjectId(*this);
        const auto root = command.cwd ? command.cwd : ProjectRoot(*this, project_id);
        if (!project_id || !root) {
            new_session_overlay = BuildNewSessionOverlayModel(render_catalog.projects, project_id);
            new_session_overlay->models = available_models;
            break;
        }
        if (command_handler) {
            DeckCommand enriched = command;
            enriched.project_id = project_id;
            enriched.cwd = root;
            command_handler(std::move(enriched));
        }
        break;
    }
    case DeckCommandKind::OpenThread: {
        if (!command.thread_id) {
            break;
        }
        active_filter = SessionFilter::All;
        tree_state.selected_thread = command.thread_id;
        for (const SessionRecord& session : render_catalog.sessions) {
            if (session.codex.id != *command.thread_id) {
                continue;
            }
            if (session.project_id) {
                tree_state.expanded_projects.insert(*session.project_id);
            } else {
                tree_state.unassigned_expanded = true;
            }
            break;
        }
        RebuildTreeRows();
        const auto row = std::ranges::find_if(tree_rows, [&command](const TreeRow& candidate) {
            return candidate.thread_id == command.thread_id;
        });
        if (row != tree_rows.end()) {
            selected_tree_row = static_cast<std::size_t>(std::distance(tree_rows.begin(), row));
            tree_row_focus_visible = true;
            tree_scroll.EnsureVisible(selected_tree_row, tree_view.RowHeight());
        }
        break;
    }
    case DeckCommandKind::OpenWorkspace: {
        if (!command.project_id) {
            break;
        }
        active_filter = SessionFilter::All;
        tree_state.expanded_projects.insert(*command.project_id);
        RebuildTreeRows();
        const auto row = std::ranges::find_if(tree_rows, [&command](const TreeRow& candidate) {
            return candidate.kind == TreeRowKind::Project && candidate.project_id == command.project_id;
        });
        if (row != tree_rows.end()) {
            selected_tree_row = static_cast<std::size_t>(std::distance(tree_rows.begin(), row));
            tree_row_focus_visible = true;
            tree_scroll.EnsureVisible(selected_tree_row, tree_view.RowHeight());
        }
        break;
    }
    case DeckCommandKind::RenameThread: {
        if (tree_rows.empty() || selected_tree_row >= tree_rows.size()) {
            break;
        }
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
        DeckCommand enriched = command;
        if (!enriched.thread_id && tree_state.selected_thread) {
            enriched.thread_id = tree_state.selected_thread;
        }
        if (command_handler && enriched.thread_id) {
            command_handler(std::move(enriched));
        }
        break;
    }
    case DeckCommandKind::ToggleFavorite: {
        DeckCommand enriched = command;
        if (!enriched.thread_id && tree_state.selected_thread) {
            enriched.thread_id = tree_state.selected_thread;
        }
        if (command_handler && enriched.thread_id) {
            command_handler(std::move(enriched));
        }
        break;
    }
    case DeckCommandKind::NewProject:
        command_palette_open = false;
        if (command_handler) {
            command_handler(command);
        }
        break;
    case DeckCommandKind::RenameProject: {
        const std::optional<ProjectId> project_id = command.project_id ? command.project_id : CurrentProjectId(*this);
        if (!project_id) {
            break;
        }
        const auto project = std::ranges::find_if(render_catalog.projects, [project_id](const Project& candidate) {
            return candidate.id == *project_id;
        });
        if (project == render_catalog.projects.end()) {
            break;
        }
        command_palette_open = false;
        project_editor = ProjectEditorModel{
            ProjectEditorMode::Rename,
            project->id,
            WidenUtf8(project->name),
            project->roots.empty() ? std::filesystem::path{} : project->roots.front(),
            0,
            false,
            {},
        };
        break;
    }
    case DeckCommandKind::DeleteProject: {
        const std::optional<ProjectId> project_id = command.project_id ? command.project_id : CurrentProjectId(*this);
        if (!project_id) {
            break;
        }
        const auto project = std::ranges::find_if(render_catalog.projects, [project_id](const Project& candidate) {
            return candidate.id == *project_id;
        });
        if (project == render_catalog.projects.end()) {
            break;
        }
        command_palette_open = false;
        project_editor = ProjectEditorModel{
            ProjectEditorMode::ConfirmDelete,
            project->id,
            WidenUtf8(project->name),
            project->roots.empty() ? std::filesystem::path{} : project->roots.front(),
            0,
            false,
            {},
        };
        break;
    }
    case DeckCommandKind::OpenFolderAsSession:
    case DeckCommandKind::PickSessionWorkspace:
    case DeckCommandKind::OpenArchive:
    case DeckCommandKind::RestoreArchivedThread:
    case DeckCommandKind::SetThemeSystem:
    case DeckCommandKind::SetThemeLight:
    case DeckCommandKind::SetThemeDark:
        command_palette_open = false;
        if (command_handler) {
            command_handler(command);
        }
        break;
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

// Installe le handler des commandes qui sortent du renderer.
void DeckRenderer::SetCommandHandler(DeckCommandHandler handler) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->command_handler = std::move(handler);
}

// Met a jour le catalogue de modeles propose dans l'overlay de creation.
void DeckRenderer::SetAvailableModels(std::vector<CodexModelInfo> models) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->available_models = std::move(models);
    if (impl_->new_session_overlay) {
        impl_->new_session_overlay->models = impl_->available_models;
    }
}

// Selectionne un thread cree ou restaure depuis le thread UI.
void DeckRenderer::SelectThread(const CodexThreadId& thread_id) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->new_session_overlay.reset();
    impl_->ExecuteCommand(DeckCommand{DeckCommandKind::OpenThread, std::nullopt, thread_id});
}

// Affiche une erreur de creation dans l'overlay encore ouvert.
void DeckRenderer::SetSessionCreationError(std::wstring message) {
    if (impl_ && impl_->new_session_overlay) {
        impl_->new_session_overlay->submitting = false;
        impl_->new_session_overlay->error = std::move(message);
    }
}

// Remplace le workspace du formulaire apres le picker natif.
void DeckRenderer::SetNewSessionWorkspacePath(std::filesystem::path workspace) {
    if (impl_ && impl_->new_session_overlay) {
        SetNewSessionWorkspace(*impl_->new_session_overlay, std::move(workspace));
        impl_->new_session_overlay->active_field = 1;
        impl_->new_session_overlay->error.clear();
    }
}

// Ouvre l'editeur d'un nouveau projet apres choix de sa racine.
void DeckRenderer::OpenNewProjectEditor(std::filesystem::path root) {
    if (impl_ == nullptr) {
        return;
    }
    std::wstring name = root.filename().wstring();
    if (name.empty()) {
        name = root.root_name().wstring();
    }
    impl_->project_editor = ProjectEditorModel{
        ProjectEditorMode::Create,
        std::nullopt,
        std::move(name),
        std::move(root),
        0,
        false,
        {},
    };
}

// Selectionne et deploie un projet cree depuis le worker stockage.
void DeckRenderer::SelectProject(ProjectId project_id) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->project_editor.reset();
    impl_->ExecuteCommand(DeckCommand{DeckCommandKind::OpenWorkspace, project_id, std::nullopt});
}

// Affiche une erreur de gestion de projet dans l'overlay courant.
void DeckRenderer::SetProjectEditorError(std::wstring message) {
    if (impl_ && impl_->project_editor) {
        impl_->project_editor->submitting = false;
        impl_->project_editor->error = std::move(message);
    }
}

// Ferme l'editeur apres une mutation terminee sans selection cible.
void DeckRenderer::CloseProjectEditor() {
    if (impl_) {
        impl_->project_editor.reset();
    }
}

// Publie les threads archives charges a la demande.
void DeckRenderer::SetArchiveThreads(std::vector<CodexThreadSummary> threads) {
    if (impl_ == nullptr) {
        return;
    }
    impl_->archive_threads = std::move(threads);
    impl_->active_filter = SessionFilter::Archived;
    impl_->selected_tree_row = 0;
    impl_->tree_scroll.offset = 0.0F;
    impl_->RebuildTreeRows();
}

// ----------------------------------------------------------------------------
// Initialise les factories et les ressources liees a la fenetre.
// ----------------------------------------------------------------------------
bool DeckRenderer::Initialize(HWND hwnd) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->render_window.store(hwnd);
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
    impl_->ApplyPendingCommandPaletteResult();
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
    impl_->viewport_size = size;
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
    if (impl_->glass_settings.enabled) {
        if (const WidgetGlassEffectFrame* frame = impl_->glass_effect.LatestFrame()) {
            GlassEffectRenderProfile profile = GlassEffectProfileForAppearance(impl_->glass_settings.effect.appearance);
            profile = ApplyUserOpacityToGlassEffectProfile(
                profile,
                static_cast<double>(impl_->glass_settings.opacity_percent) / 100.0
            );
            profile = ApplyAnimationsToGlassEffectProfile(profile, impl_->glass_settings.effect);
            std::vector<WidgetGlassEffectLens> lenses;
            if (impl_->glass_settings.effect.appearance.element_glass_enabled) {
                AppendWidgetGlassElementLens(lenses, ToD2DRect(layout.workbench), profile, kNavigationCornerRadius);
            }
            const WidgetGlassEffectAnimationSettings animation = GlassEffectAnimationSettingsForSettings(
                impl_->glass_settings.effect,
                GlassEffectAnimationTimeSeconds()
            );
            impl_->glass_cache.DrawBackground(
                context,
                impl_->background_brush.Get(),
                *frame,
                ToD2DRect(layout.workbench),
                size,
                palette,
                lenses,
                profile,
                animation
            );
        }
    }
    impl_->tree_scroll.viewport_extent = std::max(0.0F, layout.tree.bottom - layout.tree.top);
    impl_->tree_scroll.content_extent = static_cast<float>(impl_->tree_rows.size()) * impl_->tree_view.RowHeight();
    impl_->tree_scroll.Clamp();
    ActivityCounts counts = BuildActivityCounts(impl_->render_catalog.sessions);
    counts.archived = impl_->archive_threads.size();
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
        impl_->tree_indent_guide_opacity,
        impl_->title_marquee_animations,
        palette
    );
    const float splitter_center = (layout.splitter.left + layout.splitter.right) * 0.5F;
    impl_->subtitle_brush->SetOpacity(0.52F);
    DrawRoundedTreeFrame(
        context,
        D2D1::RectF(layout.tree.left, layout.tree.top, splitter_center, layout.tree.bottom),
        kNavigationCornerRadius,
        impl_->background_brush.Get(),
        impl_->subtitle_brush.Get(),
        impl_->rounded_tree_frame
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
            impl_->command_palette_cursor,
            impl_->command_palette_mode,
            (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - impl_->command_palette_caret_epoch
            ).count() / 500) % 2 == 0,
            palette
        );
    }
    if (impl_->new_session_overlay) {
        impl_->new_session_view.Render(
            context,
            D2D1::RectF(0.0F, 0.0F, size.width, size.height),
            *impl_->new_session_overlay,
            palette
        );
    }
    if (impl_->project_editor) {
        impl_->project_editor_view.Render(
            context,
            D2D1::RectF(0.0F, 0.0F, size.width, size.height),
            *impl_->project_editor,
            palette
        );
    }
    if (!impl_->composition.EndDraw().has_value()) {
        DiscardDeviceResources();
    }
}

// Remplace les reglages Glass appliques au Workbench.
void DeckRenderer::SetGlassSettings(const DeckGlassSettings& settings) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->glass_settings = settings;
    impl_->glass_cache.Discard();
}

// Capture une nouvelle image du bureau hors du chemin de peinture.
bool DeckRenderer::TickGlassCapture(HWND hwnd) {
    if (impl_ == nullptr || !impl_->glass_settings.enabled) {
        return false;
    }
    return impl_->glass_effect.EnsureInitialized(hwnd) && impl_->glass_effect.TickCapture();
}

// Libere le runtime et les caches Glass.
void DeckRenderer::ShutdownGlass() {
    if (impl_ == nullptr) {
        return;
    }
    impl_->glass_cache.Discard();
    impl_->glass_effect.Shutdown();
}

// ----------------------------------------------------------------------------
// Traite une molette verticale pour la zone Tree.
// ----------------------------------------------------------------------------
void DeckRenderer::OnMouseWheel(int delta) {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->command_palette_open && !impl_->command_palette_entries.empty()) {
        constexpr std::size_t kPaletteWheelStep = 3;
        if (delta > 0) {
            impl_->command_palette_selection = impl_->command_palette_selection > kPaletteWheelStep
                ? impl_->command_palette_selection - kPaletteWheelStep
                : 0;
        } else if (delta < 0) {
            impl_->command_palette_selection = std::min(
                impl_->command_palette_selection + kPaletteWheelStep,
                impl_->command_palette_entries.size() - 1
            );
        }
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
    if (impl_->project_editor) {
        const D2D1_RECT_F bounds = D2D1::RectF(0.0F, 0.0F, impl_->viewport_size.width, impl_->viewport_size.height);
        ProjectEditorModel& editor = *impl_->project_editor;
        const ProjectEditorHitTarget target = impl_->project_editor_view.HitTest(bounds, editor, x, y);
        if (target == ProjectEditorHitTarget::Cancel) {
            impl_->project_editor.reset();
        } else if (target == ProjectEditorHitTarget::Name) {
            editor.active_field = 0;
        } else if (target == ProjectEditorHitTarget::Submit && !editor.submitting) {
            if (editor.mode != ProjectEditorMode::ConfirmDelete && editor.name.empty()) {
                editor.error = L"Enter a project name";
            } else {
                DeckCommand command{};
                command.kind = editor.mode == ProjectEditorMode::Create
                    ? DeckCommandKind::NewProject
                    : editor.mode == ProjectEditorMode::Rename
                        ? DeckCommandKind::RenameProject
                        : DeckCommandKind::DeleteProject;
                command.project_id = editor.project_id;
                command.cwd = editor.root;
                if (editor.mode != ProjectEditorMode::ConfirmDelete) {
                    command.display_name = NarrowUtf8(editor.name);
                }
                editor.submitting = true;
                if (impl_->command_handler) {
                    impl_->command_handler(std::move(command));
                }
            }
        }
        return;
    }
    if (impl_->new_session_overlay) {
        const D2D1_RECT_F bounds = D2D1::RectF(0.0F, 0.0F, impl_->viewport_size.width, impl_->viewport_size.height);
        const NewSessionOverlayHitTarget target = impl_->new_session_view.HitTest(bounds, x, y);
        NewSessionOverlayModel& overlay = *impl_->new_session_overlay;
        switch (target) {
        case NewSessionOverlayHitTarget::Project:
            overlay.active_field = 0;
            break;
        case NewSessionOverlayHitTarget::Workspace:
            overlay.active_field = 1;
            break;
        case NewSessionOverlayHitTarget::Browse:
            if (impl_->command_handler) {
                impl_->command_handler(DeckCommand{DeckCommandKind::PickSessionWorkspace});
            }
            break;
        case NewSessionOverlayHitTarget::Model:
            overlay.active_field = 2;
            break;
        case NewSessionOverlayHitTarget::Prompt:
            overlay.active_field = 3;
            break;
        case NewSessionOverlayHitTarget::Cancel:
            impl_->new_session_overlay.reset();
            break;
        case NewSessionOverlayHitTarget::Create:
            if (overlay.workspace.empty()) {
                overlay.error = L"Choose a workspace";
                overlay.active_field = 1;
            } else if (!overlay.submitting) {
                DeckCommand create{DeckCommandKind::NewSession, overlay.project_id, std::nullopt};
                create.cwd = overlay.workspace;
                create.model = overlay.model;
                if (!overlay.prompt.empty()) {
                    create.initial_prompt = NarrowUtf8(overlay.prompt);
                }
                overlay.submitting = true;
                if (impl_->command_handler) {
                    impl_->command_handler(std::move(create));
                }
            }
            break;
        case NewSessionOverlayHitTarget::None:
            break;
        }
        return;
    }
    if (impl_->command_palette_open) {
        const D2D1_RECT_F bounds = D2D1::RectF(0.0F, 0.0F, impl_->viewport_size.width, impl_->viewport_size.height);
        if (const auto hit = impl_->command_palette_view.HitTestEntry(
                bounds,
                impl_->command_palette_entries.size(),
                impl_->command_palette_selection,
                x,
                y
            )) {
            impl_->command_palette_selection = *hit;
            const DeckCommand command = impl_->command_palette_entries[*hit].command;
            impl_->command_palette_open = false;
            impl_->ExecuteCommand(command);
        } else if (!impl_->command_palette_view.Contains(bounds, impl_->command_palette_entries.size(), x, y)) {
            impl_->command_palette_open = false;
        } else if (impl_->command_palette_view.IsPointOnQuery(
                       bounds,
                       impl_->command_palette_entries.size(),
                       x,
                       y
                   )) {
            impl_->command_palette_cursor = impl_->command_palette_query.size();
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
        }
        return;
    }
    if (y < kActivityBarHeight) {
        if (const auto filter = ActivityFilterAt(x)) {
            impl_->active_filter = ToggleFilter(impl_->active_filter, *filter);
            impl_->selected_tree_row = 0;
            impl_->tree_scroll.offset = 0.0F;
            impl_->RebuildTreeRows();
            if (*filter == SessionFilter::Archived && impl_->active_filter == SessionFilter::Archived && impl_->command_handler) {
                impl_->command_handler(DeckCommand{DeckCommandKind::OpenArchive});
            }
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
    const TreeRow clicked_row = impl_->tree_rows[*hit];
    if (impl_->active_filter == SessionFilter::Archived
        && clicked_row.kind == TreeRowKind::Session
        && clicked_row.thread_id) {
        DeckCommand restore{DeckCommandKind::RestoreArchivedThread, std::nullopt, clicked_row.thread_id};
        if (impl_->command_handler) {
            impl_->command_handler(std::move(restore));
        }
        return;
    }
    if (clicked_row.kind == TreeRowKind::Project
        && clicked_row.project_id
        && x >= tree_bounds.right - 38.0F) {
        DeckCommand create{DeckCommandKind::NewSessionInCurrentProject, clicked_row.project_id, std::nullopt};
        impl_->ExecuteCommand(create);
        return;
    }
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

// Indique si un point touche le champ de saisie de la palette ouverte.
bool DeckRenderer::IsPointOnTextInput(float x, float y) const {
    if (impl_ == nullptr) {
        return false;
    }
    const D2D1_RECT_F bounds = D2D1::RectF(0.0F, 0.0F, impl_->viewport_size.width, impl_->viewport_size.height);
    if (impl_->project_editor) {
        return impl_->project_editor_view.HitTest(bounds, *impl_->project_editor, x, y) == ProjectEditorHitTarget::Name;
    }
    if (impl_->new_session_overlay) {
        const NewSessionOverlayHitTarget target = impl_->new_session_view.HitTest(bounds, x, y);
        return target == NewSessionOverlayHitTarget::Workspace || target == NewSessionOverlayHitTarget::Prompt;
    }
    if (!impl_->command_palette_open) {
        return false;
    }
    return impl_->command_palette_view.IsPointOnQuery(
        bounds,
        impl_->command_palette_entries.size(),
        x,
        y
    );
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
    const float previous_indent_guide_opacity = impl_->tree_indent_guide_opacity;
    impl_->tree_indent_guide_opacity = ApproachVisualValue(
        impl_->tree_indent_guide_opacity,
        impl_->tree_hovered ? 1.0F : 0.0F,
        kIndentGuideFadeStep
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
        return HasActiveGlassEffectAnimation(impl_->glass_settings.effect)
            || impl_->command_palette_open || impl_->tree_scrollbar_opacity > 0.0F
            || previous_scrollbar_width != impl_->tree_scrollbar_width
            || previous_indent_guide_opacity != impl_->tree_indent_guide_opacity
            || marquee_animating;
    }
    return HasActiveGlassEffectAnimation(impl_->glass_settings.effect)
        || impl_->tree_hovered
        || impl_->resizing_tree
        || impl_->dragging_tree_scrollbar
        || impl_->tree_scrollbar_opacity > 0.0F
        || previous_scrollbar_width != impl_->tree_scrollbar_width
        || previous_indent_guide_opacity != impl_->tree_indent_guide_opacity
        || marquee_animating
        || impl_->command_palette_open;
}

// ----------------------------------------------------------------------------
// Traite une touche clavier de navigation Tree.
// ----------------------------------------------------------------------------
bool DeckRenderer::OnKeyDown(WPARAM virtual_key) {
    if (impl_ == nullptr) {
        return false;
    }
    impl_->ApplyPendingCommandPaletteResult();

    if (impl_->project_editor) {
        ProjectEditorModel& editor = *impl_->project_editor;
        if (virtual_key == VK_ESCAPE) {
            impl_->project_editor.reset();
            return true;
        }
        if (editor.mode != ProjectEditorMode::ConfirmDelete && virtual_key == VK_BACK) {
            if (!editor.name.empty()) {
                editor.name.pop_back();
            }
            editor.error.clear();
            return true;
        }
        if (virtual_key == VK_RETURN && !editor.submitting) {
            if (editor.mode != ProjectEditorMode::ConfirmDelete && editor.name.empty()) {
                editor.error = L"Enter a project name";
                return true;
            }
            DeckCommand command{};
            command.kind = editor.mode == ProjectEditorMode::Create
                ? DeckCommandKind::NewProject
                : editor.mode == ProjectEditorMode::Rename
                    ? DeckCommandKind::RenameProject
                    : DeckCommandKind::DeleteProject;
            command.project_id = editor.project_id;
            command.cwd = editor.root;
            if (editor.mode != ProjectEditorMode::ConfirmDelete) {
                command.display_name = NarrowUtf8(editor.name);
            }
            editor.submitting = true;
            if (impl_->command_handler) {
                impl_->command_handler(std::move(command));
            }
            return true;
        }
        return true;
    }

    if (impl_->new_session_overlay) {
        NewSessionOverlayModel& overlay = *impl_->new_session_overlay;
        if (virtual_key == VK_ESCAPE) {
            impl_->new_session_overlay.reset();
            return true;
        }
        if (virtual_key == VK_TAB) {
            overlay.active_field = (overlay.active_field + (IsShiftDown() ? 3 : 1)) % 4;
            return true;
        }
        if ((virtual_key == VK_UP || virtual_key == VK_DOWN) && overlay.active_field == 0) {
            std::vector<std::optional<ProjectId>> choices{std::nullopt};
            for (const Project& project : overlay.projects) {
                choices.push_back(project.id);
            }
            const auto current = std::ranges::find(choices, overlay.project_id);
            std::size_t index = current == choices.end() ? 0 : static_cast<std::size_t>(std::distance(choices.begin(), current));
            index = virtual_key == VK_DOWN ? (index + 1) % choices.size() : (index + choices.size() - 1) % choices.size();
            SelectNewSessionProject(overlay, choices[index]);
            return true;
        }
        if ((virtual_key == VK_UP || virtual_key == VK_DOWN) && overlay.active_field == 2 && !overlay.models.empty()) {
            std::vector<std::optional<std::string>> choices{std::nullopt};
            for (const CodexModelInfo& model : overlay.models) {
                choices.push_back(model.id);
            }
            const auto current = std::ranges::find(choices, overlay.model);
            std::size_t index = current == choices.end() ? 0 : static_cast<std::size_t>(std::distance(choices.begin(), current));
            index = virtual_key == VK_DOWN ? (index + 1) % choices.size() : (index + choices.size() - 1) % choices.size();
            overlay.model = choices[index];
            return true;
        }
        if (virtual_key == VK_BACK && (overlay.active_field == 1 || overlay.active_field == 3)) {
            if (overlay.active_field == 1 && !overlay.workspace.empty()) {
                std::wstring value = overlay.workspace.wstring();
                value.pop_back();
                SetNewSessionWorkspace(overlay, value);
            } else if (overlay.active_field == 3 && !overlay.prompt.empty()) {
                overlay.prompt.pop_back();
            }
            return true;
        }
        if (virtual_key == VK_RETURN && !overlay.submitting) {
            if (overlay.workspace.empty()) {
                overlay.error = L"Choose a workspace";
                overlay.active_field = 1;
                return true;
            }
            DeckCommand create{DeckCommandKind::NewSession, overlay.project_id, std::nullopt};
            create.cwd = overlay.workspace;
            create.model = overlay.model;
            if (!overlay.prompt.empty()) {
                create.initial_prompt = NarrowUtf8(overlay.prompt);
            }
            overlay.submitting = true;
            if (impl_->command_handler) {
                impl_->command_handler(std::move(create));
            }
            return true;
        }
        return true;
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
        impl_->ExecuteCommand(DeckCommand{*command, std::nullopt, std::nullopt});
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
            if (impl_->command_palette_cursor > 0) {
                impl_->command_palette_query.erase(impl_->command_palette_cursor - 1, 1);
                --impl_->command_palette_cursor;
                impl_->RebuildCommandPalette();
            }
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
            return true;
        case VK_DELETE:
            if (impl_->command_palette_cursor < impl_->command_palette_query.size()) {
                impl_->command_palette_query.erase(impl_->command_palette_cursor, 1);
                impl_->RebuildCommandPalette();
            }
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
            return true;
        case VK_LEFT:
            if (impl_->command_palette_cursor > 0) {
                --impl_->command_palette_cursor;
            }
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
            return true;
        case VK_RIGHT:
            if (impl_->command_palette_cursor < impl_->command_palette_query.size()) {
                ++impl_->command_palette_cursor;
            }
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
            return true;
        case VK_HOME:
            impl_->command_palette_cursor = 0;
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
            return true;
        case VK_END:
            impl_->command_palette_cursor = impl_->command_palette_query.size();
            impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
            return true;
        case VK_RETURN:
            if (!impl_->command_palette_entries.empty()
                && impl_->command_palette_selection < impl_->command_palette_entries.size()) {
                const DeckCommand command = impl_->command_palette_entries[impl_->command_palette_selection].command;
                impl_->command_palette_open = false;
                impl_->ExecuteCommand(command);
                return true;
            }
            impl_->command_palette_open = false;
            return true;
        default:
            return true;
        }
    }

    if (impl_->tree_rows.empty()) {
        return false;
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
        if (virtual_key == VK_RETURN
            && impl_->active_filter == SessionFilter::Archived
            && impl_->tree_rows[impl_->selected_tree_row].thread_id) {
            DeckCommand restore{
                DeckCommandKind::RestoreArchivedThread,
                std::nullopt,
                impl_->tree_rows[impl_->selected_tree_row].thread_id,
            };
            if (impl_->command_handler) {
                impl_->command_handler(std::move(restore));
            }
            return true;
        }
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
    if (impl_->project_editor) {
        ProjectEditorModel& editor = *impl_->project_editor;
        if (editor.mode == ProjectEditorMode::ConfirmDelete
            || character == L'\b' || character == L'\r' || character == L'\n' || character == L'\t'
            || std::iswcntrl(character) != 0) {
            return true;
        }
        editor.name.push_back(character);
        editor.error.clear();
        return true;
    }
    if (impl_->new_session_overlay) {
        if (character == L'\b' || character == L'\r' || character == L'\n' || character == L'\t'
            || std::iswcntrl(character) != 0) {
            return true;
        }
        NewSessionOverlayModel& overlay = *impl_->new_session_overlay;
        if (overlay.active_field == 1) {
            std::wstring value = overlay.workspace.wstring();
            value.push_back(character);
            SetNewSessionWorkspace(overlay, value);
        } else if (overlay.active_field == 3) {
            overlay.prompt.push_back(character);
        }
        return true;
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
    impl_->command_palette_query.insert(impl_->command_palette_cursor, 1, character);
    ++impl_->command_palette_cursor;
    impl_->command_palette_caret_epoch = std::chrono::steady_clock::now();
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
    impl_->glass_cache.Discard();
}

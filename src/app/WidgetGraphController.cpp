// ============================================================================
// Codex Glass - Controleur d'interaction des graphes
// ----------------------------------------------------------------------------
// Ce fichier convertit les coordonnees Win32 en DIPs et orchestre onglets,
// survol et persistance sans placer cette logique dans le renderer.
// ============================================================================

#include "WidgetApp.h"

#include "../localization/Localization.h"
#include "../menu/WidgetMenuOwnerDraw.h"
#include "../rendering/WidgetGraphFormatting.h"
#include "../rendering/WidgetGraphLayout.h"
#include "../rendering/WidgetRenderConstants.h"
#include "../rendering/WidgetRenderQuotaGraph.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

// Premier identifiant local utilise par le popup des plages.
constexpr UINT kGraphRangePopupFirstCommand = 1;

// ----------------------------------------------------------------------------
// Convertit une coordonnee physique Win32 en DIPs pour la fenetre courante.
//
// Parametres :
// - hwnd : fenetre dont le DPI doit etre utilise.
// - value : coordonnee ou dimension exprimee en pixels physiques.
//
// Retour :
// - valeur convertie dans le repere Direct2D de la fenetre.
// ----------------------------------------------------------------------------
float PixelsToDips(HWND hwnd, int value) {
    const UINT window_dpi = GetDpiForWindow(hwnd);
    const float dpi = window_dpi == 0 ? kReferenceDpi : static_cast<float>(window_dpi);
    return static_cast<float>(value) * kReferenceDpi / dpi;
}

// ----------------------------------------------------------------------------
// Convertit une coordonnee en DIPs vers les pixels physiques de la fenetre.
//
// Parametres :
// - hwnd : fenetre qui fournit le DPI courant.
// - value : coordonnee ou dimension exprimee en DIPs.
//
// Retour :
// - valeur arrondie en pixels physiques.
// ----------------------------------------------------------------------------
int DipsToPixels(HWND hwnd, float value) {
    const UINT window_dpi = GetDpiForWindow(hwnd);
    const float dpi = window_dpi == 0 ? kReferenceDpi : static_cast<float>(window_dpi);
    return static_cast<int>(std::lround(value * dpi / kReferenceDpi));
}

// ----------------------------------------------------------------------------
// Retourne la plage propre a la page graphique active.
//
// Parametres :
// - settings : reglages contenant la page et ses plages persistantes.
//
// Retour :
// - plage associee a la page active.
// ----------------------------------------------------------------------------
GraphRange ActiveGraphRange(const AppSettings& settings) {
    if (settings.graph_page == WidgetGraphPage::Tokens) {
        return settings.token_graph_range;
    }
    if (settings.graph_page == WidgetGraphPage::Summary) {
        return settings.summary_graph_range;
    }
    if (settings.graph_page == WidgetGraphPage::Activity) {
        return settings.activity_graph_range;
    }
    return settings.graph_range;
}

// ----------------------------------------------------------------------------
// Retourne le libelle de plage adapte a la semantique de la page active.
// ----------------------------------------------------------------------------
std::wstring GraphRangeLabelForPage(WidgetGraphPage page, GraphRange range) {
    if (page == WidgetGraphPage::Activity && range == GraphRange::Days30) {
        return T(IDS_GRAPH_ACTIVITY_RANGE_DAYS);
    }
    return FormatGraphRangeLabel(range);
}

// ----------------------------------------------------------------------------
// Retourne les plages pertinentes pour une page graphique.
//
// Parametres :
// - page : vue dont le selecteur doit etre alimente.
//
// Retour :
// - choix ordonnes du plus court au plus long.
// ----------------------------------------------------------------------------
std::vector<GraphRange> GraphRangesForPage(WidgetGraphPage page) {
    if (page == WidgetGraphPage::Tokens) {
        return {GraphRange::Hours1, GraphRange::Hours5, GraphRange::Hours24};
    }
    if (page == WidgetGraphPage::Summary) {
        return {
            GraphRange::Hours1,
            GraphRange::Hours5,
            GraphRange::Hours24,
            GraphRange::Days7,
            GraphRange::Days30,
        };
    }
    if (page == WidgetGraphPage::Activity) {
        return {GraphRange::Days30, GraphRange::Minutes5};
    }
    return {
        GraphRange::Minutes5,
        GraphRange::Hours1,
        GraphRange::Hours5,
        GraphRange::Hours24,
        GraphRange::Days7,
        GraphRange::Days30,
    };
}

// ----------------------------------------------------------------------------
// Affiche le popup du selecteur et retourne la plage choisie.
//
// Parametres :
// - hwnd : fenetre proprietaire du popup.
// - layout : geometrie utilisee pour positionner le popup sous le controle.
// - settings : page et plage actuellement selectionnees.
//
// Retour :
// - plage choisie, ou aucune valeur lorsque le popup est annule.
// ----------------------------------------------------------------------------
std::optional<GraphRange> ShowGraphRangePopup(
    HWND hwnd,
    const WidgetGraphLayout& layout,
    const AppSettings& settings
) {
    const std::vector<GraphRange> ranges = GraphRangesForPage(settings.graph_page);
    const GraphRange selected_range = ActiveGraphRange(settings);
    ResetOwnerMenuDrawState();
    HMENU menu = CreateOwnerPopupMenu();
    if (menu == nullptr) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        AppendCompactCommandMenuItem(
            menu,
            kGraphRangePopupFirstCommand + static_cast<UINT>(index),
            GraphRangeLabelForPage(settings.graph_page, ranges[index]).c_str(),
            ranges[index] == selected_range
        );
    }

    POINT position{
        DipsToPixels(hwnd, layout.range_selector_rect.left),
        DipsToPixels(hwnd, layout.range_selector_rect.bottom + 2.0F),
    };
    ClientToScreen(hwnd, &position);
    SetForegroundWindow(hwnd);
    BeginOwnerMenuWindowPolish();
    const UINT command = TrackPopupMenuEx(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        position.x,
        position.y,
        hwnd,
        nullptr
    );
    EndOwnerMenuWindowPolish();
    DestroyMenu(menu);
    ResetOwnerMenuDrawState();
    if (command < kGraphRangePopupFirstCommand
        || command >= kGraphRangePopupFirstCommand + ranges.size()) {
        return std::nullopt;
    }
    return ranges[command - kGraphRangePopupFirstCommand];
}

// ----------------------------------------------------------------------------
// Construit la geometrie courante du graphe a partir de la zone cliente.
// ----------------------------------------------------------------------------
WidgetGraphLayout CurrentGraphLayout(
    HWND hwnd,
    const AppSettings& settings,
    const UsageSnapshot& usage_snapshot,
    const TokenUsageSnapshot& token_snapshot,
    const WidgetRenderer& renderer
) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const std::size_t token_bar_count = settings.graph_page == WidgetGraphPage::Activity
            && settings.activity_graph_range == GraphRange::Minutes5
        ? token_snapshot.five_minute.size()
        : (settings.graph_page == WidgetGraphPage::Tokens
        ? std::min(
            settings.token_graph_range == GraphRange::Hours1
                ? token_snapshot.five_minute.size()
                : token_snapshot.hourly.size(),
            TokenGraphBucketCount(settings.token_graph_range)
        )
        : 0U);
    const std::wstring graph_title = settings.graph_page == WidgetGraphPage::Tokens
        ? T(IDS_GRAPH_TOKENS_TITLE)
        : (settings.graph_page == WidgetGraphPage::Activity
            ? T(IDS_GRAPH_ACTIVITY_TITLE)
            : (settings.graph_page == WidgetGraphPage::Summary
                ? T(IDS_GRAPH_SUMMARY_TITLE)
                : T(IDS_GRAPH_TAB_QUOTAS)));
    return BuildWidgetGraphLayout(
        D2D1::SizeF(
            PixelsToDips(hwnd, client.right - client.left),
            PixelsToDips(hwnd, client.bottom - client.top)
        ),
        settings,
        usage_snapshot,
        token_bar_count,
        renderer.MeasureGraphTitleTextWidth(graph_title),
        renderer.MeasureCaptionTextWidth(T(IDS_GRAPH_CAPTURE_HINT))
    );
}

// ----------------------------------------------------------------------------
// Convertit un point client en coordonnees independantes du DPI.
// ----------------------------------------------------------------------------
D2D1_POINT_2F ClientPointToDips(HWND hwnd, POINT point) {
    return D2D1::Point2F(
        PixelsToDips(hwnd, point.x),
        PixelsToDips(hwnd, point.y)
    );
}

// ----------------------------------------------------------------------------
// Compare deux positions optionnelles sans dependre d'un operateur Direct2D.
//
// Parametres :
// - first : premiere position optionnelle.
// - second : seconde position optionnelle.
//
// Retour :
// - true si les deux positions sont absentes ou identiques.
// ----------------------------------------------------------------------------
bool SameOptionalPoint(
    const std::optional<D2D1_POINT_2F>& first,
    const std::optional<D2D1_POINT_2F>& second
) {
    if (first.has_value() != second.has_value()) {
        return false;
    }
    return !first.has_value()
        || (first->x == second->x && first->y == second->y);
}

} // namespace

// ----------------------------------------------------------------------------
// Indique si le point client appartient aux controles du graphe.
// ----------------------------------------------------------------------------
bool WidgetApp::IsGraphInteractivePoint(HWND hwnd, POINT client_point) const {
    if (app_settings_.click_through
        || app_settings_.display_mode != WidgetDisplayMode::Complete
        || !app_settings_.show_graph) {
        return false;
    }
    const WidgetGraphLayout layout = CurrentGraphLayout(
        hwnd, app_settings_, usage_snapshot_, token_usage_snapshot_, widget_renderer_
    );
    const D2D1_POINT_2F point = ClientPointToDips(hwnd, client_point);
    return HitTestWidgetGraphTab(layout, point).has_value()
        || HitTestWidgetGraphInfo(layout, point)
        || HitTestWidgetGraphCaptureButton(layout, point)
        || HitTestWidgetGraphRangeSelector(layout, point);
}

// ----------------------------------------------------------------------------
// Met a jour les segments et les donnees de graphe survoles puis arme WM_MOUSELEAVE.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateGraphHover(HWND hwnd, POINT client_point, bool non_client) {
    std::optional<WidgetGraphPage> hovered_tab;
    std::optional<std::size_t> hovered_bar;
    std::optional<std::size_t> hovered_heatmap_cell;
    std::optional<D2D1_POINT_2F> hovered_quota_point;
    bool hovered_info = false;
    bool hovered_range_selector = false;
    bool hovered_capture_button = false;
    const bool graph_enabled = !app_settings_.click_through
        && app_settings_.display_mode == WidgetDisplayMode::Complete
        && app_settings_.show_graph;
    if (graph_enabled) {
        const WidgetGraphLayout layout = CurrentGraphLayout(
            hwnd, app_settings_, usage_snapshot_, token_usage_snapshot_, widget_renderer_
        );
        const D2D1_POINT_2F point = ClientPointToDips(hwnd, client_point);
        hovered_tab = HitTestWidgetGraphTab(layout, point);
        hovered_info = HitTestWidgetGraphInfo(layout, point);
        hovered_capture_button = HitTestWidgetGraphCaptureButton(layout, point);
        hovered_range_selector = HitTestWidgetGraphRangeSelector(layout, point);
        if (app_settings_.graph_page == WidgetGraphPage::Tokens
            && !hovered_info && !hovered_range_selector && !hovered_capture_button) {
            hovered_bar = HitTestWidgetTokenBar(layout, point);
        } else if (app_settings_.graph_page == WidgetGraphPage::Activity
            && !hovered_info && !hovered_range_selector && !hovered_capture_button) {
            hovered_heatmap_cell = HitTestWidgetHeatmapCell(layout, point);
        } else if (app_settings_.graph_page == WidgetGraphPage::Quotas
            && !hovered_info && !hovered_range_selector && !hovered_capture_button) {
            hovered_quota_point = HitTestWidgetQuotaPlot(layout, point);
        }
    }

    const bool has_hover = hovered_tab.has_value() || hovered_bar.has_value()
        || hovered_heatmap_cell.has_value()
        || hovered_quota_point.has_value() || hovered_info || hovered_range_selector
        || hovered_capture_button;
    if (has_hover && (!graph_interaction_.tracking_mouse_leave
            || graph_interaction_.tracking_non_client_leave != non_client)) {
        if (graph_interaction_.tracking_mouse_leave) {
            TRACKMOUSEEVENT cancel{
                sizeof(cancel),
                TME_CANCEL | TME_LEAVE
                    | (graph_interaction_.tracking_non_client_leave ? TME_NONCLIENT : 0U),
                hwnd,
                0
            };
            TrackMouseEvent(&cancel);
        }
        TRACKMOUSEEVENT tracking{
            sizeof(tracking),
            TME_LEAVE | (non_client ? TME_NONCLIENT : 0U),
            hwnd,
            0
        };
        graph_interaction_.tracking_mouse_leave = TrackMouseEvent(&tracking) != FALSE;
        graph_interaction_.tracking_non_client_leave = non_client
            && graph_interaction_.tracking_mouse_leave;
    }
    if (hovered_tab != graph_interaction_.hovered_graph_tab
        || hovered_bar != graph_interaction_.hovered_token_bar
        || hovered_heatmap_cell != graph_interaction_.hovered_heatmap_cell
        || !SameOptionalPoint(hovered_quota_point, graph_interaction_.hovered_quota_plot_point)
        || hovered_info != graph_interaction_.hovered_graph_info
        || hovered_range_selector != graph_interaction_.hovered_graph_range_selector
        || hovered_capture_button != graph_interaction_.hovered_graph_capture_button) {
        graph_interaction_.hovered_graph_tab = hovered_tab;
        graph_interaction_.hovered_token_bar = hovered_bar;
        graph_interaction_.hovered_heatmap_cell = hovered_heatmap_cell;
        graph_interaction_.hovered_quota_plot_point = hovered_quota_point;
        graph_interaction_.hovered_graph_info = hovered_info;
        graph_interaction_.hovered_graph_range_selector = hovered_range_selector;
        graph_interaction_.hovered_graph_capture_button = hovered_capture_button;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ----------------------------------------------------------------------------
// Efface les survols devenus invalides sans annuler un clic deja capture.
//
// Parametres :
// - hwnd : fenetre a invalider si l'etat visuel change.
//
// Effets de bord :
// - annule le suivi WM_MOUSELEAVE courant et redessine le widget au besoin.
// ----------------------------------------------------------------------------
void WidgetApp::ClearGraphHover(HWND hwnd) {
    const bool changed = graph_interaction_.hovered_graph_tab.has_value()
        || graph_interaction_.hovered_token_bar.has_value()
        || graph_interaction_.hovered_heatmap_cell.has_value()
        || graph_interaction_.hovered_quota_plot_point.has_value()
        || graph_interaction_.hovered_graph_info
        || graph_interaction_.hovered_graph_range_selector
        || graph_interaction_.hovered_graph_capture_button;
    if (graph_interaction_.tracking_mouse_leave) {
        TRACKMOUSEEVENT tracking{
            sizeof(tracking),
            TME_CANCEL | TME_LEAVE
                | (graph_interaction_.tracking_non_client_leave ? TME_NONCLIENT : 0U),
            hwnd,
            0
        };
        TrackMouseEvent(&tracking);
    }
    graph_interaction_.hovered_graph_tab.reset();
    graph_interaction_.hovered_token_bar.reset();
    graph_interaction_.hovered_heatmap_cell.reset();
    graph_interaction_.hovered_quota_plot_point.reset();
    graph_interaction_.hovered_graph_info = false;
    graph_interaction_.hovered_graph_range_selector = false;
    graph_interaction_.hovered_graph_capture_button = false;
    graph_interaction_.tracking_mouse_leave = false;
    graph_interaction_.tracking_non_client_leave = false;
    if (changed) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ----------------------------------------------------------------------------
// Traite l'enfoncement du bouton gauche dans la zone de graphe.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleGraphClick(HWND hwnd, POINT client_point) {
    if (!IsGraphInteractivePoint(hwnd, client_point)) {
        return false;
    }
    const WidgetGraphLayout layout = CurrentGraphLayout(
        hwnd, app_settings_, usage_snapshot_, token_usage_snapshot_, widget_renderer_
    );
    const D2D1_POINT_2F point = ClientPointToDips(hwnd, client_point);
    if (HitTestWidgetGraphCaptureButton(layout, point)) {
        graph_interaction_.pressed_graph_capture_button = true;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }
    if (HitTestWidgetGraphRangeSelector(layout, point)) {
        const std::optional<GraphRange> selected = ShowGraphRangePopup(
            hwnd,
            layout,
            app_settings_
        );
        const bool range_changed = selected.has_value()
            && *selected != ActiveGraphRange(app_settings_);
        if (range_changed) {
            if (app_settings_.graph_page == WidgetGraphPage::Tokens) {
                app_settings_.token_graph_range = *selected;
            } else if (app_settings_.graph_page == WidgetGraphPage::Activity) {
                app_settings_.activity_graph_range = *selected;
            } else if (app_settings_.graph_page == WidgetGraphPage::Summary) {
                app_settings_.summary_graph_range = *selected;
            } else {
                app_settings_.graph_range = *selected;
            }
            graph_interaction_.hovered_token_bar.reset();
            graph_interaction_.hovered_heatmap_cell.reset();
            graph_interaction_.hovered_quota_plot_point.reset();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        if (range_changed) {
            ScheduleSettingsSave(hwnd);
        }
        return true;
    }
    const std::optional<WidgetGraphPage> page = HitTestWidgetGraphTab(
        layout,
        point
    );
    if (!page.has_value()) {
        return true;
    }
    graph_interaction_.pressed_graph_tab = page;
    graph_press_client_point_ = client_point;
    SetCapture(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

// ----------------------------------------------------------------------------
// Transforme un glissement d'onglet en deplacement natif de la fenetre.
//
// Parametres :
// - hwnd : fenetre a transmettre au gestionnaire de deplacement Windows.
// - client_point : position physique courante dans la zone cliente.
//
// Retour :
// - true si le seuil systeme est franchi et le deplacement a commence.
//
// Effets de bord :
// - libere la capture du clic d'onglet puis entre dans la boucle de drag native.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleGraphDrag(HWND hwnd, POINT client_point) {
    if (!graph_interaction_.pressed_graph_tab.has_value()
        || !graph_press_client_point_.has_value()) {
        return false;
    }
    const int delta_x = std::abs(client_point.x - graph_press_client_point_->x);
    const int delta_y = std::abs(client_point.y - graph_press_client_point_->y);
    if (delta_x < GetSystemMetrics(SM_CXDRAG)
        && delta_y < GetSystemMetrics(SM_CYDRAG)) {
        return false;
    }

    graph_interaction_.pressed_graph_tab.reset();
    graph_press_client_point_.reset();
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    ClearGraphHover(hwnd);
    POINT screen_point = client_point;
    ClientToScreen(hwnd, &screen_point);
    SendMessageW(
        hwnd,
        WM_NCLBUTTONDOWN,
        HTCAPTION,
        MAKELPARAM(screen_point.x, screen_point.y)
    );
    return true;
}

// ----------------------------------------------------------------------------
// Termine un clic sur un segment et change de page si le relachement est valide.
// ----------------------------------------------------------------------------
bool WidgetApp::HandleGraphButtonRelease(HWND hwnd, POINT client_point) {
    if (graph_interaction_.pressed_graph_capture_button) {
        graph_interaction_.pressed_graph_capture_button = false;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        const WidgetGraphLayout layout = CurrentGraphLayout(
            hwnd, app_settings_, usage_snapshot_, token_usage_snapshot_, widget_renderer_
        );
        const bool released_on_button = HitTestWidgetGraphCaptureButton(
            layout,
            ClientPointToDips(hwnd, client_point)
        );
        if (released_on_button) {
            CopyActiveGraphTabToClipboard(hwnd);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }
    if (!graph_interaction_.pressed_graph_tab.has_value()) {
        return false;
    }

    const WidgetGraphPage pressed_page = *graph_interaction_.pressed_graph_tab;
    graph_interaction_.pressed_graph_tab.reset();
    graph_press_client_point_.reset();
    if (GetCapture() == hwnd) {
        ReleaseCapture();
    }
    const WidgetGraphLayout layout = CurrentGraphLayout(
        hwnd, app_settings_, usage_snapshot_, token_usage_snapshot_, widget_renderer_
    );
    const std::optional<WidgetGraphPage> released_page = HitTestWidgetGraphTab(
        layout,
        ClientPointToDips(hwnd, client_point)
    );
    const bool page_changed = released_page == pressed_page
        && pressed_page != app_settings_.graph_page;
    if (page_changed) {
        app_settings_.graph_page = pressed_page;
        graph_interaction_.hovered_token_bar.reset();
        graph_interaction_.hovered_heatmap_cell.reset();
        graph_interaction_.hovered_quota_plot_point.reset();
        graph_interaction_.hovered_graph_info = false;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    if (page_changed) {
        ScheduleSettingsSave(hwnd);
    }
    return true;
}

// ----------------------------------------------------------------------------
// Copie le contenu nettoye de l'onglet graphique actif dans le presse-papiers.
//
// Parametres :
// - hwnd : fenetre principale dont la section graphique doit etre capturee.
//
// Retour :
// - true si la capture recadree a ete publiee dans le presse-papiers.
//
// Effets de bord :
// - masque les controles d'en-tete pendant une composition DWM synchrone.
// ----------------------------------------------------------------------------
bool WidgetApp::CopyActiveGraphTabToClipboard(HWND hwnd) {
    return widget_renderer_.CopyActiveGraphTabToClipboard(
        hwnd,
        app_settings_,
        usage_snapshot_,
        token_usage_snapshot_,
        usage_history_store_,
        app_settings_.graph_range,
        quota_graph_animation_.Frame(WidgetQuotaGraphAnimation::Clock::now())
    );
}

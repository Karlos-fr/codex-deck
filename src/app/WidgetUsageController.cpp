// ============================================================================
// Codex Glass - Implementation du controle applicatif de l'usage
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp les operations de recuperation d'usage
// et de conversion d'intervalle.
// ============================================================================

#include "WidgetUsageController.h"

#include "WidgetTimers.h"

#include "../resources/ResourceIds.h"
#include "../localization/Localization.h"
#include "../shell/WidgetTrayIcon.h"
#include "../shell/WidgetTrayTooltip.h"
#include "../usage/UsageRefreshWorker.h"
#include "../window/WidgetWindow.h"

#include <chrono>

// ----------------------------------------------------------------------------
// Convertit un intervalle de recuperation en duree Win32.
//
// Parametres :
// - interval : intervalle de recuperation a convertir.
//
// Retour :
// - duree en millisecondes, ou 0 pour le mode manuel.
// ----------------------------------------------------------------------------
UINT WidgetApp::RefreshIntervalMilliseconds(UsageRefreshInterval interval) const {
    switch (interval) {
    case UsageRefreshInterval::Manual:
        return 0;

    case UsageRefreshInterval::Seconds10:
        return 10 * 1000;

    case UsageRefreshInterval::Seconds30:
        return 30 * 1000;

    case UsageRefreshInterval::Minute1:
        return 60 * 1000;

    case UsageRefreshInterval::Minutes5:
        return 5 * 60 * 1000;

    case UsageRefreshInterval::Minutes15:
        return 15 * 60 * 1000;

    default:
        return 60 * 1000;
    }
}

// ----------------------------------------------------------------------------
// Convertit un nombre de secondes en intervalle de recuperation supporte.
//
// Parametres :
// - seconds : duree sauvegardee en secondes.
//
// Retour :
// - intervalle de recuperation correspondant.
// ----------------------------------------------------------------------------
UsageRefreshInterval WidgetApp::RefreshIntervalFromSeconds(int seconds) const {
    switch (seconds) {
    case 0:
        return UsageRefreshInterval::Manual;

    case 10:
        return UsageRefreshInterval::Seconds10;

    case 30:
        return UsageRefreshInterval::Seconds30;

    case 300:
        return UsageRefreshInterval::Minutes5;

    case 900:
        return UsageRefreshInterval::Minutes15;

    case 60:
    default:
        return UsageRefreshInterval::Minute1;
    }
}

// ----------------------------------------------------------------------------
// Convertit un intervalle de recuperation en secondes persistables.
//
// Parametres :
// - interval : intervalle de recuperation courant.
//
// Retour :
// - duree en secondes, 0 pour le mode manuel.
// ----------------------------------------------------------------------------
int WidgetApp::RefreshIntervalToSeconds(UsageRefreshInterval interval) const {
    return static_cast<int>(RefreshIntervalMilliseconds(interval) / 1000);
}

// ----------------------------------------------------------------------------
// Definit l'intervalle de recuperation et redemarre le timer associe.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les timers.
// - interval : nouvel intervalle de recuperation.
// ----------------------------------------------------------------------------
void WidgetApp::SetUsageRefreshInterval(HWND hwnd, UsageRefreshInterval interval) {
    usage_refresh_interval_ = interval;
    app_settings_.refresh_interval_seconds = RefreshIntervalToSeconds(interval);
    ApplyUsageFetchTimer(hwnd);
    SaveCurrentSettings(hwnd);
}

// ----------------------------------------------------------------------------
// Recupere un nouveau snapshot d'usage en evitant les appels concurrents.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner apres la recuperation.
// ----------------------------------------------------------------------------
void WidgetApp::RefreshUsageSnapshot(HWND hwnd) {
    RefreshTokenUsageSnapshot(hwnd);

    if (usage_refresh_in_progress_) {
        return;
    }

    usage_refresh_in_progress_ = true;
    usage_refresh_started_at_ = std::chrono::steady_clock::now();
    usage_refresh_completion_pending_ = false;
    const auto refresh_wall_time = std::chrono::system_clock::now();
    quota_graph_animation_.Hold(
        refresh_wall_time,
        usage_snapshot_.sampled_at.time_since_epoch().count() != 0
            ? usage_snapshot_.sampled_at
            : refresh_wall_time
    );

    UsageSnapshot refreshing_snapshot = usage_snapshot_;
    refreshing_snapshot.freshness = UsageFreshness::Refreshing;
    usage_snapshot_ = refreshing_snapshot;
    graph_interaction_.hovered_quota_plot_point.reset();
    StartRefreshingLabelAnimation(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);

    if (!usage_refresh_worker_.Start(hwnd)) {
        usage_refresh_in_progress_ = false;
        usage_refresh_started_at_ = std::chrono::steady_clock::time_point{};
        quota_graph_animation_.Clear();
        StopRefreshingLabelAnimation(hwnd);
        usage_snapshot_.freshness = UsageFreshness::Error;
        usage_snapshot_.error_message = T(IDS_ERROR_USAGE_REFRESH);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ----------------------------------------------------------------------------
// Met a jour l'infobulle tray depuis les quotas actuellement visibles.
//
// Parametres :
// - hwnd : fenetre proprietaire de l'icone de notification.
//
// Effet de bord : demande au shell de remplacer le texte affiche au survol.
// ----------------------------------------------------------------------------
void WidgetApp::RefreshTrayQuotaTooltip(HWND hwnd) const {
    RefreshTrayIcon(
        hwnd,
        BuildWidgetTrayTooltip(usage_snapshot_, app_settings_.quota_visibility)
    );
}

// ----------------------------------------------------------------------------
// Applique sur le thread UI le resultat produit par le worker de refresh.
//
// Parametres :
// - hwnd : fenetre a redessiner et eventuellement a faire vibrer.
// ----------------------------------------------------------------------------
void WidgetApp::CompleteUsageRefresh(HWND hwnd) {
    if (!usage_refresh_in_progress_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto minimum_visible_duration = std::chrono::milliseconds{
        kRefreshingLabelMinimumVisibleMs
    };
    if (usage_refresh_started_at_ != std::chrono::steady_clock::time_point{}
        && now - usage_refresh_started_at_ < minimum_visible_duration) {
        usage_refresh_completion_pending_ = true;
        return;
    }

    const std::optional<UsageRefreshResult> result = usage_refresh_worker_.TakeResult();
    if (!result) {
        return;
    }

    const UsageSnapshot previous_snapshot = usage_snapshot_;
    if (result->failure == UsageRefreshFailure::None) {
        usage_snapshot_ = result->snapshot;
        if (previous_snapshot.sampled_at.time_since_epoch().count() != 0) {
            quota_graph_animation_.Start(
                usage_snapshot_.sampled_at,
                WidgetQuotaGraphAnimation::Clock::now()
            );
        } else {
            quota_graph_animation_.Clear();
        }
        ApplyQuotaGraphAnimationTimer(hwnd);
        ApplyPreferredWindowSize(hwnd, app_settings_, usage_snapshot_);
        PositionColorPanelWindow();
        RefreshGlassEffectFrame(hwnd);
        UpdateRollingUsageAnimations(hwnd, previous_snapshot, usage_snapshot_);

        const bool should_start_vibration = vibration_controller_.ObserveSnapshot(
            app_settings_.motion_effects,
            usage_snapshot_,
            std::chrono::system_clock::now()
        );
        if (should_start_vibration) {
            StartWidgetVibration(hwnd);
        }
    } else {
        quota_graph_animation_.Clear();
        ApplyQuotaGraphAnimationTimer(hwnd);
        usage_snapshot_.freshness = UsageFreshness::Error;
        usage_snapshot_.error_message = result->failure == UsageRefreshFailure::StandardException
            ? T(IDS_ERROR_USAGE_REFRESH)
            : T(IDS_UNKNOWN_ERROR);
    }

    usage_refresh_in_progress_ = false;
    usage_refresh_started_at_ = std::chrono::steady_clock::time_point{};
    usage_refresh_completion_pending_ = false;
    StopRefreshingLabelAnimation(hwnd);
    RefreshTrayQuotaTooltip(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Demarre au besoin un scan asynchrone des sessions locales.
// ----------------------------------------------------------------------------
void WidgetApp::RefreshTokenUsageSnapshot(HWND hwnd) {
    if (token_usage_refresh_worker_.IsActive()) {
        return;
    }
    if (token_usage_refresh_worker_.Start(hwnd)) {
        token_usage_snapshot_.freshness = TokenUsageFreshness::Loading;
        graph_interaction_.hovered_token_bar.reset();
        graph_interaction_.hovered_heatmap_cell.reset();
        InvalidateRect(hwnd, nullptr, FALSE);
    } else {
        token_usage_snapshot_.freshness = TokenUsageFreshness::Error;
        graph_interaction_.hovered_token_bar.reset();
        graph_interaction_.hovered_heatmap_cell.reset();
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// ----------------------------------------------------------------------------
// Applique le resultat du scan local sur le thread UI.
// ----------------------------------------------------------------------------
void WidgetApp::CompleteTokenUsageRefresh(HWND hwnd) {
    const std::optional<TokenUsageSnapshot> result = token_usage_refresh_worker_.TakeResult();
    if (result.has_value()) {
        token_usage_snapshot_ = *result;
        const std::size_t visible_count = app_settings_.token_graph_range == GraphRange::Hours1
            ? token_usage_snapshot_.five_minute.size()
            : token_usage_snapshot_.hourly.size();
        if (token_usage_snapshot_.freshness != TokenUsageFreshness::Fresh
            || (graph_interaction_.hovered_token_bar.has_value()
                && *graph_interaction_.hovered_token_bar >= visible_count)) {
            graph_interaction_.hovered_token_bar.reset();
        }
        if (token_usage_snapshot_.freshness != TokenUsageFreshness::Fresh
            || (graph_interaction_.hovered_heatmap_cell.has_value()
                && *graph_interaction_.hovered_heatmap_cell
                    >= token_usage_snapshot_.daily.size())) {
            graph_interaction_.hovered_heatmap_cell.reset();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

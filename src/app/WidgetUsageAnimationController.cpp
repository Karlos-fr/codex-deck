// ============================================================================
// Codex Glass - Implementation des animations d'usage applicatives
// ----------------------------------------------------------------------------
// Ce fichier extrait de WidgetApp.cpp la coordination des chiffres roulants et
// le mode de test temporaire qui force leur evolution.
// ============================================================================

#include "WidgetUsageAnimationController.h"

#include "WidgetTimers.h"
#include "../usage/UsageFormatting.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Avance un pourcentage utilise pour faire baisser l'affichage restant de 1.
//
// Parametres :
// - used_percent : pourcentage utilise courant.
// - available : disponibilite courante de la valeur.
//
// Retour :
// - prochain pourcentage utilise a afficher en mode test.
// ----------------------------------------------------------------------------
double NextRollingNumberTestUsedPercent(double used_percent, bool available) {
    if (!available) {
        return 1.0;
    }

    const double clamped_percent = std::clamp(used_percent, 0.0, 99.0);
    const double next_percent = std::floor(clamped_percent) + 1.0;
    return next_percent > 99.0 ? 1.0 : next_percent;
}

} // namespace

// ----------------------------------------------------------------------------
// Met a jour les animations de chiffres quand les indicateurs changent.
//
// Parametres :
// - hwnd : handle de la fenetre a animer.
// - previous_snapshot : releve affiche avant la mise a jour.
// - current_snapshot : nouveau releve affiche.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateRollingUsageAnimations(
    HWND hwnd,
    const UsageSnapshot& previous_snapshot,
    const UsageSnapshot& current_snapshot
) {
    const std::wstring five_hour_text = FormatOptionalRemainingPercent(
        current_snapshot.five_hour_used_percent,
        current_snapshot.five_hour_available
    );
    const std::wstring weekly_text = FormatOptionalRemainingPercent(
        current_snapshot.weekly_used_percent,
        current_snapshot.weekly_available
    );

    if (previous_snapshot.sampled_at.time_since_epoch().count() == 0
        || previous_snapshot.freshness == UsageFreshness::Refreshing) {
        five_hour_rolling_animation_.Reset(five_hour_text);
        weekly_rolling_animation_.Reset(weekly_text);
        ApplyRollingNumberAnimationTimer(hwnd);
        return;
    }

    const auto now = WidgetRollingNumberAnimation::Clock::now();
    five_hour_rolling_animation_.SetText(five_hour_text, now);
    weekly_rolling_animation_.SetText(weekly_text, now);
    ApplyRollingNumberAnimationTimer(hwnd);
}

// ----------------------------------------------------------------------------
// Active le timer de rendu des chiffres uniquement pendant une animation.
//
// Parametres :
// - hwnd : handle de la fenetre recevant les timers.
// ----------------------------------------------------------------------------
void WidgetApp::ApplyRollingNumberAnimationTimer(HWND hwnd) {
    KillTimer(hwnd, kRollingNumberAnimationTimerId);
    const auto now = WidgetRollingNumberAnimation::Clock::now();
    if (five_hour_rolling_animation_.IsActive(now) || weekly_rolling_animation_.IsActive(now)) {
        SetTimer(hwnd, kRollingNumberAnimationTimerId, kRollingNumberAnimationIntervalMs, nullptr);
    }
}

// ----------------------------------------------------------------------------
// Active ou desactive le test temporaire des chiffres.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::ToggleRollingNumberTestMode(HWND hwnd) {
    rolling_number_test_enabled_ = !rolling_number_test_enabled_;
    ApplyRollingNumberTestTimer(hwnd);
    if (rolling_number_test_enabled_) {
        TriggerRollingNumberTest(hwnd);
    }
}

// ----------------------------------------------------------------------------
// Fait baisser les valeurs affichees de 1 point pour tester l'animation.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
// ----------------------------------------------------------------------------
void WidgetApp::TriggerRollingNumberTest(HWND hwnd) {
    UsageSnapshot previous_snapshot = usage_snapshot_;
    UsageSnapshot test_snapshot = usage_snapshot_;
    test_snapshot.sampled_at = std::chrono::system_clock::now();
    test_snapshot.freshness = UsageFreshness::Fresh;
    test_snapshot.error_message.reset();
    test_snapshot.five_hour_available = true;
    test_snapshot.weekly_available = true;
    test_snapshot.five_hour_used_percent = NextRollingNumberTestUsedPercent(
        usage_snapshot_.five_hour_used_percent,
        usage_snapshot_.five_hour_available
    );
    test_snapshot.weekly_used_percent = NextRollingNumberTestUsedPercent(
        usage_snapshot_.weekly_used_percent,
        usage_snapshot_.weekly_available
    );

    usage_snapshot_ = test_snapshot;
    UpdateRollingUsageAnimations(hwnd, previous_snapshot, usage_snapshot_);
    InvalidateRect(hwnd, nullptr, FALSE);

    if (rolling_number_test_enabled_) {
        KillTimer(hwnd, kRollingNumberTestTimerId);
        rolling_number_test_next_tick_ = WidgetRollingNumberAnimation::Clock::now()
            + std::chrono::milliseconds{kRollingNumberTestIntervalMs};
        SetTimer(hwnd, kRollingNumberTestTimerId, kRollingNumberTestIntervalMs, nullptr);
    }
}

// ----------------------------------------------------------------------------
// Maintient le test et le rendu des chiffres pendant un deplacement Windows.
//
// Parametres :
// - hwnd : handle de la fenetre principale.
//
// Effets de bord :
// - declenche une valeur de test arrivee a echeance ;
// - traite immediatement la peinture d'une animation active.
// ----------------------------------------------------------------------------
void WidgetApp::UpdateRollingNumbersDuringMove(HWND hwnd) {
    auto now = WidgetRollingNumberAnimation::Clock::now();
    if (rolling_number_test_enabled_
        && rolling_number_test_next_tick_ != WidgetRollingNumberAnimation::TimePoint{}
        && now >= rolling_number_test_next_tick_) {
        TriggerRollingNumberTest(hwnd);
        now = WidgetRollingNumberAnimation::Clock::now();
    }

    if (five_hour_rolling_animation_.IsActive(now)
        || weekly_rolling_animation_.IsActive(now)) {
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    }
}

// ============================================================================
// Codex Glass - Tests des calculs UI Quotas et Tokens
// ----------------------------------------------------------------------------
// Ce fichier valide les formatages, series, echelles, layouts et hit-tests sans
// ouvrir la fenetre principale ni acceder aux sources de donnees distantes.
// ============================================================================

#include "localization/Localization.h"
#include "animation/WidgetQuotaGraphAnimation.h"
#include "color/WidgetColorPanel.h"
#include "color/WidgetColorTools.h"
#include "rendering/WidgetGraphData.h"
#include "rendering/WidgetGraphFormatting.h"
#include "rendering/WidgetGraphInteraction.h"
#include "rendering/WidgetGraphLayout.h"
#include "rendering/WidgetRenderProviderInfo.h"
#include "rendering/WidgetTokenGraphState.h"
#include "usage/UsageFormatting.h"

#include <cmath>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Leve une erreur de test si une condition est fausse.
// ----------------------------------------------------------------------------
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// ----------------------------------------------------------------------------
// Cree un instant UTC stable pour les tests de formatage.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point UtcTime(
    int year,
    int month,
    int day,
    int hour,
    int minute = 0,
    int second = 0
) {
    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
    return std::chrono::system_clock::from_time_t(_mkgmtime64(&utc));
}

// ----------------------------------------------------------------------------
// Indique si deux rectangles Direct2D sont identiques a la precision du test.
// ----------------------------------------------------------------------------
bool SameRect(const D2D1_RECT_F& left, const D2D1_RECT_F& right) {
    constexpr float kTolerance = 0.001F;
    return std::abs(left.left - right.left) < kTolerance
        && std::abs(left.top - right.top) < kTolerance
        && std::abs(left.right - right.right) < kTolerance
        && std::abs(left.bottom - right.bottom) < kTolerance;
}

// ----------------------------------------------------------------------------
// Valide les formateurs localises de date, credits, tokens et pourcentages.
// ----------------------------------------------------------------------------
void TestLocalizedFormatting() {
    SetUiLanguage(UiLanguage::French);
    const auto instant = UtcTime(2026, 7, 23, 14, 5, 32);
    Require(FormatLastUpdateTime(instant) == L"M\u00e0j 14:05:32", "heure francaise incorrecte");
    Require(FormatGraphDate(instant) == L"23 juil.", "date francaise incorrecte");
    Require(FormatRemainingPercent(25.4) == L"75 %", "pourcentage restant incorrect");
    Require(FormatGraphPercent(64.6) == L"65 %", "pourcentage graphe incorrect");
    Require(FormatGraphTokenMillions(112'400'000ULL) == L"112,4 M", "tokens francais incorrects");
    Require(GraphRangeDuration(GraphRange::Minutes5) == std::chrono::minutes{5}, "duree 5 min incorrecte");
    Require(FormatQuotaAxisTime(instant, GraphRange::Minutes5) == L"14:05", "axe 5 min incorrect");
    Require(
        FormatQuotaTooltipTime(instant, GraphRange::Minutes5) == L"14:05:32",
        "tooltip 5 min incorrect"
    );
    Require(FormatGraphRangeLabel(GraphRange::Minutes5) == L"5 min", "plage 5 min incorrecte");
    Require(GraphRangeDuration(GraphRange::Hours1) == std::chrono::hours{1}, "duree 1 h incorrecte");
    Require(FormatQuotaAxisTime(instant, GraphRange::Hours1) == L"14:05", "axe 1 h incorrect");
    Require(FormatQuotaTooltipTime(instant, GraphRange::Hours1) == L"14:05", "tooltip 1 h incorrect");
    Require(TokenGraphBucketCount(GraphRange::Hours1) == 12U, "buckets Tokens 1 h incorrects");
    Require(TokenGraphBucketCount(GraphRange::Hours5) == 5U, "buckets Tokens 5 h incorrects");
    Require(TokenGraphBucketCount(GraphRange::Hours24) == 24U, "buckets Tokens 24 h incorrects");
    Require(
        FormatQuotaResetMetadata(instant, true) == L"R\u00e9initialisation \u00b7 jeu. 23 juil. \u00b7 14:05",
        "reinitialisation francaise incorrecte"
    );
    Require(
        FormatQuotaResetMetadata({}, false) == L"R\u00e9initialisation \u00b7 n/a",
        "reinitialisation indisponible incorrecte"
    );

    UsageSnapshot snapshot{};
    snapshot.identity.plan_type = L"pro";
    snapshot.credits.available = true;
    snapshot.credits.balance = 1.5;
    Require(
        FormatProviderSummary(snapshot) == L"Pro \u00b7 1.5 cr\u00e9dit",
        "resume credits francais incorrect"
    );

    SetUiLanguage(UiLanguage::English);
    Require(FormatLastUpdateTime(instant) == L"Upd 14:05:32", "heure anglaise incorrecte");
    Require(FormatGraphDate(instant) == L"23 Jul.", "date anglaise incorrecte");
    Require(FormatGraphTokenMillions(112'400'000ULL) == L"112.4 M", "tokens anglais incorrects");
    Require(
        FormatProviderSummary(snapshot) == L"Pro \u00b7 1.5 credits",
        "resume credits anglais incorrect"
    );
}

// ----------------------------------------------------------------------------
// Valide les seuils et arrondis de l'echelle verticale Tokens.
// ----------------------------------------------------------------------------
void TestTokenAxisScale() {
    Require(
        CalculateTokenAxisScale(0) == std::pair<std::uint64_t, std::uint64_t>{400ULL, 100ULL},
        "echelle tokens a zero incorrecte"
    );
    Require(
        CalculateTokenAxisScale(80'000'000ULL) == std::pair<std::uint64_t, std::uint64_t>{80'000'000ULL, 20'000'000ULL},
        "echelle tokens dynamique incorrecte"
    );
    Require(
        CalculateTokenAxisScale(150'000'000ULL) == std::pair<std::uint64_t, std::uint64_t>{150'000'000ULL, 50'000'000ULL},
        "echelle tokens a 150 M incorrecte"
    );
    const auto above = CalculateTokenAxisScale(151'000'000ULL);
    Require(above.first >= 151'000'000ULL, "echelle tokens tronquee au-dessus de 150 M");
    Require(above.first % above.second == 0ULL, "maximum tokens non aligne sur son pas");
}

// ----------------------------------------------------------------------------
// Verifie que le dernier libelle Tokens remplace son voisin trop rapproche.
// ----------------------------------------------------------------------------
void TestTokenAxisLabelIndices() {
    Require(BuildTokenAxisLabelIndices(0).empty(), "graduations presentes sans bucket");
    Require(
        BuildTokenAxisLabelIndices(6) == std::vector<std::size_t>{0, 1, 2, 3, 4, 5},
        "graduations compactes incorrectes"
    );
    Require(
        BuildTokenAxisLabelIndices(12) == std::vector<std::size_t>{0, 2, 4, 6, 8, 11},
        "derniers libelles cinq minutes encore adjacents"
    );
    Require(
        BuildTokenAxisLabelIndices(24) == std::vector<std::size_t>{0, 4, 8, 12, 16, 23},
        "derniers libelles horaires encore trop proches"
    );
}

// ----------------------------------------------------------------------------
// Valide tous les etats des vues horaire et quotidienne de tokens.
// ----------------------------------------------------------------------------
void TestTokenGraphStates() {
    TokenUsageSnapshot snapshot{};
    Require(
        ResolveHourlyTokenGraphState(snapshot, GraphRange::Hours5) == WidgetTokenGraphState::Loading,
        "etat Loading horaire incorrect"
    );
    Require(
        ResolveTokenHeatmapState(snapshot) == WidgetTokenGraphState::Loading,
        "etat Loading heatmap incorrect"
    );

    snapshot.available = true;
    snapshot.freshness = TokenUsageFreshness::Fresh;
    snapshot.hourly.resize(kTokenHourlyGraphBucketCount);
    snapshot.daily.push_back(TokenDailyUsage{"2026-08-15"});
    Require(
        ResolveHourlyTokenGraphState(snapshot, GraphRange::Hours5) == WidgetTokenGraphState::Building,
        "etat Building horaire incorrect"
    );
    Require(
        ResolveTokenHeatmapState(snapshot) == WidgetTokenGraphState::Building,
        "etat Building heatmap incorrect"
    );

    snapshot.hourly.back().counts.input_tokens = 1;
    snapshot.daily.back().counts.output_tokens = 1;
    snapshot.freshness = TokenUsageFreshness::Error;
    Require(
        ResolveHourlyTokenGraphState(snapshot, GraphRange::Hours5) == WidgetTokenGraphState::Data,
        "donnees horaires masquees par une erreur temporaire"
    );
    Require(
        ResolveTokenHeatmapState(snapshot) == WidgetTokenGraphState::Data,
        "heatmap masquee par une erreur temporaire"
    );

    snapshot.hourly.clear();
    snapshot.daily.clear();
    Require(
        ResolveHourlyTokenGraphState(snapshot, GraphRange::Hours5) == WidgetTokenGraphState::Error,
        "etat Error horaire incorrect"
    );
    snapshot.available = true;
    snapshot.freshness = TokenUsageFreshness::Fresh;
    snapshot.five_minute.resize(kTokenFiveMinuteGraphBucketCount);
    Require(
        ResolveHourlyTokenGraphState(snapshot, GraphRange::Hours1) == WidgetTokenGraphState::Building,
        "etat Building cinq minutes incorrect"
    );
    snapshot.five_minute.back().counts.output_tokens = 1;
    Require(
        ResolveHourlyTokenGraphState(snapshot, GraphRange::Hours1) == WidgetTokenGraphState::Data,
        "donnees cinq minutes non detectees"
    );
    snapshot.freshness = TokenUsageFreshness::Empty;
    Require(
        ResolveTokenHeatmapState(snapshot) == WidgetTokenGraphState::NoSessions,
        "etat Empty heatmap incorrect"
    );
}

// ----------------------------------------------------------------------------
// Valide l'ordre, les trous et un saut de valeur correspondant a un reset.
// ----------------------------------------------------------------------------
void TestQuotaSeries() {
    const auto first = UtcTime(2026, 8, 1, 10);
    const auto missing = UtcTime(2026, 8, 1, 11);
    const auto reset = UtcTime(2026, 8, 1, 12);
    std::vector<UsageHistorySample> samples(3);
    samples[0].sampled_at = reset;
    samples[0].weekly_used_percent = 4.0;
    samples[0].weekly_reset_at = reset;
    samples[1].sampled_at = first;
    samples[1].weekly_used_percent = 90.0;
    samples[2].sampled_at = missing;

    const std::vector<QuotaGraphSample> series = BuildQuotaGraphSeries(samples);
    Require(series.size() == 3, "taille de serie quotas incorrecte");
    Require(series[0].sampled_at == first && series[2].sampled_at == reset, "serie quotas non ordonnee");
    Require(series[0].remaining_percent == std::optional<double>{10.0}, "valeur quotas initiale incorrecte");
    Require(!series[1].remaining_percent.has_value(), "trou quotas perdu");
    Require(series[2].remaining_percent == std::optional<double>{96.0}, "valeur apres reset incorrecte");
    Require(series[2].reset_at == std::optional{reset}, "date de reset quotas perdue");
}

// ----------------------------------------------------------------------------
// Valide la geometrie partagee et tous les hit-tests de la zone graphique.
// ----------------------------------------------------------------------------
void TestGraphLayoutAndHitTests() {
    Require(WidgetGraphPageToStoredValue(WidgetGraphPage::Quotas) == 0, "valeur INI Quotas modifiee");
    Require(WidgetGraphPageToStoredValue(WidgetGraphPage::Tokens) == 1, "valeur INI Tokens modifiee");
    Require(WidgetGraphPageToStoredValue(WidgetGraphPage::Activity) == 2, "valeur INI Activite incorrecte");
    Require(
        WidgetGraphPageFromStoredValue(99) == WidgetGraphPage::Quotas,
        "repli INI inconnu incorrect"
    );
    UsageSnapshot snapshot{};
    AppSettings quotas_settings{};
    quotas_settings.display_mode = WidgetDisplayMode::Complete;
    quotas_settings.show_graph = true;
    quotas_settings.graph_page = WidgetGraphPage::Quotas;
    AppSettings tokens_settings = quotas_settings;
    tokens_settings.graph_page = WidgetGraphPage::Tokens;
    AppSettings activity_settings = quotas_settings;
    activity_settings.graph_page = WidgetGraphPage::Activity;

    const D2D1_SIZE_F size = D2D1::SizeF(340.0F, 404.0F);
    const WidgetGraphLayout quotas = BuildWidgetGraphLayout(size, quotas_settings, snapshot, 0, 120.0F);
    const WidgetGraphLayout tokens = BuildWidgetGraphLayout(size, tokens_settings, snapshot, 5, 120.0F);
    const WidgetGraphLayout activity = BuildWidgetGraphLayout(size, activity_settings, snapshot, 0, 120.0F);
    Require(SameRect(quotas.section_rect, tokens.section_rect), "section de layout variable entre pages");
    Require(SameRect(quotas.plot_rect, tokens.plot_rect), "trace variable entre pages");
    Require(SameRect(quotas.x_axis_rect, tokens.x_axis_rect), "axe X variable entre pages");
    Require(SameRect(quotas.y_axis_rect, tokens.y_axis_rect), "axe Y variable entre pages");
    Require(SameRect(quotas.segmented_control_rect, tokens.segmented_control_rect), "switch variable entre pages");
    Require(SameRect(quotas.range_selector_rect, tokens.range_selector_rect), "selecteur variable entre pages");
    Require(SameRect(quotas.freshness_status_rect, tokens.freshness_status_rect), "statut variable entre pages");
    Require(SameRect(quotas.section_rect, activity.section_rect), "section Activite variable");
    Require(SameRect(quotas.plot_rect, activity.plot_rect), "trace Activite variable");
    Require(SameRect(quotas.segmented_control_rect, activity.segmented_control_rect), "switch Activite variable");
    Require(tokens.token_bar_rects.size() == 5, "cinq emplacements horaires absents");
    Require(
        activity.heatmap_cell_rects.size()
            == activity.heatmap_week_count * kWidgetHeatmapRowCount,
        "heatmap sans sept lignes completes"
    );

    const auto center = [](const D2D1_RECT_F& rect) {
        return D2D1::Point2F((rect.left + rect.right) * 0.5F, (rect.top + rect.bottom) * 0.5F);
    };
    Require(HitTestWidgetGraphTab(quotas, center(quotas.quotas_tab_rect)) == WidgetGraphPage::Quotas, "hit-test Quotas incorrect");
    Require(HitTestWidgetGraphTab(quotas, center(quotas.tokens_tab_rect)) == WidgetGraphPage::Tokens, "hit-test Tokens incorrect");
    Require(HitTestWidgetGraphTab(quotas, center(quotas.activity_tab_rect)) == WidgetGraphPage::Activity, "hit-test Activite incorrect");
    Require(HitTestWidgetGraphInfo(quotas, center(quotas.help_button_rect)), "hit-test info incorrect");
    Require(
        HitTestWidgetGraphRangeSelector(quotas, center(quotas.range_selector_rect)),
        "hit-test selecteur de plage incorrect"
    );
    Require(HitTestWidgetTokenBar(tokens, center(tokens.token_bar_rects.front())) == 0U, "premiere barre non detectee");
    Require(HitTestWidgetTokenBar(tokens, center(tokens.token_bar_rects.back())) == 4U, "derniere barre non detectee");
    Require(
        HitTestWidgetHeatmapCell(activity, center(activity.heatmap_cell_rects.front())) == 0U,
        "premiere cellule heatmap non detectee"
    );
    Require(HitTestWidgetQuotaPlot(quotas, center(quotas.plot_rect)).has_value(), "trace Quotas non detecte");
    Require(!HitTestWidgetQuotaPlot(quotas, D2D1::Point2F(0.0F, 0.0F)).has_value(), "point hors trace detecte");

    const WidgetGraphLayout narrow = BuildWidgetGraphLayout(
        D2D1::SizeF(240.0F, 404.0F),
        activity_settings,
        snapshot,
        0,
        120.0F
    );
    const WidgetGraphLayout wide = BuildWidgetGraphLayout(
        D2D1::SizeF(520.0F, 404.0F),
        activity_settings,
        snapshot,
        0,
        120.0F
    );
    const float minimum_segment_width = narrow.quotas_tab_rect.right - narrow.quotas_tab_rect.left;
    Require(minimum_segment_width >= 40.0F, "segments trop etroits a la largeur minimale");
    Require(
        std::abs(minimum_segment_width - (narrow.tokens_tab_rect.right - narrow.tokens_tab_rect.left))
            < 0.01F,
        "segments inegaux a la largeur minimale"
    );
    Require(wide.heatmap_week_count > narrow.heatmap_week_count, "heatmap non responsive en largeur");
    Require(
        activity.range_selector_rect.left == activity.range_selector_rect.right,
        "selecteur Activite encore visible"
    );
    Require(
        std::abs(
            activity.heatmap_cell_rects[kWidgetHeatmapRowCount - 1].bottom
                - activity.heatmap_grid_rect.bottom
        ) < 0.01F,
        "heatmap sans occupation complete de la hauteur disponible"
    );

    // Echelles DPI Windows validees en reconvertissant les points physiques.
    constexpr std::array<float, 4> kTestDpiValues = {96.0F, 120.0F, 144.0F, 192.0F};
    for (const float dpi : kTestDpiValues) {
        const float scale = dpi / 96.0F;
        const auto physical_to_dips = [scale](D2D1_POINT_2F point) {
            return D2D1::Point2F(point.x / scale, point.y / scale);
        };
        const D2D1_RECT_F& last_bar = tokens.token_bar_rects.back();
        const D2D1_POINT_2F physical_bar_center = D2D1::Point2F(
            ((last_bar.left + last_bar.right) * 0.5F) * scale,
            ((last_bar.top + last_bar.bottom) * 0.5F) * scale
        );
        Require(
            HitTestWidgetTokenBar(tokens, physical_to_dips(physical_bar_center)) == 4U,
            "hit-test horaire incorrect apres conversion DPI"
        );
        const D2D1_RECT_F& last_cell = activity.heatmap_cell_rects.back();
        const D2D1_POINT_2F physical_cell_center = D2D1::Point2F(
            ((last_cell.left + last_cell.right) * 0.5F) * scale,
            ((last_cell.top + last_cell.bottom) * 0.5F) * scale
        );
        Require(
            HitTestWidgetHeatmapCell(activity, physical_to_dips(physical_cell_center))
                == activity.heatmap_cell_rects.size() - 1,
            "hit-test heatmap incorrect apres conversion DPI"
        );
    }
}

// ----------------------------------------------------------------------------
// Valide le gel des samples puis la translation douce de la courbe Quotas.
// ----------------------------------------------------------------------------
void TestQuotaGraphAnimation() {
    WidgetQuotaGraphAnimation animation;
    const auto cutoff = UtcTime(2026, 8, 15, 12, 0);
    const auto previous_range_end = cutoff - std::chrono::seconds{10};
    const WidgetQuotaGraphAnimation::TimePoint started_at{};

    animation.Hold(cutoff, previous_range_end);
    WidgetQuotaGraphAnimationFrame frame = animation.Frame(started_at);
    Require(frame.maximum_sampled_at == std::optional{cutoff}, "gel Quotas absent");
    Require(frame.range_end == std::optional{previous_range_end}, "fenetre Quotas non gelee");
    Require(!frame.active, "gel Quotas anime");

    animation.Start(cutoff, started_at);
    frame = animation.Frame(started_at);
    Require(!frame.maximum_sampled_at.has_value(), "gel Quotas non libere");
    Require(frame.active && frame.range_end == std::optional{previous_range_end}, "depart Quotas incorrect");

    frame = animation.Frame(started_at + std::chrono::milliseconds{210});
    Require(
        frame.active && frame.range_end.has_value()
            && *frame.range_end > previous_range_end && *frame.range_end < cutoff,
        "interpolation Quotas incorrecte"
    );

    frame = animation.Frame(started_at + std::chrono::milliseconds{421});
    Require(!frame.active && !frame.range_end.has_value(), "animation Quotas non terminee");
}

// ----------------------------------------------------------------------------
// Valide la couleur de controle active et son exposition dans le panneau.
// ----------------------------------------------------------------------------
void TestActiveControlColor() {
    WidgetColorSettings colors{};

    // Couleur arbitraire distincte des valeurs par defaut pour le test.
    constexpr COLORREF kTestActiveColor = RGB(0x31, 0x7A, 0xD8);
    SetWidgetColorField(colors, WidgetColorField::ActiveControl, kTestActiveColor);
    Require(
        GetWidgetColorField(colors, WidgetColorField::ActiveControl) == kTestActiveColor,
        "couleur des controles actifs non conservee"
    );

    const WidgetColorPanelLayout layout = BuildWidgetColorPanelLayout(D2D1::SizeF(544.0F, 204.0F));
    const D2D1_RECT_F& swatch = layout.color_swatch_rects.back();
    Require(
        swatch.bottom < layout.random_button_rect.top,
        "bouton aleatoire superpose a la derniere couleur"
    );
    const POINT swatch_center{
        static_cast<LONG>((swatch.left + swatch.right) * 0.5F),
        static_cast<LONG>((swatch.top + swatch.bottom) * 0.5F),
    };
    const WidgetColorPanelHitTestResult hit = HitTestWidgetColorPanel(layout, swatch_center);
    Require(
        hit.action == WidgetColorPanelActionType::PickColor
            && hit.field == WidgetColorField::ActiveControl,
        "controle actif absent du panneau de couleurs"
    );
}

} // namespace

// ----------------------------------------------------------------------------
// Execute tous les tests UI et retourne un code compatible CTest.
// ----------------------------------------------------------------------------
int main() {
    try {
        _putenv_s("TZ", "UTC");
        _tzset();
        TestLocalizedFormatting();
        TestTokenAxisScale();
        TestTokenAxisLabelIndices();
        TestTokenGraphStates();
        TestQuotaSeries();
        TestGraphLayoutAndHitTests();
        TestQuotaGraphAnimation();
        TestActiveControlColor();
        std::cout << "WidgetUiTests: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WidgetUiTests: " << error.what() << '\n';
        return 1;
    }
}

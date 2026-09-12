// ============================================================================
// Codex Glass - Tests des providers et du parsing Codex
// ----------------------------------------------------------------------------
// Ce fichier valide les fixtures anonymisees et le worker generique. Il ne
// realise aucun appel reseau et utilise seulement des bases temporaires isolees.
// ============================================================================

#include "providers/codex/CodexUsageParser.h"
#include "tokens/CodexSessionScanner.h"
#include "tokens/TokenHeatmapData.h"
#include "tokens/TokenUsageBuckets.h"
#include "tokens/TokenUsageRefreshWorker.h"
#include "tokens/TokenUsageStore.h"
#include "usage/IUsageProvider.h"
#include "usage/UsageHistoryStore.h"
#include "usage/UsageRefreshWorker.h"

#include <atomic>
#include <cmath>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <sqlite3.h>

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
// Charge une fixture UTF-8 depuis le depot.
// ----------------------------------------------------------------------------
std::string LoadFixture(const char* relative_path) {
    const std::string path = std::string(CODEX_DECK_SOURCE_DIR) + "/tests/fixtures/" + relative_path;
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input), "fixture introuvable");
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// ----------------------------------------------------------------------------
// Valide les formes simples et enrichies de l'endpoint Codex.
// ----------------------------------------------------------------------------
void TestCodexUsageParser() {
    std::wstring error;
    const auto basic = CodexUsageParser::Parse(LoadFixture("codex_usage_basic.json"), &error);
    Require(basic.has_value(), "fixture basic non parseable");
    Require(basic->five_hour_available && basic->weekly_available, "quotas basic absents");
    Require(basic->identity.plan_type == std::optional<std::wstring>{L"free"}, "forfait free absent");
    Require(!basic->credits.available, "credits basic ne devraient pas etre presents");

    const auto enriched = CodexUsageParser::Parse(LoadFixture("codex_usage_enriched.json"), &error);
    Require(enriched.has_value(), "fixture enriched non parseable");
    Require(enriched->identity.plan_type == std::optional<std::wstring>{L"pro"}, "forfait pro absent");
    Require(enriched->credits.available && enriched->credits.balance.has_value(), "credits enrichis absents");
    Require(std::abs(*enriched->credits.balance - 42.75) < 0.001, "solde credits incorrect");
    Require(enriched->additional_rate_limits.size() == 1, "limite supplementaire absente");
    Require(enriched->spend_control.has_value(), "controle de depense absent");

    const auto tolerant = CodexUsageParser::Parse(LoadFixture("codex_usage_tolerant.json"), &error);
    Require(tolerant.has_value(), "fixture tolerant non parseable");
    Require(tolerant->identity.plan_type == std::optional<std::wstring>{L"future_plan"}, "forfait inconnu perdu");
    Require(tolerant->credits.unlimited, "credits illimites absents");
    Require(tolerant->additional_rate_limits.size() == 1, "entrees invalides mal isolees");
    Require(tolerant->spend_control.has_value(), "spend_control textuel absent");
}

// ----------------------------------------------------------------------------
// Faux provider controlable utilise par les tests du worker.
// ----------------------------------------------------------------------------
class FakeUsageProvider final : public IUsageProvider {
public:
    // Indique si FetchUsage doit lever une exception.
    bool throws = false;

    // Indique si FetchUsage doit attendre une annulation.
    bool waits_for_cancel = false;

    // Indique qu'une annulation a ete recue.
    std::atomic_bool cancelled = false;

    // ------------------------------------------------------------------------
    // Retourne l'identifiant de test Codex.
    // ------------------------------------------------------------------------
    UsageProviderId ProviderId() const override {
        return UsageProviderId::Codex;
    }

    // ------------------------------------------------------------------------
    // Produit un snapshot, une exception ou une attente annulable.
    // ------------------------------------------------------------------------
    UsageSnapshot FetchUsage() override {
        if (throws) {
            throw std::runtime_error("echec simule");
        }
        while (waits_for_cancel && !cancelled.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        UsageSnapshot snapshot{};
        snapshot.five_hour_used_percent = 25.0;
        snapshot.weekly_used_percent = 50.0;
        return snapshot;
    }

    // ------------------------------------------------------------------------
    // Libere une attente simulee.
    // ------------------------------------------------------------------------
    void Cancel() override {
        cancelled.store(true);
    }
};

// ----------------------------------------------------------------------------
// Valide succes, exception et annulation du worker generique.
// ----------------------------------------------------------------------------
void TestUsageRefreshWorker() {
    auto success_provider = std::make_shared<FakeUsageProvider>();
    UsageRefreshWorker success_worker([success_provider]() { return success_provider; }, false);
    Require(success_worker.Start(nullptr), "worker succes non demarre");
    const auto success = success_worker.TakeResult();
    Require(success.has_value(), "resultat succes absent");
    Require(success->failure == UsageRefreshFailure::None, "worker succes en erreur");
    Require(success->snapshot.five_hour_used_percent == 25.0, "snapshot du faux provider perdu");

    auto error_provider = std::make_shared<FakeUsageProvider>();
    error_provider->throws = true;
    UsageRefreshWorker error_worker([error_provider]() { return error_provider; }, false);
    Require(error_worker.Start(nullptr), "worker erreur non demarre");
    const auto error = error_worker.TakeResult();
    Require(error.has_value(), "resultat erreur absent");
    Require(error->failure == UsageRefreshFailure::StandardException, "exception standard non classee");

    auto waiting_provider = std::make_shared<FakeUsageProvider>();
    waiting_provider->waits_for_cancel = true;
    UsageRefreshWorker waiting_worker([waiting_provider]() { return waiting_provider; }, false);
    Require(waiting_worker.Start(nullptr), "worker annulable non demarre");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    waiting_worker.Stop();
    Require(waiting_provider->cancelled.load(), "annulation non transmise au provider");
}

// ----------------------------------------------------------------------------
// Copie une fixture dans un profil Codex temporaire.
// ----------------------------------------------------------------------------
void CopySessionFixture(
    const std::filesystem::path& profile,
    const char* fixture_name,
    const std::filesystem::path& relative_destination
) {
    const std::filesystem::path source =
        std::filesystem::path(CODEX_DECK_SOURCE_DIR) / "tests" / "fixtures" / "sessions" / fixture_name;
    const std::filesystem::path destination = profile / relative_destination;
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
}

// ----------------------------------------------------------------------------
// Valide les deltas, les archives, la ligne partielle et la serie calendaire.
// ----------------------------------------------------------------------------
void TestCodexSessionScanner() {
    const std::filesystem::path profile = std::filesystem::temp_directory_path()
        / ("codex-glass-token-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::remove_all(profile, error);
    CopySessionFixture(
        profile,
        "active-session.jsonl",
        std::filesystem::path("sessions") / "2026" / "08" / "13" / "active-session.jsonl"
    );
    CopySessionFixture(
        profile,
        "archived-session.jsonl",
        std::filesystem::path("archived_sessions") / "archived-session.jsonl"
    );
    CopySessionFixture(
        profile,
        "active-session.jsonl",
        std::filesystem::path("archived_sessions") / "duplicate-active-session.jsonl"
    );
    const std::filesystem::path active_file = profile / "sessions" / "2026" / "08" / "13" / "active-session.jsonl";
    {
        std::ifstream input(active_file, std::ios::binary | std::ios::ate);
        const std::uintmax_t size = static_cast<std::uintmax_t>(input.tellg());
        input.seekg(-1, std::ios::end);
        char trailing = '\0';
        input.get(trailing);
        if (trailing == '\n') {
            std::filesystem::resize_file(active_file, size - 1);
        }
    }

    std::tm utc{};
    utc.tm_year = 2026 - 1900;
    utc.tm_mon = 8 - 1;
    utc.tm_mday = 14;
    utc.tm_hour = 12;
    const auto now = std::chrono::system_clock::from_time_t(_mkgmtime64(&utc));
    TokenScanCache cache;
    const CodexSessionScanner scanner(profile);
    const TokenUsageSnapshot snapshot = scanner.Scan(now, 30, nullptr, &cache);

    Require(snapshot.available, "profil temporaire non detecte");
    Require(snapshot.daily.size() == 30, "serie de 30 jours absente");
    Require(snapshot.daily.back().date_key == "2026-08-14", "jour courant absent en fin de serie");
    Require(snapshot.hourly.size() == kTokenHourlyGraphBucketCount, "serie de cinq heures absente");
    Require(
        snapshot.five_minute.size() == kTokenFiveMinuteGraphBucketCount,
        "serie de cinq minutes absente"
    );
    Require(
        std::is_sorted(
            snapshot.hourly.begin(),
            snapshot.hourly.end(),
            [](const TokenHourlyUsage& left, const TokenHourlyUsage& right) {
                return left.bucket_start < right.bucket_start;
            }
        ),
        "serie horaire non ordonnee"
    );
    Require(snapshot.sessions.size() == 2, "session archivee dupliquee ou sessions absentes");

    std::uint64_t total = 0;
    std::uint64_t cached = 0;
    std::uint64_t requests = 0;
    for (const TokenDailyUsage& day : snapshot.daily) {
        total += day.counts.Total();
        cached += day.counts.cached_input_tokens;
        requests += day.request_count;
    }
    Require(total == 2590, "total de tokens incorrect ou cumulatif recompte");
    Require(cached == 950, "tokens caches incorrects");
    Require(requests == 4, "ligne partielle ou evenement duplique comptabilise");

    std::tm hourly_utc{};
    hourly_utc.tm_year = 2026 - 1900;
    hourly_utc.tm_mon = 8 - 1;
    hourly_utc.tm_mday = 14;
    hourly_utc.tm_min = 30;
    const auto hourly_now = std::chrono::system_clock::from_time_t(_mkgmtime64(&hourly_utc));
    TokenScanCache hourly_cache;
    const TokenUsageSnapshot hourly_snapshot = scanner.Scan(
        hourly_now,
        2,
        nullptr,
        &hourly_cache
    );
    std::uint64_t hourly_total = 0;
    std::uint64_t hourly_requests = 0;
    for (const TokenHourlyUsage& hour : hourly_snapshot.hourly) {
        hourly_total += hour.counts.Total();
        hourly_requests += hour.request_count;
    }
    Require(hourly_total == 1680, "fenetre horaire ne correspond pas aux evenements visibles");
    Require(hourly_requests == 2, "requetes horaires recomptees ou perdues");
    const TokenUsageSnapshot same_hourly_snapshot = scanner.Scan(
        hourly_now,
        2,
        nullptr,
        &hourly_cache
    );
    std::uint64_t same_hourly_total = 0;
    for (const TokenHourlyUsage& hour : same_hourly_snapshot.hourly) {
        same_hourly_total += hour.counts.Total();
    }
    Require(same_hourly_total == hourly_total, "cache horaire inchange recompte");

    const TokenUsageSnapshot unchanged = scanner.Scan(now, 30, nullptr, &cache);
    std::uint64_t unchanged_total = 0;
    for (const TokenDailyUsage& day : unchanged.daily) {
        unchanged_total += day.counts.Total();
    }
    Require(unchanged_total == total, "second scan sans changement duplique les compteurs");

    {
        std::ofstream output(active_file, std::ios::binary | std::ios::app);
        output << ",\"info\":{\"last_token_usage\":{\"input_tokens\":10,\"output_tokens\":5}}}}\n";
    }
    const TokenUsageSnapshot appended = scanner.Scan(now, 30, nullptr, &cache);
    std::uint64_t appended_total = 0;
    std::uint64_t appended_requests = 0;
    for (const TokenDailyUsage& day : appended.daily) {
        appended_total += day.counts.Total();
        appended_requests += day.request_count;
    }
    Require(appended_total == total + 15, "reprise a l'offset du fichier ajoutee incorrecte");
    Require(appended_requests == requests + 1, "nouvelle ligne complete non comptabilisee");

    const std::filesystem::path database = profile / "token-cache.db";
    TokenUsageStore store;
    Require(store.Open(database.wstring()), "cache SQLite tokens non ouvert");
    Require(store.Save(cache, appended), "cache SQLite tokens non enregistre");
    TokenScanCache restored_cache;
    Require(store.LoadScanCache(restored_cache), "curseurs tokens non relus");
    Require(restored_cache.files.size() == cache.files.size(), "nombre de curseurs restaure incorrect");
    TokenUsageSnapshot restored_snapshot;
    Require(store.LoadLatestSnapshot(restored_snapshot), "dernier snapshot tokens non relu");
    Require(restored_snapshot.daily.size() == 30, "serie quotidienne SQLite incomplete");
    Require(
        restored_snapshot.hourly.size() == kTokenHourlyGraphBucketCount,
        "serie horaire SQLite incomplete"
    );
    Require(
        restored_snapshot.five_minute.size() == kTokenFiveMinuteGraphBucketCount,
        "serie cinq minutes SQLite incomplete"
    );
    store.Close();

    TokenUsageRefreshWorker worker(profile, database.wstring());
    Require(worker.Start(nullptr), "worker tokens non demarre");
    const auto worker_result = worker.TakeResult();
    Require(worker_result.has_value(), "resultat du worker tokens absent");
    Require(
        worker_result->daily.size() == static_cast<std::size_t>(kTokenHeatmapCoverageDays),
        "worker tokens sans couverture heatmap"
    );

    TokenUsageRefreshWorker stopped_worker(profile, database.wstring());
    Require(stopped_worker.Start(nullptr), "worker tokens annulable non demarre");
    stopped_worker.Stop();

    std::filesystem::remove_all(profile, error);
}

// Declaration anticipee du constructeur d'instant UTC utilise ci-dessous.
std::chrono::system_clock::time_point UtcTime(int year, int month, int day, int hour);

// ----------------------------------------------------------------------------
// Valide les buckets horaires, les heures repetees et les niveaux heatmap.
// ----------------------------------------------------------------------------
void TestTokenHourlyBucketsAndHeatmap() {
    const auto now = UtcTime(2026, 8, 15, 12) + std::chrono::minutes{37};
    const std::vector<TokenHourlyUsage> hours = BuildRecentTokenHours(
        now,
        kTokenHourlyGraphBucketCount
    );
    Require(hours.size() == kTokenHourlyGraphBucketCount, "nombre de buckets horaires incorrect");
    Require(
        hours.back().bucket_start == TokenLocalHourStart(now),
        "heure courante absente du dernier bucket"
    );
    for (std::size_t index = 1; index < hours.size(); ++index) {
        Require(
            hours[index].bucket_start - hours[index - 1].bucket_start == std::chrono::hours{1},
            "trou dans les buckets horaires"
        );
    }
    const std::vector<TokenFiveMinuteUsage> five_minutes = BuildRecentTokenFiveMinutes(
        now,
        kTokenFiveMinuteGraphBucketCount
    );
    Require(
        five_minutes.size() == kTokenFiveMinuteGraphBucketCount,
        "nombre de buckets de cinq minutes incorrect"
    );
    Require(
        five_minutes.back().bucket_start == TokenFiveMinuteStart(now),
        "tranche courante absente du dernier bucket"
    );
    for (std::size_t index = 1; index < five_minutes.size(); ++index) {
        Require(
            five_minutes[index].bucket_start - five_minutes[index - 1].bucket_start
                == std::chrono::minutes{5},
            "trou dans les buckets de cinq minutes"
        );
    }

    const auto first_autumn_hour = UtcTime(2026, 10, 25, 0) + std::chrono::minutes{30};
    const auto second_autumn_hour = UtcTime(2026, 10, 25, 1) + std::chrono::minutes{30};
    Require(
        TokenHourKey(TokenLocalHourStart(first_autumn_hour))
            != TokenHourKey(TokenLocalHourStart(second_autumn_hour)),
        "deux occurrences de l'heure d'automne confondues"
    );

    std::vector<TokenDailyUsage> days{
        TokenDailyUsage{"2026-08-31"},
        TokenDailyUsage{"2026-09-01"},
        TokenDailyUsage{"2026-09-02"},
    };
    days[0].counts.input_tokens = 1;
    days[1].counts.input_tokens = 100;
    days[2].counts.input_tokens = 10'000;
    const std::vector<TokenHeatmapCell> cells = BuildTokenHeatmapCells(days, 2);
    Require(cells.size() == 2 * kTokenHeatmapRowCount, "matrice heatmap incomplete");
    const auto current = std::find_if(cells.begin(), cells.end(), [](const TokenHeatmapCell& cell) {
        return cell.date_key == "2026-09-02";
    });
    Require(current != cells.end(), "jour courant absent de la heatmap");
    Require(current->column == 1 && current->row == 2, "jour courant mal aligne dans sa semaine");
    Require(current->level == kTokenHeatmapActiveLevelCount, "maximum heatmap mal normalise");
    Require(
        CalculateTokenHeatmapLevel(1, 1, 10'000)
            < CalculateTokenHeatmapLevel(5'000, 1, 10'000),
        "niveaux visibles non croissants"
    );
    Require(
        CalculateTokenHeatmapLevel(1, 1, 10'000) == 1
            && CalculateTokenHeatmapLevel(10'000, 1, 10'000)
                == kTokenHeatmapActiveLevelCount,
        "palette heatmap incomplete entre le minimum et le maximum"
    );
    const auto future = std::find_if(cells.begin(), cells.end(), [](const TokenHeatmapCell& cell) {
        return cell.column == 1 && cell.row == 3;
    });
    Require(future != cells.end() && !future->present, "jour futur confondu avec un jour a zero");
}

// ----------------------------------------------------------------------------
// Cree un instant UTC stable pour les tests calendaires.
// ----------------------------------------------------------------------------
std::chrono::system_clock::time_point UtcTime(int year, int month, int day, int hour) {
    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    return std::chrono::system_clock::from_time_t(_mkgmtime64(&utc));
}

// ----------------------------------------------------------------------------
// Valide minuit local, les changements d'heure et un profil sans session.
// ----------------------------------------------------------------------------
void TestTokenCalendarAndEmptyProfile() {
    const std::filesystem::path profile = std::filesystem::temp_directory_path()
        / ("codex-glass-calendar-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::remove_all(profile, error);
    const std::filesystem::path session = profile / "sessions" / "calendar.jsonl";
    std::filesystem::create_directories(session.parent_path());
    {
        std::ofstream output(session, std::ios::binary);
        const auto event = [&output](const char* timestamp) {
            output << "{\"timestamp\":\"" << timestamp
                << "\",\"type\":\"event_msg\",\"payload\":{\"type\":\"token_count\",\"info\":{"
                << "\"last_token_usage\":{\"input_tokens\":1,\"output_tokens\":1}}}}\n";
        };
        event("2026-03-28T22:30:00Z");
        event("2026-03-28T23:30:00Z");
        event("2026-03-29T00:30:00Z");
        event("2026-03-29T01:30:00Z");
        event("2026-10-25T00:30:00Z");
        event("2026-10-25T01:30:00Z");
    }

    const CodexSessionScanner scanner(profile);
    const TokenUsageSnapshot spring = scanner.Scan(UtcTime(2026, 3, 29, 12), 2);
    Require(spring.daily.size() == 2, "serie autour de minuit incomplete");
    Require(spring.daily[0].request_count == 1, "evenement avant minuit local mal groupe");
    Require(spring.daily[1].request_count == 3, "passage a l'heure d'ete mal groupe");

    const TokenUsageSnapshot autumn = scanner.Scan(UtcTime(2026, 10, 25, 12), 1);
    Require(autumn.daily.size() == 1, "jour d'heure d'hiver absent");
    Require(autumn.daily[0].request_count == 2, "heure d'hiver doublee ou perdue");

    const std::filesystem::path empty_profile = profile / "empty-profile";
    std::filesystem::create_directories(empty_profile);
    const TokenUsageSnapshot empty = CodexSessionScanner(empty_profile).Scan(UtcTime(2026, 8, 14, 12), 30);
    Require(empty.available, "CODEX_HOME vide considere indisponible");
    Require(empty.daily.size() == 30, "profil vide sans serie de jours a zero");
    Require(empty.sessions.empty(), "profil vide avec sessions fantomes");

    const TokenUsageSnapshot year_boundary = CodexSessionScanner(empty_profile).Scan(
        UtcTime(2027, 1, 2, 12),
        30
    );
    Require(year_boundary.daily.size() == 30, "serie inter-annee incomplete");
    Require(year_boundary.daily.front().date_key == "2026-12-04", "debut inter-annee incorrect");
    Require(year_boundary.daily.back().date_key == "2027-01-02", "jour courant inter-annee incorrect");

    std::filesystem::remove_all(profile, error);
}

// ----------------------------------------------------------------------------
// Valide le delta cumulatif lorsqu'une base se trouve avant la couverture.
// ----------------------------------------------------------------------------
void TestScannerCoverageBoundary() {
    const std::filesystem::path profile = std::filesystem::temp_directory_path()
        / ("codex-glass-boundary-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::remove_all(profile, error);
    const std::filesystem::path session = profile / "sessions" / "boundary.jsonl";
    std::filesystem::create_directories(session.parent_path());
    {
        std::ofstream output(session, std::ios::binary);
        output << "{\"timestamp\":\"2026-08-13T12:00:00Z\",\"type\":\"event_msg\","
            "\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{"
            "\"input_tokens\":100}}}}\n";
        output << "{\"timestamp\":\"2026-08-14T12:00:00Z\",\"type\":\"event_msg\","
            "\"payload\":{\"type\":\"token_count\",\"info\":{\"total_token_usage\":{"
            "\"input_tokens\":110}}}}\n";
    }
    const TokenUsageSnapshot snapshot = CodexSessionScanner(profile).Scan(
        UtcTime(2026, 8, 15, 12),
        2
    );
    std::uint64_t total = 0;
    for (const TokenDailyUsage& day : snapshot.daily) {
        total += day.counts.Total();
    }
    Require(total == 10, "base cumulative hors couverture recomptee");
    std::filesystem::remove_all(profile, error);
}

// ----------------------------------------------------------------------------
// Mesure un premier scan volumineux et verifie une annulation immediate.
// ----------------------------------------------------------------------------
void TestScannerPerformanceAndCancellation() {
    // Nombre d'evenements representant le profil volumineux synthetique.
    constexpr int kSyntheticEventCount = 5000;
    const std::filesystem::path profile = std::filesystem::temp_directory_path()
        / ("codex-glass-performance-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::remove_all(profile, error);
    const std::filesystem::path session = profile / "sessions" / "large.jsonl";
    std::filesystem::create_directories(session.parent_path());
    {
        std::ofstream output(session, std::ios::binary);
        for (int index = 0; index < kSyntheticEventCount; ++index) {
            output << "{\"timestamp\":\"2026-08-15T10:00:00Z\",\"type\":\"event_msg\","
                "\"payload\":{\"type\":\"token_count\",\"turn_id\":\"turn-" << index
                << "\",\"info\":{\"last_token_usage\":{\"input_tokens\":1,"
                "\"output_tokens\":1}}}}\n";
        }
    }
    const CodexSessionScanner scanner(profile);
    const auto started_at = std::chrono::steady_clock::now();
    const TokenUsageSnapshot snapshot = scanner.Scan(UtcTime(2026, 8, 15, 12), 1);
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    Require(snapshot.daily.front().request_count == kSyntheticEventCount, "scan volumineux incomplet");
    Require(elapsed < std::chrono::seconds{10}, "premier rescan volumineux anormalement lent");

    std::atomic_bool cancelled = true;
    const auto cancel_started_at = std::chrono::steady_clock::now();
    scanner.Scan(UtcTime(2026, 8, 15, 12), 1, &cancelled);
    Require(
        std::chrono::steady_clock::now() - cancel_started_at < std::chrono::seconds{1},
        "annulation du scanner trop lente"
    );
    std::filesystem::remove_all(profile, error);
}

// ----------------------------------------------------------------------------
// Verifie la continuite d'une plage malgre la deduplication des releves.
// ----------------------------------------------------------------------------
void TestUsageHistoryRangeContinuity() {
    const std::filesystem::path database_path = std::filesystem::temp_directory_path()
        / ("codex-glass-history-test-" + std::to_string(GetCurrentProcessId()) + ".db");
    std::error_code error;
    std::filesystem::remove(database_path, error);

    UsageHistoryStore store;
    Require(store.Open(database_path.wstring()), "ouverture historique de test impossible");

    const auto initial_time = UtcTime(2026, 8, 15, 12);
    UsageSnapshot snapshot{};
    snapshot.freshness = UsageFreshness::Fresh;
    snapshot.sampled_at = initial_time;
    snapshot.weekly_available = true;
    snapshot.weekly_used_percent = 23.0;
    Require(store.RecordSnapshot(snapshot), "premier sample historique refuse");

    snapshot.sampled_at = initial_time + std::chrono::seconds{10};
    Require(store.RecordSnapshot(snapshot), "deduplication historique en erreur");

    const auto range_start = initial_time + std::chrono::minutes{2};
    const std::vector<UsageHistorySample> samples = store.LoadSamplesForRange(range_start, 100);
    Require(samples.size() == 1, "sample deduplique duplique dans la plage");
    Require(samples.front().sampled_at == range_start, "borne de continuite historique absente");
    Require(
        samples.front().weekly_used_percent == std::optional<double>{23.0},
        "valeur de continuite historique incorrecte"
    );

    store.Close();
    std::filesystem::remove(database_path, error);
}

// ----------------------------------------------------------------------------
// Valide migration additive, cache obsolete et rollback SQLite atomique.
// ----------------------------------------------------------------------------
void TestTokenUsageStoreMigrationAndRollback() {
    const std::filesystem::path database_path = std::filesystem::temp_directory_path()
        / ("codex-glass-token-migration-" + std::to_string(GetCurrentProcessId()) + ".db");
    std::error_code error;
    std::filesystem::remove(database_path, error);

    sqlite3* legacy_database = nullptr;
    Require(
        sqlite3_open(database_path.string().c_str(), &legacy_database) == SQLITE_OK,
        "ancienne base SQLite non creee"
    );
    const char* legacy_sql =
        "CREATE TABLE token_daily_aggregates (date_key TEXT PRIMARY KEY, input_tokens INTEGER NOT NULL, "
        "cached_input_tokens INTEGER NOT NULL, output_tokens INTEGER NOT NULL, "
        "reasoning_output_tokens INTEGER NOT NULL, request_count INTEGER NOT NULL, updated_at INTEGER NOT NULL);"
        "INSERT INTO token_daily_aggregates VALUES('2026-08-15',40,0,2,0,1,1);"
        "CREATE TABLE token_scan_files (path TEXT PRIMARY KEY, file_size INTEGER NOT NULL, "
        "modified_ticks INTEGER NOT NULL, valid_offset INTEGER NOT NULL, payload TEXT NOT NULL);"
        "INSERT INTO token_scan_files VALUES('legacy.jsonl',12,34,12,'{\"days\":[]}');";
    Require(
        sqlite3_exec(legacy_database, legacy_sql, nullptr, nullptr, nullptr) == SQLITE_OK,
        "ancien schema SQLite non initialise"
    );
    sqlite3_close(legacy_database);

    TokenUsageStore store;
    Require(store.Open(database_path.wstring()), "migration additive SQLite impossible");
    TokenUsageSnapshot migrated{};
    Require(store.LoadLatestSnapshot(migrated), "historique quotidien perdu pendant la migration");
    Require(migrated.daily.size() == 1, "jour historique non conserve pendant la migration");
    Require(migrated.daily.front().counts.Total() == 42, "compteurs historiques modifies");
    TokenScanCache obsolete_cache;
    Require(store.LoadScanCache(obsolete_cache), "lecture du cache obsolete en erreur");
    Require(obsolete_cache.files.empty(), "payload sans version horaire accepte");

    TokenUsageSnapshot baseline = migrated;
    baseline.available = true;
    baseline.updated_at = UtcTime(2026, 8, 15, 12);
    baseline.hourly = BuildRecentTokenHours(baseline.updated_at, kTokenHourlyGraphBucketCount);
    baseline.hourly.back().counts.input_tokens = 9;
    Require(store.Save({}, baseline), "snapshot de reference non enregistre");

    TokenUsageSnapshot invalid = baseline;
    invalid.daily.front().counts.input_tokens = 999;
    invalid.hourly.push_back(invalid.hourly.back());
    Require(!store.Save({}, invalid), "doublon horaire non rejete");
    TokenUsageSnapshot after_rollback{};
    Require(store.LoadLatestSnapshot(after_rollback), "snapshot absent apres rollback");
    Require(
        after_rollback.daily.front().counts.Total() == 42,
        "transaction partielle conservee apres rollback"
    );
    Require(
        after_rollback.hourly.size() == kTokenHourlyGraphBucketCount,
        "agregats horaires alteres apres rollback"
    );
    store.Close();
    std::filesystem::remove(database_path, error);
}

} // namespace

// ----------------------------------------------------------------------------
// Execute tous les tests et retourne un code compatible CTest.
// ----------------------------------------------------------------------------
int main() {
    try {
        TestCodexUsageParser();
        TestUsageRefreshWorker();
        TestCodexSessionScanner();
        TestTokenCalendarAndEmptyProfile();
        TestScannerCoverageBoundary();
        TestScannerPerformanceAndCancellation();
        TestTokenHourlyBucketsAndHeatmap();
        TestUsageHistoryRangeContinuity();
        TestTokenUsageStoreMigrationAndRollback();
        std::cout << "CodexUsageTests: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CodexUsageTests: " << error.what() << '\n';
        return 1;
    }
}

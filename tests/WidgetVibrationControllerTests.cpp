// ============================================================================
// Codex Glass - Tests du declencheur des Effets Motion
// ----------------------------------------------------------------------------
// Ce fichier valide le lien entre les releves de quotas et le declenchement
// Motion. Il ne demarre ni fenetre, ni animation, ni source distante.
// ============================================================================

#include "vibration/WidgetVibrationController.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {

// ----------------------------------------------------------------------------
// Leve une erreur de test lorsqu'une condition est fausse.
//
// Parametres :
// - condition : resultat a verifier.
// - message : diagnostic associe a l'echec.
// ----------------------------------------------------------------------------
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// ----------------------------------------------------------------------------
// Construit un releve frais avec les deux quotas standards.
//
// Parametres :
// - five_hour_used : pourcentage utilise sur cinq heures.
// - weekly_used : pourcentage utilise sur la semaine.
//
// Retour :
// - releve exploitable par le controleur.
// ----------------------------------------------------------------------------
UsageSnapshot StandardSnapshot(double five_hour_used, double weekly_used) {
    UsageSnapshot snapshot{};
    snapshot.freshness = UsageFreshness::Fresh;
    snapshot.five_hour_available = true;
    snapshot.weekly_available = true;
    snapshot.five_hour_used_percent = five_hour_used;
    snapshot.weekly_used_percent = weekly_used;
    return snapshot;
}

// ----------------------------------------------------------------------------
// Construit une limite supplementaire a fenetre principale disponible.
//
// Parametres :
// - id : identifiant stable de la limite.
// - used_percent : pourcentage utilise courant.
//
// Retour :
// - limite comparable entre deux releves.
// ----------------------------------------------------------------------------
ProviderAdditionalRateLimit AdditionalLimit(const wchar_t* id, double used_percent) {
    ProviderAdditionalRateLimit limit{};
    limit.id = id;
    limit.label = id;
    limit.primary.available = true;
    limit.primary.used_percent = used_percent;
    return limit;
}

// ----------------------------------------------------------------------------
// Verifie le seuil, le cooldown et l'activation globale du declencheur.
// ----------------------------------------------------------------------------
void TestStandardQuotaTrigger() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.usage_drop_threshold_percent = 5;
    settings.minimum_interval_seconds = 60;
    WidgetVibrationController controller;
    const std::chrono::system_clock::time_point started_at{};

    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(10.0, 20.0), started_at),
        "premier releve declenche Motion");
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(14.0, 20.0), started_at + std::chrono::seconds{10}),
        "baisse sous le seuil declenche Motion");
    Require(controller.ObserveSnapshot(settings, StandardSnapshot(19.0, 20.0), started_at + std::chrono::seconds{20}),
        "baisse cinq heures non reliee a Motion");
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(19.0, 30.0), started_at + std::chrono::seconds{40}),
        "cooldown Motion ignore");
    Require(controller.ObserveSnapshot(settings, StandardSnapshot(19.0, 35.0), started_at + std::chrono::seconds{81}),
        "declenchement apres cooldown absent");

    settings.enabled = false;
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(30.0, 35.0), started_at + std::chrono::seconds{150}),
        "Motion desactive se declenche");
}

// ----------------------------------------------------------------------------
// Verifie qu'un quota standard disponible suffit lorsque l'autre vaut n/a.
// ----------------------------------------------------------------------------
void TestPartialStandardQuotaTrigger() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.usage_drop_threshold_percent = 5;
    settings.minimum_interval_seconds = 0;
    WidgetVibrationController controller;
    const std::chrono::system_clock::time_point started_at{};

    UsageSnapshot previous = StandardSnapshot(0.0, 40.0);
    previous.five_hour_available = false;
    UsageSnapshot current = previous;
    current.weekly_used_percent = 46.0;
    Require(!controller.ObserveSnapshot(settings, previous, started_at), "premier quota partiel declenche Motion");
    Require(controller.ObserveSnapshot(settings, current, started_at + std::chrono::seconds{1}),
        "quota semaine seul non relie a Motion");
}

// ----------------------------------------------------------------------------
// Verifie qu'une limite supplementaire telle que Spark declenche Motion.
// ----------------------------------------------------------------------------
void TestAdditionalQuotaTrigger() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.usage_drop_threshold_percent = 5;
    settings.minimum_interval_seconds = 0;
    WidgetVibrationController controller;
    const std::chrono::system_clock::time_point started_at{};

    UsageSnapshot previous{};
    previous.freshness = UsageFreshness::Fresh;
    previous.five_hour_available = false;
    previous.weekly_available = false;
    previous.additional_rate_limits.push_back(AdditionalLimit(L"codex-spark", 12.0));
    UsageSnapshot current = previous;
    current.additional_rate_limits.front().primary.used_percent = 18.0;

    Require(!controller.ObserveSnapshot(settings, previous, started_at), "premiere limite Spark declenche Motion");
    Require(controller.ObserveSnapshot(settings, current, started_at + std::chrono::seconds{1}),
        "baisse Spark non reliee a Motion");
}

// ----------------------------------------------------------------------------
// Verifie que les releves invalides ne remplacent pas la derniere base saine.
// ----------------------------------------------------------------------------
void TestInvalidSnapshotIgnored() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.usage_drop_threshold_percent = 5;
    settings.minimum_interval_seconds = 0;
    WidgetVibrationController controller;
    const std::chrono::system_clock::time_point started_at{};

    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(10.0, 20.0), started_at),
        "premier releve valide declenche Motion");
    UsageSnapshot invalid = StandardSnapshot(90.0, 90.0);
    invalid.freshness = UsageFreshness::Error;
    Require(!controller.ObserveSnapshot(settings, invalid, started_at + std::chrono::seconds{1}),
        "releve en erreur declenche Motion");
    Require(controller.ObserveSnapshot(settings, StandardSnapshot(16.0, 20.0), started_at + std::chrono::seconds{2}),
        "releve invalide remplace la base Motion");
}

// ----------------------------------------------------------------------------
// Verifie les declencheurs independants de baisse et de reset du quota.
// ----------------------------------------------------------------------------
void TestIndependentQuotaTriggers() {
    WidgetMotionEffectsSettings settings{};
    settings.enabled = true;
    settings.usage_drop_threshold_percent = 1;
    settings.minimum_interval_seconds = 5;
    WidgetVibrationController controller;
    const std::chrono::system_clock::time_point started_at{};

    settings.trigger_on_usage_drop = false;
    settings.trigger_on_quota_reset = true;
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(10.0, 20.0), started_at),
        "premier releve declenche un reset Motion");
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(12.0, 20.0), started_at + std::chrono::seconds{6}),
        "baisse desactivee declenche Motion");
    Require(controller.ObserveSnapshot(settings, StandardSnapshot(0.0, 20.0), started_at + std::chrono::seconds{12}),
        "reset cinq heures non relie a Motion");

    controller.Reset();
    settings.trigger_on_usage_drop = true;
    settings.trigger_on_quota_reset = false;
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(10.0, 20.0), started_at),
        "premier releve declenche une baisse Motion");
    Require(controller.ObserveSnapshot(settings, StandardSnapshot(11.0, 20.0), started_at + std::chrono::seconds{6}),
        "baisse de un pour cent non reliee a Motion");
    Require(!controller.ObserveSnapshot(settings, StandardSnapshot(0.0, 20.0), started_at + std::chrono::seconds{12}),
        "reset desactive declenche Motion");
}

} // namespace

// ----------------------------------------------------------------------------
// Execute les tests du controleur et retourne un code compatible CTest.
// ----------------------------------------------------------------------------
int main() {
    try {
        TestStandardQuotaTrigger();
        TestPartialStandardQuotaTrigger();
        TestAdditionalQuotaTrigger();
        TestInvalidSnapshotIgnored();
        TestIndependentQuotaTriggers();
        std::cout << "WidgetVibrationControllerTests: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WidgetVibrationControllerTests: " << error.what() << '\n';
        return 1;
    }
}

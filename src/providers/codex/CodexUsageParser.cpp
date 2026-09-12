// ============================================================================
// Codex Glass - Parsing des donnees d'usage Codex
// ----------------------------------------------------------------------------
// Ce fichier parse independamment chaque bloc de la reponse Codex afin qu'un
// champ optionnel invalide ne masque jamais les quotas principaux valides.
// ============================================================================

#include "CodexUsageParser.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>

namespace {

// Duree maximale consideree comme une fenetre courte.
constexpr int kShortWindowMaxSeconds = 12 * 60 * 60;

// Duree de secours du reset de session.
constexpr std::chrono::hours kFallbackFiveHourResetDelay{5};

// Duree de secours du reset hebdomadaire.
constexpr std::chrono::hours kFallbackWeeklyResetDelay{24 * 7};

// ----------------------------------------------------------------------------
// Convertit une chaine UTF-8 en UTF-16.
// ----------------------------------------------------------------------------
std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring output(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), size);
    return output;
}

// ----------------------------------------------------------------------------
// Lit une chaine optionnelle.
// ----------------------------------------------------------------------------
std::optional<std::string> ReadString(const nlohmann::json& object, const char* key) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_string()) {
        return std::nullopt;
    }
    return iterator->get<std::string>();
}

// ----------------------------------------------------------------------------
// Lit un booleen optionnel.
// ----------------------------------------------------------------------------
std::optional<bool> ReadBool(const nlohmann::json& object, const char* key) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_boolean()) {
        return std::nullopt;
    }
    return iterator->get<bool>();
}

// ----------------------------------------------------------------------------
// Lit un nombre accepte sous forme JSON numerique ou textuelle.
// ----------------------------------------------------------------------------
std::optional<double> ReadFlexibleDouble(const nlohmann::json& object, const char* key) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || iterator->is_null()) {
        return std::nullopt;
    }
    if (iterator->is_number()) {
        return iterator->get<double>();
    }
    if (iterator->is_string()) {
        try {
            size_t consumed = 0;
            const std::string text = iterator->get<std::string>();
            const double value = std::stod(text, &consumed);
            return consumed == text.size() ? std::optional<double>{value} : std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Lit un entier accepte sous forme numerique ou textuelle.
// ----------------------------------------------------------------------------
std::optional<long long> ReadFlexibleInteger(const nlohmann::json& object, const char* key) {
    const auto value = ReadFlexibleDouble(object, key);
    if (!value.has_value() || !std::isfinite(*value)) {
        return std::nullopt;
    }
    return static_cast<long long>(*value);
}

// ----------------------------------------------------------------------------
// Parse une fenetre de limite independante.
// ----------------------------------------------------------------------------
ProviderRateLimitWindow ParseWindow(const nlohmann::json& value) {
    ProviderRateLimitWindow window{};
    if (!value.is_object()) {
        return window;
    }
    const auto used = ReadFlexibleDouble(value, "used_percent");
    const auto seconds = ReadFlexibleInteger(value, "limit_window_seconds");
    if (!used.has_value()) {
        return window;
    }
    window.available = true;
    window.used_percent = std::clamp(*used, 0.0, 100.0);
    window.window_seconds = seconds.has_value() && *seconds > 0
        ? static_cast<int>(std::min<long long>(*seconds, INT_MAX))
        : 0;
    if (const auto reset = ReadFlexibleInteger(value, "reset_at"); reset.has_value() && *reset > 0) {
        window.reset_at = std::chrono::system_clock::time_point{std::chrono::seconds{*reset}};
    }
    return window;
}

// ----------------------------------------------------------------------------
// Lit une fenetre nommee dans un objet de rate limit.
// ----------------------------------------------------------------------------
ProviderRateLimitWindow ParseNamedWindow(const nlohmann::json& rate_limit, const char* key) {
    if (!rate_limit.is_object()) {
        return {};
    }
    const auto iterator = rate_limit.find(key);
    return iterator == rate_limit.end() ? ProviderRateLimitWindow{} : ParseWindow(*iterator);
}

// ----------------------------------------------------------------------------
// Indique si une fenetre correspond vraisemblablement a une session courte.
// ----------------------------------------------------------------------------
bool IsShortWindow(const ProviderRateLimitWindow& window) {
    return window.available && window.window_seconds > 0 && window.window_seconds <= kShortWindowMaxSeconds;
}

// ----------------------------------------------------------------------------
// Parse les credits optionnels.
// ----------------------------------------------------------------------------
ProviderCredits ParseCredits(const nlohmann::json& root) {
    ProviderCredits credits{};
    const auto iterator = root.find("credits");
    if (iterator == root.end() || !iterator->is_object()) {
        return credits;
    }
    credits.available = true;
    credits.has_credits = ReadBool(*iterator, "has_credits").value_or(false);
    credits.unlimited = ReadBool(*iterator, "unlimited").value_or(false);
    credits.balance = ReadFlexibleDouble(*iterator, "balance");
    return credits;
}

// ----------------------------------------------------------------------------
// Parse un controle de depense optionnel.
// ----------------------------------------------------------------------------
std::optional<ProviderSpendControl> ParseSpendControlObject(const nlohmann::json& value) {
    if (!value.is_object()) {
        return std::nullopt;
    }
    ProviderSpendControl control{};
    control.limit = ReadFlexibleDouble(value, "limit");
    control.used = ReadFlexibleDouble(value, "used");
    control.remaining_percent = ReadFlexibleDouble(value, "remaining_percent");
    if (const auto reset_at = ReadFlexibleInteger(value, "reset_at"); reset_at.has_value() && *reset_at > 0) {
        control.reset_at = std::chrono::system_clock::time_point{std::chrono::seconds{*reset_at}};
    } else if (const auto resets_at = ReadFlexibleInteger(value, "resets_at"); resets_at.has_value() && *resets_at > 0) {
        control.reset_at = std::chrono::system_clock::time_point{std::chrono::seconds{*resets_at}};
    }
    if (!control.limit.has_value() && !control.used.has_value() && !control.remaining_percent.has_value()
        && !control.reset_at.has_value()) {
        return std::nullopt;
    }
    return control;
}

// ----------------------------------------------------------------------------
// Resout le controle de depense selon la precedence de la reponse Codex.
// ----------------------------------------------------------------------------
std::optional<ProviderSpendControl> ParseSpendControl(const nlohmann::json& root) {
    if (const auto iterator = root.find("individual_limit"); iterator != root.end()) {
        if (const auto parsed = ParseSpendControlObject(*iterator); parsed.has_value()) {
            return parsed;
        }
    }
    if (const auto rate = root.find("rate_limit"); rate != root.end() && rate->is_object()) {
        if (const auto iterator = rate->find("individual_limit"); iterator != rate->end()) {
            if (const auto parsed = ParseSpendControlObject(*iterator); parsed.has_value()) {
                return parsed;
            }
        }
    }
    for (const char* key : {"spend_control", "spendControl"}) {
        const auto spend = root.find(key);
        if (spend == root.end() || !spend->is_object()) {
            continue;
        }
        for (const char* limit_key : {"individual_limit", "individualLimit"}) {
            const auto limit = spend->find(limit_key);
            if (limit != spend->end()) {
                if (const auto parsed = ParseSpendControlObject(*limit); parsed.has_value()) {
                    return parsed;
                }
            }
        }
    }
    return std::nullopt;
}

// ----------------------------------------------------------------------------
// Produit un identifiant stable a partir d'un texte provider.
// ----------------------------------------------------------------------------
std::wstring StableLimitId(const std::wstring& source, size_t index) {
    std::wstring id;
    id.reserve(source.size());
    for (const wchar_t character : source) {
        if ((character >= L'a' && character <= L'z') || (character >= L'0' && character <= L'9')) {
            id.push_back(character);
        } else if (character >= L'A' && character <= L'Z') {
            id.push_back(static_cast<wchar_t>(character - L'A' + L'a'));
        } else if (!id.empty() && id.back() != L'-') {
            id.push_back(L'-');
        }
    }
    while (!id.empty() && id.back() == L'-') {
        id.pop_back();
    }
    return id.empty() ? L"additional-" + std::to_wstring(index + 1) : id;
}

// ----------------------------------------------------------------------------
// Parse independamment les limites supplementaires valides.
// ----------------------------------------------------------------------------
std::vector<ProviderAdditionalRateLimit> ParseAdditionalLimits(const nlohmann::json& root) {
    std::vector<ProviderAdditionalRateLimit> result;
    const auto iterator = root.find("additional_rate_limits");
    if (iterator == root.end() || !iterator->is_array()) {
        return result;
    }
    size_t index = 0;
    for (const auto& value : *iterator) {
        if (!value.is_object()) {
            ++index;
            continue;
        }
        const auto rate = value.find("rate_limit");
        if (rate == value.end() || !rate->is_object()) {
            ++index;
            continue;
        }
        ProviderAdditionalRateLimit limit{};
        const auto name = ReadString(value, "limit_name");
        const auto feature = ReadString(value, "metered_feature");
        limit.label = Utf8ToWide(name.value_or(feature.value_or("Limite supplementaire")));
        if (feature.has_value()) {
            limit.metered_feature = Utf8ToWide(*feature);
        }
        limit.id = StableLimitId(Utf8ToWide(feature.value_or(name.value_or(""))), index);
        limit.primary = ParseNamedWindow(*rate, "primary_window");
        limit.secondary = ParseNamedWindow(*rate, "secondary_window");
        if (limit.primary.available || limit.secondary.available) {
            result.push_back(std::move(limit));
        }
        ++index;
    }
    return result;
}

} // namespace

// ----------------------------------------------------------------------------
// Parse une reponse complete de l'endpoint d'usage.
// ----------------------------------------------------------------------------
std::optional<UsageSnapshot> CodexUsageParser::Parse(
    const std::string& json_text,
    std::wstring* error_message
) {
    const nlohmann::json root = nlohmann::json::parse(json_text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        if (error_message != nullptr) {
            *error_message = L"JSON usage Codex invalide";
        }
        return std::nullopt;
    }

    const auto rate_iterator = root.find("rate_limit");
    const nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json& rate_limit = rate_iterator != root.end() && rate_iterator->is_object()
        ? *rate_iterator
        : empty;
    ProviderRateLimitWindow primary = ParseNamedWindow(rate_limit, "primary_window");
    ProviderRateLimitWindow secondary = ParseNamedWindow(rate_limit, "secondary_window");

    ProviderRateLimitWindow five_hour{};
    ProviderRateLimitWindow weekly{};
    if (primary.available && secondary.available) {
        if (IsShortWindow(primary) && !IsShortWindow(secondary)) {
            five_hour = primary;
            weekly = secondary;
        } else if (!IsShortWindow(primary) && IsShortWindow(secondary)) {
            weekly = primary;
            five_hour = secondary;
        } else if (primary.window_seconds <= secondary.window_seconds) {
            five_hour = primary;
            weekly = secondary;
        } else {
            weekly = primary;
            five_hour = secondary;
        }
    } else if (primary.available) {
        (IsShortWindow(primary) ? five_hour : weekly) = primary;
    } else if (secondary.available) {
        (IsShortWindow(secondary) ? five_hour : weekly) = secondary;
    }

    if (!five_hour.available && !weekly.available) {
        if (error_message != nullptr) {
            *error_message = L"Format usage Codex inattendu";
        }
        return std::nullopt;
    }

    const auto now = std::chrono::system_clock::now();
    UsageSnapshot snapshot{};
    snapshot.sampled_at = now;
    snapshot.freshness = UsageFreshness::Fresh;
    snapshot.identity.provider_id = UsageProviderId::Codex;
    if (const auto plan = ReadString(root, "plan_type"); plan.has_value() && !plan->empty()) {
        snapshot.identity.plan_type = Utf8ToWide(*plan);
    }
    snapshot.credits = ParseCredits(root);
    snapshot.additional_rate_limits = ParseAdditionalLimits(root);
    snapshot.spend_control = ParseSpendControl(root);
    snapshot.five_hour_available = five_hour.available;
    snapshot.weekly_available = weekly.available;
    snapshot.five_hour_used_percent = five_hour.available ? five_hour.used_percent : 0.0;
    snapshot.weekly_used_percent = weekly.available ? weekly.used_percent : 0.0;
    snapshot.five_hour_reset_at = five_hour.reset_at.value_or(now + kFallbackFiveHourResetDelay);
    snapshot.weekly_reset_at = weekly.reset_at.value_or(now + kFallbackWeeklyResetDelay);
    return snapshot;
}

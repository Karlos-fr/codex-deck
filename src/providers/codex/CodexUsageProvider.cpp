// ============================================================================
// Codex Glass - Provider d'usage Codex
// ----------------------------------------------------------------------------
// Ce fichier orchestre les modules Codex sans contenir de parsing JSON, de
// lecture de fichier ou de details WinHTTP.
// ============================================================================

#include "CodexUsageProvider.h"

#include "CodexAuthReader.h"
#include "CodexUsageParser.h"

namespace {

// ----------------------------------------------------------------------------
// Construit un snapshot d'erreur sans valeur fictive.
// ----------------------------------------------------------------------------
UsageSnapshot MakeUnavailableSnapshot(const std::wstring& error_message) {
    UsageSnapshot snapshot{};
    snapshot.identity.provider_id = UsageProviderId::Codex;
    snapshot.sampled_at = std::chrono::system_clock::now();
    snapshot.five_hour_available = false;
    snapshot.weekly_available = false;
    snapshot.freshness = UsageFreshness::Error;
    snapshot.error_message = error_message.empty() ? L"Donnees reelles indisponibles" : error_message;
    return snapshot;
}

} // namespace

// ----------------------------------------------------------------------------
// Retourne l'identifiant du provider Codex.
// ----------------------------------------------------------------------------
UsageProviderId CodexUsageProvider::ProviderId() const {
    return UsageProviderId::Codex;
}

// ----------------------------------------------------------------------------
// Recupere un snapshot distant complet.
// ----------------------------------------------------------------------------
UsageSnapshot CodexUsageProvider::FetchUsage() {
    std::wstring error_message;
    const auto credentials = CodexAuthReader::Read(&error_message);
    if (!credentials.has_value()) {
        return MakeUnavailableSnapshot(error_message);
    }
    const auto json = client_.Fetch(*credentials, &error_message);
    if (!json.has_value()) {
        return MakeUnavailableSnapshot(error_message);
    }
    const auto snapshot = CodexUsageParser::Parse(*json, &error_message);
    return snapshot.value_or(MakeUnavailableSnapshot(error_message));
}

// ----------------------------------------------------------------------------
// Annule l'appel HTTP actif.
// ----------------------------------------------------------------------------
void CodexUsageProvider::Cancel() {
    client_.Cancel();
}

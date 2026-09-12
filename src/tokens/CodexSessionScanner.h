// ============================================================================
// Codex Glass - Scanner des sessions locales Codex
// ----------------------------------------------------------------------------
// Ce fichier declare l'analyse des JSONL de sessions Codex. Le scanner ne lit
// aucun secret d'authentification et ne realise aucun appel reseau.
// ============================================================================

#pragma once

#include "TokenUsageTypes.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <set>
#include <string>

// ----------------------------------------------------------------------------
// Etat persistant d'un fichier JSONL necessaire a une reprise incrementale.
// ----------------------------------------------------------------------------
struct TokenFileScanCache {
    // Chemin absolu normalise du fichier.
    std::filesystem::path path;

    // Taille observee lors du dernier scan valide.
    std::uintmax_t file_size = 0;

    // Horodatage technique de derniere modification.
    std::int64_t modified_ticks = 0;

    // Position suivant la derniere ligne JSON complete.
    std::uintmax_t valid_offset = 0;

    // Identifiant de session technique.
    std::wstring session_id;

    // Dernier modele observe.
    std::wstring model;

    // Dernier compteur cumulatif necessaire au prochain delta.
    TokenUsageCounts previous_total{};

    // Indique si le compteur cumulatif precedent est exploitable.
    bool has_previous_total = false;

    // Agregats quotidiens deja produits par ce fichier.
    std::map<std::string, TokenDailyUsage> days;

    // Agregats horaires recents indexes par leur debut UTC en secondes.
    std::map<std::int64_t, TokenHourlyUsage> hours;

    // Agregats recents de cinq minutes indexes par leur debut UTC en secondes.
    std::map<std::int64_t, TokenFiveMinuteUsage> five_minutes;

    // Agregat de session deja produit par ce fichier.
    TokenSessionUsage session{};

    // Identifiants techniques d'evenements deja comptes dans ce fichier.
    std::set<std::string> seen_events;
};

// ----------------------------------------------------------------------------
// Cache de scan indexe par chemin normalise.
// ----------------------------------------------------------------------------
struct TokenScanCache {
    // Entrees connues au dernier scan.
    std::map<std::wstring, TokenFileScanCache> files;
};

// ----------------------------------------------------------------------------
// Analyse les sessions actives et archivees d'un CODEX_HOME.
// ----------------------------------------------------------------------------
class CodexSessionScanner {
public:
    // ------------------------------------------------------------------------
    // Cree un scanner pour le profil Codex actif.
    // ------------------------------------------------------------------------
    CodexSessionScanner();

    // ------------------------------------------------------------------------
    // Cree un scanner pour un profil explicite, notamment dans les tests.
    // ------------------------------------------------------------------------
    explicit CodexSessionScanner(std::filesystem::path codex_home);

    // ------------------------------------------------------------------------
    // Resolut le repertoire CODEX_HOME actif sans lire auth.json.
    // ------------------------------------------------------------------------
    static std::filesystem::path ResolveCodexHome();

    // ------------------------------------------------------------------------
    // Analyse une fenetre de jours jusqu'a l'instant fourni.
    // ------------------------------------------------------------------------
    TokenUsageSnapshot Scan(
        std::chrono::system_clock::time_point now,
        int coverage_days,
        const std::atomic_bool* cancellation_requested = nullptr,
        TokenScanCache* cache = nullptr
    ) const;

private:
    // Repertoire racine contenant sessions et archived_sessions.
    std::filesystem::path codex_home_;
};

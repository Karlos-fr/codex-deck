// ============================================================================
// Codex Glass - Stockage historique des releves d'usage
// ----------------------------------------------------------------------------
// Ce fichier declare la couche SQLite qui conserve les releves Codex locaux et
// applique l'anti-doublon utilise par les graphes.
// ============================================================================

#pragma once

#include "UsageSnapshot.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

// ----------------------------------------------------------------------------
// Represente un sample historique lu depuis SQLite pour le graphe.
// ----------------------------------------------------------------------------
struct UsageHistorySample {
    // Date et heure du sample historise.
    std::chrono::system_clock::time_point sampled_at{};

    // Pourcentage utilise sur la fenetre 5 h si disponible.
    std::optional<double> five_hour_used_percent;

    // Pourcentage utilise sur la fenetre hebdomadaire si disponible.
    std::optional<double> weekly_used_percent;

    // Date de reset 5 h si disponible.
    std::optional<std::chrono::system_clock::time_point> five_hour_reset_at;

    // Date de reset hebdomadaire si disponible.
    std::optional<std::chrono::system_clock::time_point> weekly_reset_at;
};

// ----------------------------------------------------------------------------
// Gere la base SQLite locale contenant l'historique d'usage Codex.
// ----------------------------------------------------------------------------
class UsageHistoryStore {
public:
    // ------------------------------------------------------------------------
    // Cree un store sans connexion SQLite ouverte.
    // ------------------------------------------------------------------------
    UsageHistoryStore() = default;

    // ------------------------------------------------------------------------
    // Ferme automatiquement la base SQLite encore ouverte.
    // ------------------------------------------------------------------------
    ~UsageHistoryStore();

    // ------------------------------------------------------------------------
    // Interdit la copie pour eviter deux proprietaires du meme handle SQLite.
    // ------------------------------------------------------------------------
    UsageHistoryStore(const UsageHistoryStore&) = delete;

    // ------------------------------------------------------------------------
    // Interdit l'affectation par copie pour proteger le handle SQLite.
    // ------------------------------------------------------------------------
    UsageHistoryStore& operator=(const UsageHistoryStore&) = delete;

    // ------------------------------------------------------------------------
    // Interdit le deplacement pour garder la duree de vie du handle explicite.
    // ------------------------------------------------------------------------
    UsageHistoryStore(UsageHistoryStore&&) = delete;

    // ------------------------------------------------------------------------
    // Interdit l'affectation par deplacement pour garder le handle stable.
    // ------------------------------------------------------------------------
    UsageHistoryStore& operator=(UsageHistoryStore&&) = delete;

    // ------------------------------------------------------------------------
    // Ouvre ou cree la base SQLite puis initialise son schema.
    //
    // Parametres :
    // - database_path : chemin complet du fichier SQLite.
    //
    // Retour :
    // - true si la base est prete.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool Open(const std::wstring& database_path);

    // ------------------------------------------------------------------------
    // Ferme la connexion SQLite courante.
    // ------------------------------------------------------------------------
    void Close();

    // ------------------------------------------------------------------------
    // Indique si la base SQLite est actuellement ouverte.
    //
    // Retour :
    // - true si une connexion SQLite est disponible.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool IsOpen() const;

    // ------------------------------------------------------------------------
    // Enregistre un releve si les valeurs changent ou si un checkpoint est du.
    //
    // Parametres :
    // - snapshot : releve d'usage a considerer.
    //
    // Retour :
    // - true si le traitement s'est termine sans erreur SQLite.
    // - false sinon.
    // ------------------------------------------------------------------------
    bool RecordSnapshot(const UsageSnapshot& snapshot);

    // ------------------------------------------------------------------------
    // Charge les samples historiques depuis une date minimale.
    //
    // Parametres :
    // - since : date minimale incluse.
    // - max_samples : nombre maximal de samples retournes.
    //
    // Retour :
    // - liste ordonnee de samples historiques.
    // ------------------------------------------------------------------------
    std::vector<UsageHistorySample> LoadSamplesSince(
        std::chrono::system_clock::time_point since,
        int max_samples
    ) const;

    // ------------------------------------------------------------------------
    // Charge une plage et prolonge le dernier sample connu jusqu'a son debut.
    //
    // Parametres :
    // - range_start : borne temporelle gauche incluse.
    // - max_samples : nombre maximal de samples reels retournes.
    //
    // Retour :
    // - liste ordonnee, eventuellement precedee d'un point de continuite.
    // ------------------------------------------------------------------------
    std::vector<UsageHistorySample> LoadSamplesForRange(
        std::chrono::system_clock::time_point range_start,
        int max_samples
    ) const;

    // ------------------------------------------------------------------------
    // Retourne la derniere erreur lisible produite par le store.
    //
    // Retour :
    // - message d'erreur courant, ou chaine vide.
    // ------------------------------------------------------------------------
    const std::wstring& LastError() const;

private:
    // Connexion SQLite ouverte sur la base locale.
    sqlite3* database_ = nullptr;

    // Derniere erreur fonctionnelle ou SQLite exposee a l'appelant.
    mutable std::wstring last_error_;
};

// ----------------------------------------------------------------------------
// Retourne le chemin portable de la base historique.
//
// Retour :
// - chemin complet vers CodexGlass.db a cote de l'executable.
// ----------------------------------------------------------------------------
std::wstring GetUsageHistoryDatabasePath();

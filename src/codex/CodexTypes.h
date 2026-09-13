// ============================================================================
// Codex Deck - Types stables Codex
// ----------------------------------------------------------------------------
// Ce module contient les modeles internes exposes par la couche app-server. Les
// payloads complets restent a la frontiere Codex et ne sont pas persistables.
// ============================================================================

#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

// Identifiant stable d'un thread Codex.
using CodexThreadId = std::string;

// Identifiant stable d'un tour Codex.
using CodexTurnId = std::string;

// ----------------------------------------------------------------------------
// Modele disponible expose par le catalogue Codex.
// ----------------------------------------------------------------------------
struct CodexModelInfo {
    // Identifiant wire stable du modele.
    std::string id;

    // Nom humain affiche par Codex.
    std::string display_name;

    // Indique le choix par defaut du serveur.
    bool is_default = false;

    // Efforts de raisonnement reellement proposes.
    std::vector<std::string> supported_efforts;
};

// ----------------------------------------------------------------------------
// Resume leger d'un thread retourne par les listes Codex.
// ----------------------------------------------------------------------------
struct CodexThreadSummary {
    // Identifiant Codex du thread.
    CodexThreadId id;

    // Nom affiche par Codex.
    std::string name;

    // Repertoire de travail associe au thread.
    std::filesystem::path cwd;

    // Timestamp de creation fourni par Codex.
    std::int64_t created_at = 0;

    // Timestamp de derniere activite fourni par Codex.
    std::int64_t updated_at = 0;

    // Indique si le thread appartient aux archives.
    bool archived = false;
};

// ----------------------------------------------------------------------------
// Capture d'un tour dans le detail d'un thread.
// ----------------------------------------------------------------------------
struct CodexTurnSnapshot {
    // Identifiant Codex du tour.
    std::string id;

    // Statut brut du tour tel que renvoye par Codex.
    std::string status;

    // Items bruts gardes a la frontiere Workbench.
    std::vector<nlohmann::json> items;
};

// ----------------------------------------------------------------------------
// Detail complet d'un thread lu ou repris.
// ----------------------------------------------------------------------------
struct CodexThreadDetail {
    // Resume stable du thread.
    CodexThreadSummary summary;

    // Tours actuellement connus pour ce thread.
    std::vector<CodexTurnSnapshot> turns;
};

// ----------------------------------------------------------------------------
// Etats runtime partages entre les vues Codex Deck.
// ----------------------------------------------------------------------------
enum class SessionStatus {
    // Aucun travail actif connu.
    Idle,

    // Un tour ou outil est en cours.
    Working,

    // Codex attend une decision utilisateur.
    NeedsAttention,

    // Le dernier tour s'est termine correctement.
    Completed,

    // Le dernier evenement signale une erreur.
    Error,
};

// ----------------------------------------------------------------------------
// Notification JSON-RPC recue sans identifiant de requete.
// ----------------------------------------------------------------------------
struct CodexNotification {
    // Methode de notification.
    std::string method;

    // Parametres bruts de la notification.
    nlohmann::json params;
};

// ----------------------------------------------------------------------------
// Requete envoyee par le serveur et necessitant une reponse.
// ----------------------------------------------------------------------------
struct CodexServerRequest {
    // Identifiant JSON-RPC a reutiliser pour la reponse.
    nlohmann::json id;

    // Methode de la requete serveur.
    std::string method;

    // Parametres bruts de la requete.
    nlohmann::json params;
};

// Reponse RPC brute validee au niveau enveloppe, notification ou requete serveur.
using CodexInboundMessage = std::variant<CodexNotification, CodexServerRequest, nlohmann::json>;

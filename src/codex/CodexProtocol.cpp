// ============================================================================
// Codex Deck - Implementation du parsing app-server
// ----------------------------------------------------------------------------
// Ce fichier applique le contrat tolerant entre JSON-RPC app-server et les
// modeles internes utilises par Codex Deck.
// ============================================================================

#include "CodexProtocol.h"

namespace {

// ----------------------------------------------------------------------------
// Construit une erreur de protocole.
//
// Parametres :
// - message : diagnostic court.
//
// Retour :
// - erreur InvalidResponse.
// ----------------------------------------------------------------------------
CodexError InvalidResponse(const wchar_t* message) {
    return CodexError{CodexErrorCode::InvalidResponse, message};
}

// ----------------------------------------------------------------------------
// Lit une chaine optionnelle.
//
// Parametres :
// - payload : objet JSON source.
// - key : nom du champ.
//
// Retour :
// - chaine lue ou chaine vide.
// ----------------------------------------------------------------------------
std::string OptionalString(const nlohmann::json& payload, const char* key) {
    const auto found = payload.find(key);
    return found != payload.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

// ----------------------------------------------------------------------------
// Lit un entier optionnel.
//
// Parametres :
// - payload : objet JSON source.
// - key : nom du champ.
//
// Retour :
// - entier lu ou zero.
// ----------------------------------------------------------------------------
std::int64_t OptionalInt64(const nlohmann::json& payload, const char* key) {
    const auto found = payload.find(key);
    return found != payload.end() && found->is_number_integer() ? found->get<std::int64_t>() : 0;
}

// ----------------------------------------------------------------------------
// Lit un booleen optionnel.
//
// Parametres :
// - payload : objet JSON source.
// - key : nom du champ.
//
// Retour :
// - booleen lu ou false.
// ----------------------------------------------------------------------------
bool OptionalBool(const nlohmann::json& payload, const char* key) {
    const auto found = payload.find(key);
    return found != payload.end() && found->is_boolean() ? found->get<bool>() : false;
}

}  // namespace

// ----------------------------------------------------------------------------
// Parse un resume de thread Codex de facon tolerante.
// ----------------------------------------------------------------------------
std::expected<CodexThreadSummary, CodexError> ParseThreadSummary(const nlohmann::json& payload) {
    if (!payload.is_object()) {
        return std::unexpected(InvalidResponse(L"Le resume de thread doit etre un objet"));
    }

    const auto id = payload.find("id");
    if (id == payload.end() || !id->is_string() || id->get<std::string>().empty()) {
        return std::unexpected(InvalidResponse(L"Le resume de thread ne contient pas d'identifiant valide"));
    }

    CodexThreadSummary summary{};
    summary.id = id->get<std::string>();
    summary.name = OptionalString(payload, "name");
    summary.cwd = std::filesystem::path(OptionalString(payload, "cwd"));
    summary.created_at = OptionalInt64(payload, "createdAt");
    summary.updated_at = OptionalInt64(payload, "updatedAt");
    summary.archived = OptionalBool(payload, "archived");
    return summary;
}

// ----------------------------------------------------------------------------
// Parse une enveloppe JSON-RPC entrante.
// ----------------------------------------------------------------------------
std::expected<CodexInboundMessage, CodexError> ParseServerMessage(const nlohmann::json& payload) {
    if (!payload.is_object()) {
        return std::unexpected(InvalidResponse(L"Le message JSON-RPC doit etre un objet"));
    }

    const bool has_method = payload.contains("method");
    const bool has_id = payload.contains("id");
    const bool has_result_or_error = payload.contains("result") || payload.contains("error");

    if (has_method) {
        const auto method = payload.find("method");
        if (!method->is_string() || method->get<std::string>().empty()) {
            return std::unexpected(InvalidResponse(L"La methode JSON-RPC est invalide"));
        }

        const nlohmann::json params = payload.contains("params") ? payload.at("params") : nlohmann::json::object();
        if (has_id) {
            return CodexInboundMessage{CodexServerRequest{payload.at("id"), method->get<std::string>(), params}};
        }
        return CodexInboundMessage{CodexNotification{method->get<std::string>(), params}};
    }

    if (has_id && has_result_or_error) {
        return CodexInboundMessage{payload};
    }

    return std::unexpected(InvalidResponse(L"L'enveloppe JSON-RPC ne correspond a aucun type connu"));
}

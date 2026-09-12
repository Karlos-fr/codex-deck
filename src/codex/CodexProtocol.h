// ============================================================================
// Codex Deck - Parsing du protocole app-server
// ----------------------------------------------------------------------------
// Ce module transforme les payloads JSON app-server en types internes legers.
// Les champs inconnus sont ignores pour rester compatible avec les evolutions.
// ============================================================================

#pragma once

#include "CodexError.h"
#include "CodexTypes.h"

#include <nlohmann/json.hpp>

#include <expected>

// ----------------------------------------------------------------------------
// Parse un resume de thread Codex de facon tolerante.
//
// Parametres :
// - payload : objet JSON de thread.
//
// Retour :
// - resume de thread ou erreur si l'identifiant requis est absent.
// ----------------------------------------------------------------------------
std::expected<CodexThreadSummary, CodexError> ParseThreadSummary(const nlohmann::json& payload);

// ----------------------------------------------------------------------------
// Parse une enveloppe JSON-RPC entrante.
//
// Parametres :
// - payload : objet JSON recu sur stdout.
//
// Retour :
// - message route ou erreur si l'enveloppe est invalide.
// ----------------------------------------------------------------------------
std::expected<CodexInboundMessage, CodexError> ParseServerMessage(const nlohmann::json& payload);

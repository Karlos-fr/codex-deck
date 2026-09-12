// ============================================================================
// Codex Deck - Tests du protocole app-server
// ----------------------------------------------------------------------------
// Ce fichier valide le parsing tolerant des objets Codex et des enveloppes
// JSON-RPC sans lancer de processus externe.
// ============================================================================

#include "codex/CodexProtocol.h"

#include <nlohmann/json.hpp>

#include <variant>

// ----------------------------------------------------------------------------
// Verifie le parsing tolerant d'un resume de thread.
//
// Retour :
// - zero lorsque les champs connus sont lus et les champs futurs ignores.
// ----------------------------------------------------------------------------
int TestThreadSummaryParsing() {
    const nlohmann::json payload = {
        {"id", "thr_123"},
        {"name", "Audio parity"},
        {"cwd", "D:\\VibeCoding\\spotifyamp"},
        {"createdAt", 1'757'000'000},
        {"updatedAt", 1'757'000'120},
        {"status", "idle"},
        {"futureField", {{"ignored", true}}},
    };

    const auto parsed = ParseThreadSummary(payload);
    if (!parsed) {
        return 1;
    }
    if (parsed->id != "thr_123") {
        return 2;
    }
    if (parsed->name != "Audio parity") {
        return 3;
    }
    if (parsed->cwd != std::filesystem::path("D:\\VibeCoding\\spotifyamp")) {
        return 4;
    }
    if (parsed->created_at != 1'757'000'000 || parsed->updated_at != 1'757'000'120) {
        return 5;
    }
    if (parsed->archived) {
        return 6;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Verifie le routage d'une reponse JSON-RPC brute.
//
// Retour :
// - zero lorsque l'enveloppe est reconnue comme reponse.
// ----------------------------------------------------------------------------
int TestRpcResponseParsing() {
    const nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 7},
        {"result", {{"ok", true}}},
    };
    const auto parsed = ParseServerMessage(payload);
    if (!parsed) {
        return 10;
    }
    if (!std::holds_alternative<nlohmann::json>(*parsed)) {
        return 11;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Verifie le routage d'une notification JSON-RPC.
//
// Retour :
// - zero lorsque la notification est exposee avec sa methode et ses params.
// ----------------------------------------------------------------------------
int TestNotificationParsing() {
    const nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"method", "turn/completed"},
        {"params", {{"threadId", "thr_123"}}},
    };
    const auto parsed = ParseServerMessage(payload);
    if (!parsed) {
        return 20;
    }
    const auto* notification = std::get_if<CodexNotification>(&*parsed);
    if (notification == nullptr) {
        return 21;
    }
    if (notification->method != "turn/completed") {
        return 22;
    }
    if (notification->params.at("threadId") != "thr_123") {
        return 23;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Verifie le routage d'une requete serveur d'approbation.
//
// Retour :
// - zero lorsque l'id JSON-RPC est conserve pour la reponse future.
// ----------------------------------------------------------------------------
int TestServerRequestParsing() {
    const nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", 12},
        {"method", "item/commandExecution/requestApproval"},
        {"params", {{"threadId", "thr_123"}, {"title", "Run tests"}}},
    };
    const auto parsed = ParseServerMessage(payload);
    if (!parsed) {
        return 30;
    }
    const auto* request = std::get_if<CodexServerRequest>(&*parsed);
    if (request == nullptr) {
        return 31;
    }
    if (request->id != 12) {
        return 32;
    }
    if (request->method != "item/commandExecution/requestApproval") {
        return 33;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Execute les tests de parsing du protocole.
//
// Retour :
// - zero si tous les cas attendus sont valides.
// ----------------------------------------------------------------------------
int main() {
    if (const int result = TestThreadSummaryParsing(); result != 0) {
        return result;
    }
    if (const int result = TestRpcResponseParsing(); result != 0) {
        return result;
    }
    if (const int result = TestNotificationParsing(); result != 0) {
        return result;
    }
    if (const int result = TestServerRequestParsing(); result != 0) {
        return result;
    }
    return 0;
}

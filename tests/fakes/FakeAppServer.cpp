// ============================================================================
// Codex Deck - Faux app-server de test
// ----------------------------------------------------------------------------
// Ce binaire simule un sous-ensemble JSON-RPC de codex app-server afin de tester
// pipes, transport et facade sans consommer une installation Codex reelle.
// ============================================================================

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Retourne le thread de fixture commun aux tests.
//
// Retour :
// - objet JSON representant un resume de thread.
// ----------------------------------------------------------------------------
nlohmann::json FixtureThread() {
    return {
        {"id", "thr_123"},
        {"name", "Audio parity"},
        {"cwd", "D:\\VibeCoding\\spotifyamp"},
        {"createdAt", 1'757'000'000},
        {"updatedAt", 1'757'000'120},
        {"archived", false},
    };
}

// ----------------------------------------------------------------------------
// Ecrit une reponse JSON-RPC sur une seule ligne.
//
// Parametres :
// - response : enveloppe a serialiser.
// ----------------------------------------------------------------------------
void WriteResponse(const nlohmann::json& response) {
    std::cout << response.dump() << '\n';
    std::cout.flush();
}

// ----------------------------------------------------------------------------
// Construit une reponse de succes.
//
// Parametres :
// - id : identifiant de la requete.
// - result : resultat JSON-RPC.
//
// Retour :
// - enveloppe JSON-RPC.
// ----------------------------------------------------------------------------
nlohmann::json Success(const nlohmann::json& id, nlohmann::json result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

}  // namespace

// ----------------------------------------------------------------------------
// Execute le faux serveur app-server.
//
// Retour :
// - code de sortie du processus.
// ----------------------------------------------------------------------------
int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        nlohmann::json request = nlohmann::json::parse(line, nullptr, false);
        if (request.is_discarded() || !request.is_object()) {
            continue;
        }

        const nlohmann::json id = request.value("id", nlohmann::json(nullptr));
        const std::string method = request.value("method", std::string{});
        if (method == "initialize") {
            WriteResponse(Success(id, {{"serverInfo", {{"name", "fake-codex-app-server"}}}}));
        } else if (method == "thread/list") {
            WriteResponse(Success(id, {{"data", nlohmann::json::array({FixtureThread()})}, {"nextCursor", nullptr}}));
        } else if (method == "thread/read") {
            WriteResponse(Success(id, {{"thread", FixtureThread()}, {"turns", nlohmann::json::array()}}));
        } else if (method == "exit") {
            return 17;
        } else {
            WriteResponse(Success(id, nlohmann::json::object()));
        }
    }
    return 0;
}

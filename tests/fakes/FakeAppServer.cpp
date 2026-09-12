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
int main(int argc, char** argv) {
    bool exit_after_initialize = false;
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--exit-after-initialize") {
            exit_after_initialize = true;
        }
    }

    bool initialized = false;
    nlohmann::json current_thread = FixtureThread();
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
            initialized = true;
            if (exit_after_initialize) {
                return 17;
            }
        } else if (!initialized) {
            WriteResponse({{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", -32001}, {"message", "not initialized"}}}});
        } else if (method == "thread/list") {
            WriteResponse(Success(id, {{"data", nlohmann::json::array({current_thread})}, {"nextCursor", nullptr}}));
        } else if (method == "thread/read") {
            nlohmann::json thread = current_thread;
            thread["id"] = request.at("params").value("threadId", "thr_123");
            WriteResponse(Success(id, {{"thread", thread}, {"turns", nlohmann::json::array()}}));
        } else if (method == "thread/resume") {
            nlohmann::json thread = current_thread;
            thread["id"] = request.at("params").value("threadId", "thr_123");
            WriteResponse(Success(id, {{"thread", thread}, {"turns", nlohmann::json::array()}}));
        } else if (method == "thread/start") {
            current_thread = FixtureThread();
            current_thread["id"] = "thr_new";
            current_thread["name"] = "New thread";
            current_thread["cwd"] = request.at("params").value("cwd", "");
            WriteResponse(Success(id, {{"thread", current_thread}}));
        } else if (method == "thread/name/set") {
            current_thread["name"] = request.at("params").value("name", "");
            WriteResponse(Success(id, nlohmann::json::object()));
        } else if (method == "thread/archive") {
            current_thread["archived"] = true;
            WriteResponse(Success(id, nlohmann::json::object()));
        } else if (method == "turn/start") {
            const std::string thread_id = request.at("params").value("threadId", "thr_new");
            WriteResponse(Success(id, {{"turnId", "turn_1"}}));
            WriteResponse({{"jsonrpc", "2.0"}, {"method", "turn/started"}, {"params", {{"threadId", thread_id}, {"turnId", "turn_1"}}}});
            WriteResponse({{"jsonrpc", "2.0"}, {"method", "turn/completed"}, {"params", {{"threadId", thread_id}, {"turnId", "turn_1"}}}});
        } else if (method == "exit") {
            return 17;
        } else {
            WriteResponse(Success(id, nlohmann::json::object()));
        }
    }
    return 0;
}

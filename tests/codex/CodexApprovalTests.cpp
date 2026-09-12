// ============================================================================
// Codex Deck - Tests de routage des approvals Codex
// ----------------------------------------------------------------------------
// Ce fichier valide que les requetes serveur d'approbation sont stockees par id
// JSON-RPC et resolues avec une reponse au meme id.
// ============================================================================

#include "codex/CodexEventRouter.h"
#include "codex/JsonRpcTransport.h"
#include "model/SessionRuntimeRegistry.h"

#include <windows.h>

#include <condition_variable>
#include <mutex>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Ferme un handle Win32 valide.
//
// Parametres :
// - handle : handle a fermer.
// ----------------------------------------------------------------------------
void CloseIfValid(HANDLE handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
}

// ----------------------------------------------------------------------------
// Lit une ligne UTF-8 depuis un pipe.
//
// Parametres :
// - handle : pipe source.
//
// Retour :
// - ligne sans saut final.
// ----------------------------------------------------------------------------
std::string ReadLine(HANDLE handle) {
    std::string line;
    char ch = '\0';
    DWORD read = 0;
    while (ReadFile(handle, &ch, 1, &read, nullptr) && read == 1) {
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
    return line;
}

// ----------------------------------------------------------------------------
// Ecrit une ligne JSON vers un pipe.
//
// Parametres :
// - handle : pipe cible.
// - payload : objet a serialiser.
// ----------------------------------------------------------------------------
void WriteJsonLine(HANDLE handle, const nlohmann::json& payload) {
    const std::string line = payload.dump() + "\n";
    DWORD written = 0;
    WriteFile(handle, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
}

// ----------------------------------------------------------------------------
// Cree les pipes de transport pour un serveur local.
//
// Parametres :
// - client_read : pipe lu par le transport.
// - client_write : pipe ecrit par le transport.
// - server_read : pipe lu par le serveur.
// - server_write : pipe ecrit par le serveur.
//
// Retour :
// - true si les pipes sont crees.
// ----------------------------------------------------------------------------
bool CreateTransportPipes(HANDLE* client_read, HANDLE* client_write, HANDLE* server_read, HANDLE* server_write) {
    return CreatePipe(client_read, server_write, nullptr, 0)
        && CreatePipe(server_read, client_write, nullptr, 0);
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie le routage des approvals commande et changement de fichier.
//
// Retour :
// - zero si les approvals sont correlees par id.
// ----------------------------------------------------------------------------
int main() {
    HANDLE client_read = nullptr;
    HANDLE client_write = nullptr;
    HANDLE server_read = nullptr;
    HANDLE server_write = nullptr;
    if (!CreateTransportPipes(&client_read, &client_write, &server_read, &server_write)) {
        return 1;
    }

    JsonRpcTransport transport;
    SessionRuntimeRegistry registry;
    CodexEventRouter router(transport, registry);
    router.AttachTransportHandlers();

    if (!transport.Start(client_read, client_write)) {
        return 2;
    }

    WriteJsonLine(server_write, {
        {"jsonrpc", "2.0"},
        {"id", 41},
        {"method", "item/commandExecution/requestApproval"},
        {"params", {{"threadId", "thr_123"}, {"turnId", "turn_1"}, {"title", "Run tests"}}},
    });
    Sleep(100);

    const SessionRuntime* runtime = registry.Find("thr_123");
    if (runtime == nullptr || !runtime->pending_approval || runtime->pending_approval->request_id != 41) {
        return 3;
    }

    if (!router.ResolveApproval(41, "accept")) {
        return 4;
    }
    const nlohmann::json response = nlohmann::json::parse(ReadLine(server_read));
    if (response.at("id") != 41 || response.at("result").at("decision") != "accept") {
        return 5;
    }

    WriteJsonLine(server_write, {
        {"jsonrpc", "2.0"},
        {"id", "file-approval"},
        {"method", "item/fileChange/requestApproval"},
        {"params", {
            {"threadId", "thr_456"},
            {"title", "Apply patch"},
            {"availableDecisions", nlohmann::json::array({"accept", "decline", "view"})},
        }},
    });
    Sleep(100);

    runtime = registry.Find("thr_456");
    if (runtime == nullptr || !runtime->pending_approval || runtime->pending_approval->request_id != "file-approval") {
        return 6;
    }
    if (runtime->pending_approval->available_decisions.size() != 3) {
        return 7;
    }
    if (!router.ResolveApproval("file-approval", "view")) {
        return 8;
    }
    const nlohmann::json second_response = nlohmann::json::parse(ReadLine(server_read));
    if (second_response.at("id") != "file-approval" || second_response.at("result").at("decision") != "view") {
        return 9;
    }

    CloseIfValid(server_write);
    transport.Stop();
    CloseIfValid(server_read);
    return 0;
}

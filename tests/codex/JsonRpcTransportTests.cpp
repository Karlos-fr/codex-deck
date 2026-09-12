// ============================================================================
// Codex Deck - Tests du transport JSON-RPC
// ----------------------------------------------------------------------------
// Ce fichier valide l'entrelacement des reponses, notifications et requetes
// sans lancer de processus externe.
// ============================================================================

#include "codex/JsonRpcTransport.h"

#include <windows.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
// Ecrit une ligne UTF-8 vers un pipe.
//
// Parametres :
// - handle : pipe cible.
// - json : payload a serialiser.
// ----------------------------------------------------------------------------
void WriteJsonLine(HANDLE handle, const nlohmann::json& json) {
    const std::string line = json.dump() + "\n";
    DWORD written = 0;
    WriteFile(handle, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
}

// ----------------------------------------------------------------------------
// Cree deux pipes connectant le transport et le faux serveur local.
//
// Parametres :
// - client_read : stdout lu par le transport.
// - client_write : stdin ecrit par le transport.
// - server_read : stdin lu par le faux serveur.
// - server_write : stdout ecrit par le faux serveur.
//
// Retour :
// - true si les quatre handles sont prets.
// ----------------------------------------------------------------------------
bool CreateTransportPipes(HANDLE* client_read, HANDLE* client_write, HANDLE* server_read, HANDLE* server_write) {
    return CreatePipe(client_read, server_write, nullptr, 0)
        && CreatePipe(server_read, client_write, nullptr, 0);
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie que le transport route les reponses par identifiant.
//
// Retour :
// - zero si les completions et la notification sont correctement livrees.
// ----------------------------------------------------------------------------
int main() {
    HANDLE client_read = nullptr;
    HANDLE client_write = nullptr;
    HANDLE server_read = nullptr;
    HANDLE server_write = nullptr;
    if (!CreateTransportPipes(&client_read, &client_write, &server_read, &server_write)) {
        return 1;
    }

    std::mutex mutex;
    std::condition_variable condition;
    std::vector<int> completions;
    bool notification_seen = false;

    JsonRpcTransport transport;
    transport.SetNotificationHandler([&](CodexNotification notification) {
        std::lock_guard lock(mutex);
        if (notification.method == "turn/started") {
            notification_seen = true;
        }
        condition.notify_all();
    });
    if (!transport.Start(client_read, client_write)) {
        CloseIfValid(server_read);
        CloseIfValid(server_write);
        return 2;
    }

    std::jthread server([&]() {
        const nlohmann::json first = nlohmann::json::parse(ReadLine(server_read));
        const nlohmann::json second = nlohmann::json::parse(ReadLine(server_read));
        WriteJsonLine(server_write, {{"jsonrpc", "2.0"}, {"id", second.at("id")}, {"result", {{"value", 2}}}});
        WriteJsonLine(server_write, {{"jsonrpc", "2.0"}, {"method", "turn/started"}, {"params", {{"threadId", "thr_123"}}}});
        WriteJsonLine(server_write, {{"jsonrpc", "2.0"}, {"id", first.at("id")}, {"result", {{"value", 1}}}});
    });

    transport.Request("thread/list", nlohmann::json::object(), [&](std::expected<nlohmann::json, CodexError> result) {
        std::lock_guard lock(mutex);
        if (result) {
            completions.push_back(result->at("value").get<int>());
        }
        condition.notify_all();
    });
    transport.Request("thread/read", nlohmann::json::object(), [&](std::expected<nlohmann::json, CodexError> result) {
        std::lock_guard lock(mutex);
        if (result) {
            completions.push_back(result->at("value").get<int>());
        }
        condition.notify_all();
    });

    {
        std::unique_lock lock(mutex);
        condition.wait_for(lock, std::chrono::seconds(3), [&]() {
            return completions.size() == 2 && notification_seen;
        });
    }

    CloseIfValid(server_write);
    server_write = nullptr;
    transport.Stop();
    CloseIfValid(server_read);

    if (!notification_seen) {
        return 3;
    }
    if (completions.size() != 2) {
        return 4;
    }
    if (completions[0] != 2 || completions[1] != 1) {
        return 5;
    }
    return 0;
}

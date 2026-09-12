// ============================================================================
// Codex Deck - Implementation du transport JSON-RPC
// ----------------------------------------------------------------------------
// Ce fichier lit et ecrit des enveloppes JSON-RPC ligne par ligne sur des pipes
// Win32, avec correlation des reponses par identifiant.
// ============================================================================

#include "JsonRpcTransport.h"

#include <utility>
#include <vector>

namespace {

// ----------------------------------------------------------------------------
// Ferme un handle Win32 valide.
//
// Parametres :
// - handle : handle a fermer puis neutraliser.
// ----------------------------------------------------------------------------
void CloseIfValid(HANDLE& handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
    handle = nullptr;
}

// ----------------------------------------------------------------------------
// Construit une erreur de pipe avec code systeme.
//
// Parametres :
// - code : famille d'erreur.
// - message : diagnostic court.
//
// Retour :
// - erreur Codex.
// ----------------------------------------------------------------------------
CodexError PipeError(CodexErrorCode code, const wchar_t* message) {
    return CodexError{code, message, static_cast<int>(GetLastError())};
}

// ----------------------------------------------------------------------------
// Construit une erreur de deconnexion locale.
//
// Parametres :
// - message : diagnostic court.
//
// Retour :
// - erreur Disconnected.
// ----------------------------------------------------------------------------
CodexError DisconnectedError(const wchar_t* message) {
    return CodexError{CodexErrorCode::Disconnected, message};
}

// ----------------------------------------------------------------------------
// Convertit l'id JSON-RPC brut en id numerique local.
//
// Parametres :
// - id : valeur JSON-RPC.
//
// Retour :
// - identifiant numerique ou zero si invalide.
// ----------------------------------------------------------------------------
RpcRequestId NumericId(const nlohmann::json& id) {
    return id.is_number_unsigned() ? id.get<RpcRequestId>() : 0;
}

}  // namespace

// ----------------------------------------------------------------------------
// Arrete le transport et libere les handles possedes.
// ----------------------------------------------------------------------------
JsonRpcTransport::~JsonRpcTransport() {
    Stop();
}

// ----------------------------------------------------------------------------
// Installe le handler de notifications.
// ----------------------------------------------------------------------------
void JsonRpcTransport::SetNotificationHandler(NotificationHandler handler) {
    std::lock_guard lock(handler_mutex_);
    notification_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Installe le handler de requetes serveur.
// ----------------------------------------------------------------------------
void JsonRpcTransport::SetServerRequestHandler(ServerRequestHandler handler) {
    std::lock_guard lock(handler_mutex_);
    server_request_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Installe le handler de deconnexion.
// ----------------------------------------------------------------------------
void JsonRpcTransport::SetDisconnectHandler(DisconnectHandler handler) {
    std::lock_guard lock(handler_mutex_);
    disconnect_handler_ = std::move(handler);
}

// ----------------------------------------------------------------------------
// Demarre la lecture asynchrone sur les pipes fournis.
// ----------------------------------------------------------------------------
std::expected<void, CodexError> JsonRpcTransport::Start(HANDLE stdout_read, HANDLE stdin_write) {
    Stop();
    if (stdout_read == nullptr || stdout_read == INVALID_HANDLE_VALUE
        || stdin_write == nullptr || stdin_write == INVALID_HANDLE_VALUE) {
        return std::unexpected(DisconnectedError(L"Handles JSON-RPC invalides"));
    }

    stdout_read_ = stdout_read;
    stdin_write_ = stdin_write;
    stopping_ = false;
    reader_ = std::jthread([this]() {
        ReadLoop();
    });
    return {};
}

// ----------------------------------------------------------------------------
// Envoie une requete JSON-RPC.
// ----------------------------------------------------------------------------
RpcRequestId JsonRpcTransport::Request(std::string method, nlohmann::json params, RpcCompletion completion) {
    const RpcRequestId id = next_id_.fetch_add(1);
    {
        std::lock_guard lock(pending_mutex_);
        pending_.emplace(id, std::move(completion));
    }

    const nlohmann::json payload = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", std::move(method)},
        {"params", std::move(params)},
    };
    if (const auto written = WriteJsonLine(payload); !written) {
        RpcCompletion failed;
        {
            std::lock_guard lock(pending_mutex_);
            const auto found = pending_.find(id);
            if (found != pending_.end()) {
                failed = std::move(found->second);
                pending_.erase(found);
            }
        }
        if (failed) {
            failed(std::unexpected(written.error()));
        }
    }
    return id;
}

// ----------------------------------------------------------------------------
// Repond a une requete serveur.
// ----------------------------------------------------------------------------
std::expected<void, CodexError> JsonRpcTransport::SendResponse(const nlohmann::json& id, nlohmann::json result) {
    return WriteJsonLine({{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}});
}

// ----------------------------------------------------------------------------
// Arrete le transport et complete les requetes pendantes.
// ----------------------------------------------------------------------------
void JsonRpcTransport::Stop() {
    const bool was_stopping = stopping_.exchange(true);
    CloseIfValid(stdout_read_);
    CloseIfValid(stdin_write_);
    if (reader_.joinable()) {
        reader_.request_stop();
        reader_.join();
    }
    if (!was_stopping) {
        Disconnect(DisconnectedError(L"Transport JSON-RPC arrete"));
    }
}

// ----------------------------------------------------------------------------
// Boucle de lecture executee sur worker.
// ----------------------------------------------------------------------------
void JsonRpcTransport::ReadLoop() {
    std::string line;
    char ch = '\0';
    DWORD read = 0;
    while (!stopping_) {
        const BOOL ok = ReadFile(stdout_read_, &ch, 1, &read, nullptr);
        if (!ok || read == 0) {
            break;
        }
        if (ch == '\n') {
            ProcessLine(line);
            line.clear();
        } else {
            line.push_back(ch);
        }
    }
    if (!stopping_) {
        Disconnect(PipeError(CodexErrorCode::PipeReadFailed, L"Lecture JSON-RPC interrompue"));
    }
}

// ----------------------------------------------------------------------------
// Traite une ligne JSON-RPC recue.
// ----------------------------------------------------------------------------
void JsonRpcTransport::ProcessLine(const std::string& line) {
    nlohmann::json payload = nlohmann::json::parse(line, nullptr, false);
    if (payload.is_discarded()) {
        Disconnect(CodexError{CodexErrorCode::InvalidJson, L"Ligne JSON-RPC invalide"});
        return;
    }

    const auto parsed = ParseServerMessage(payload);
    if (!parsed) {
        Disconnect(parsed.error());
        return;
    }

    if (const auto* response = std::get_if<nlohmann::json>(&*parsed)) {
        const RpcRequestId id = NumericId(response->at("id"));
        RpcCompletion completion;
        {
            std::lock_guard lock(pending_mutex_);
            const auto found = pending_.find(id);
            if (found != pending_.end()) {
                completion = std::move(found->second);
                pending_.erase(found);
            }
        }
        if (completion) {
            if (response->contains("error")) {
                int rpc_code = 0;
                if (response->at("error").is_object() && response->at("error").contains("code")) {
                    rpc_code = response->at("error").at("code").get<int>();
                }
                completion(std::unexpected(CodexError{CodexErrorCode::RpcError, L"Erreur JSON-RPC distante", 0, rpc_code}));
            } else {
                completion(response->value("result", nlohmann::json::object()));
            }
        }
        return;
    }

    std::lock_guard lock(handler_mutex_);
    if (const auto* notification = std::get_if<CodexNotification>(&*parsed)) {
        if (notification_handler_) {
            notification_handler_(CodexNotification{notification->method, notification->params});
        }
    } else if (const auto* request = std::get_if<CodexServerRequest>(&*parsed)) {
        if (server_request_handler_) {
            server_request_handler_(CodexServerRequest{request->id, request->method, request->params});
        }
    }
}

// ----------------------------------------------------------------------------
// Ecrit une enveloppe JSON-RPC complete.
// ----------------------------------------------------------------------------
std::expected<void, CodexError> JsonRpcTransport::WriteJsonLine(const nlohmann::json& payload) {
    std::lock_guard lock(write_mutex_);
    if (stdin_write_ == nullptr || stdin_write_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(DisconnectedError(L"Pipe stdin JSON-RPC indisponible"));
    }

    const std::string line = payload.dump() + "\n";
    std::size_t offset = 0;
    while (offset < line.size()) {
        DWORD written = 0;
        const DWORD remaining = static_cast<DWORD>(line.size() - offset);
        if (!WriteFile(stdin_write_, line.data() + offset, remaining, &written, nullptr)) {
            return std::unexpected(PipeError(CodexErrorCode::PipeWriteFailed, L"Ecriture JSON-RPC impossible"));
        }
        if (written == 0) {
            return std::unexpected(PipeError(CodexErrorCode::PipeWriteFailed, L"Ecriture JSON-RPC incomplete"));
        }
        offset += written;
    }
    return {};
}

// ----------------------------------------------------------------------------
// Signale une deconnexion et vide les completions pendantes.
// ----------------------------------------------------------------------------
void JsonRpcTransport::Disconnect(CodexError error) {
    std::vector<RpcCompletion> completions;
    {
        std::lock_guard lock(pending_mutex_);
        for (auto& [id, completion] : pending_) {
            (void)id;
            completions.push_back(std::move(completion));
        }
        pending_.clear();
    }

    for (auto& completion : completions) {
        if (completion) {
            completion(std::unexpected(error));
        }
    }

    std::lock_guard lock(handler_mutex_);
    if (!stopping_ && disconnect_handler_) {
        disconnect_handler_(std::move(error));
    }
}

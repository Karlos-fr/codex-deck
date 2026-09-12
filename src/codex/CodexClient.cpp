// ============================================================================
// Codex Deck - Implementation de la facade app-server
// ----------------------------------------------------------------------------
// Ce fichier construit les appels JSON-RPC V2 minimaux et parse leurs resultats
// en types internes, sans bloquer le thread appelant.
// ============================================================================

#include "CodexClient.h"

#include <memory>
#include <utility>

namespace {

// Limite de page par defaut pour thread/list.
constexpr int kThreadListLimit = 100;

// Nom de client transmis pendant initialize.
constexpr char kClientName[] = "codex-deck";

// Titre humain transmis pendant initialize.
constexpr char kClientTitle[] = "Codex Deck";

// Version initiale transmise pendant initialize.
constexpr char kClientVersion[] = "0.1.0";

// ----------------------------------------------------------------------------
// Construit une erreur de reponse invalide.
//
// Parametres :
// - message : diagnostic court.
//
// Retour :
// - erreur Codex.
// ----------------------------------------------------------------------------
CodexError InvalidResponse(const wchar_t* message) {
    return CodexError{CodexErrorCode::InvalidResponse, message};
}

// ----------------------------------------------------------------------------
// Parse les tours d'un detail de thread.
//
// Parametres :
// - payload : tableau JSON optionnel.
//
// Retour :
// - liste de snapshots de tours.
// ----------------------------------------------------------------------------
std::vector<CodexTurnSnapshot> ParseTurns(const nlohmann::json& payload) {
    std::vector<CodexTurnSnapshot> turns;
    if (!payload.is_array()) {
        return turns;
    }
    for (const nlohmann::json& item : payload) {
        if (!item.is_object()) {
            continue;
        }
        CodexTurnSnapshot turn{};
        if (item.contains("id") && item.at("id").is_string()) {
            turn.id = item.at("id").get<std::string>();
        }
        if (item.contains("status") && item.at("status").is_string()) {
            turn.status = item.at("status").get<std::string>();
        }
        if (item.contains("items") && item.at("items").is_array()) {
            for (const nlohmann::json& timeline_item : item.at("items")) {
                turn.items.push_back(timeline_item);
            }
        }
        turns.push_back(std::move(turn));
    }
    return turns;
}

// ----------------------------------------------------------------------------
// Parse un detail de thread depuis une reponse app-server.
//
// Parametres :
// - payload : resultat JSON-RPC.
//
// Retour :
// - detail de thread ou erreur.
// ----------------------------------------------------------------------------
std::expected<CodexThreadDetail, CodexError> ParseThreadDetail(const nlohmann::json& payload) {
    if (!payload.is_object() || !payload.contains("thread")) {
        return std::unexpected(InvalidResponse(L"La reponse de thread ne contient pas de champ thread"));
    }
    auto summary = ParseThreadSummary(payload.at("thread"));
    if (!summary) {
        return std::unexpected(summary.error());
    }
    return CodexThreadDetail{*summary, ParseTurns(payload.value("turns", nlohmann::json::array()))};
}

// ----------------------------------------------------------------------------
// Parse un resume depuis une reponse contenant thread.
//
// Parametres :
// - payload : resultat JSON-RPC.
//
// Retour :
// - resume de thread ou erreur.
// ----------------------------------------------------------------------------
std::expected<CodexThreadSummary, CodexError> ParseThreadResult(const nlohmann::json& payload) {
    if (!payload.is_object() || !payload.contains("thread")) {
        return std::unexpected(InvalidResponse(L"La reponse ne contient pas de resume de thread"));
    }
    return ParseThreadSummary(payload.at("thread"));
}

// ----------------------------------------------------------------------------
// Complete une operation void depuis une reponse JSON.
//
// Parametres :
// - result : resultat RPC brut.
// - completion : callback a appeler.
// ----------------------------------------------------------------------------
void CompleteVoid(std::expected<nlohmann::json, CodexError> result, VoidCompletion completion) {
    if (!result) {
        completion(std::unexpected(result.error()));
        return;
    }
    completion({});
}

// ----------------------------------------------------------------------------
// Ajoute les options communes a thread/list.
//
// Parametres :
// - options : options internes.
// - cursor : curseur optionnel de pagination.
//
// Retour :
// - objet params pour thread/list.
// ----------------------------------------------------------------------------
nlohmann::json ThreadListParams(const ThreadListOptions& options, const std::optional<std::string>& cursor) {
    nlohmann::json params = {
        {"limit", kThreadListLimit},
        {"archived", options.archived},
    };
    if (options.cwd) {
        params["cwd"] = options.cwd->string();
    }
    if (cursor) {
        params["cursor"] = *cursor;
    }
    return params;
}

}  // namespace

// ----------------------------------------------------------------------------
// Cree une facade autour d'un transport deja demarre.
// ----------------------------------------------------------------------------
CodexClient::CodexClient(JsonRpcTransport& transport)
    : transport_(transport) {
}

// ----------------------------------------------------------------------------
// Execute le handshake initialize.
// ----------------------------------------------------------------------------
void CodexClient::Connect(VoidCompletion completion) {
    nlohmann::json params = {
        {"clientInfo", {
            {"name", kClientName},
            {"title", kClientTitle},
            {"version", kClientVersion},
        }},
    };
    transport_.Request("initialize", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        CompleteVoid(std::move(result), std::move(completion));
    });
}

// ----------------------------------------------------------------------------
// Liste tous les threads correspondant aux options.
// ----------------------------------------------------------------------------
void CodexClient::ListThreads(ThreadListOptions options, ThreadListCompletion completion) {
    struct ListState {
        ThreadListOptions options;
        ThreadListCompletion completion;
        std::vector<CodexThreadSummary> threads;
    };
    auto state = std::make_shared<ListState>(ListState{std::move(options), std::move(completion), {}});
    auto request_page = std::make_shared<std::move_only_function<void(std::optional<std::string>)>>();
    *request_page = [this, state, request_page](std::optional<std::string> cursor) mutable {
        transport_.Request("thread/list", ThreadListParams(state->options, cursor), [this, state, request_page](std::expected<nlohmann::json, CodexError> result) mutable {
            if (!result) {
                state->completion(std::unexpected(result.error()));
                return;
            }
            if (!result->is_object() || !result->contains("data") || !result->at("data").is_array()) {
                state->completion(std::unexpected(InvalidResponse(L"La liste de threads est invalide")));
                return;
            }
            for (const nlohmann::json& item : result->at("data")) {
                auto parsed = ParseThreadSummary(item);
                if (!parsed) {
                    state->completion(std::unexpected(parsed.error()));
                    return;
                }
                state->threads.push_back(std::move(*parsed));
            }

            if (result->contains("nextCursor") && !result->at("nextCursor").is_null()) {
                (*request_page)(result->at("nextCursor").get<std::string>());
                return;
            }
            state->completion(std::move(state->threads));
        });
    };
    (*request_page)(std::nullopt);
}

// ----------------------------------------------------------------------------
// Lit passivement un thread avec ses tours.
// ----------------------------------------------------------------------------
void CodexClient::ReadThread(CodexThreadId thread_id, ThreadDetailCompletion completion) {
    nlohmann::json params = {{"threadId", std::move(thread_id)}, {"includeTurns", true}};
    transport_.Request("thread/read", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        if (!result) {
            completion(std::unexpected(result.error()));
            return;
        }
        auto detail = ParseThreadDetail(*result);
        if (!detail) {
            completion(std::unexpected(detail.error()));
            return;
        }
        completion(std::move(*detail));
    });
}

// ----------------------------------------------------------------------------
// Reprend interactivement un thread avec ses tours.
// ----------------------------------------------------------------------------
void CodexClient::ResumeThread(CodexThreadId thread_id, ThreadDetailCompletion completion) {
    nlohmann::json params = {{"threadId", std::move(thread_id)}, {"includeTurns", true}};
    transport_.Request("thread/resume", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        if (!result) {
            completion(std::unexpected(result.error()));
            return;
        }
        auto detail = ParseThreadDetail(*result);
        if (!detail) {
            completion(std::unexpected(detail.error()));
            return;
        }
        completion(std::move(*detail));
    });
}

// ----------------------------------------------------------------------------
// Cree un thread Codex.
// ----------------------------------------------------------------------------
void CodexClient::StartThread(StartThreadOptions options, ThreadSummaryCompletion completion) {
    nlohmann::json params = {{"cwd", options.cwd.string()}};
    if (options.model) {
        params["model"] = *options.model;
    }
    transport_.Request("thread/start", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        if (!result) {
            completion(std::unexpected(result.error()));
            return;
        }
        auto summary = ParseThreadResult(*result);
        if (!summary) {
            completion(std::unexpected(summary.error()));
            return;
        }
        completion(std::move(*summary));
    });
}

// ----------------------------------------------------------------------------
// Renomme un thread cote Codex.
// ----------------------------------------------------------------------------
void CodexClient::SetThreadName(CodexThreadId thread_id, std::string name, VoidCompletion completion) {
    nlohmann::json params = {{"threadId", std::move(thread_id)}, {"name", std::move(name)}};
    transport_.Request("thread/name/set", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        CompleteVoid(std::move(result), std::move(completion));
    });
}

// ----------------------------------------------------------------------------
// Archive un thread cote Codex.
// ----------------------------------------------------------------------------
void CodexClient::ArchiveThread(CodexThreadId thread_id, VoidCompletion completion) {
    nlohmann::json params = {{"threadId", std::move(thread_id)}};
    transport_.Request("thread/archive", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        CompleteVoid(std::move(result), std::move(completion));
    });
}

// ----------------------------------------------------------------------------
// Lance un tour dans un thread.
// ----------------------------------------------------------------------------
void CodexClient::StartTurn(StartTurnOptions options, TurnCompletion completion) {
    nlohmann::json params = {
        {"threadId", std::move(options.thread_id)},
        {"input", nlohmann::json::array({
            {
                {"type", "text"},
                {"text", std::move(options.prompt)},
                {"textElements", nlohmann::json::array()},
            },
        })},
    };
    transport_.Request("turn/start", std::move(params), [completion = std::move(completion)](std::expected<nlohmann::json, CodexError> result) mutable {
        if (!result) {
            completion(std::unexpected(result.error()));
            return;
        }
        if (!result->is_object() || !result->contains("turnId") || !result->at("turnId").is_string()) {
            completion(std::unexpected(InvalidResponse(L"La reponse turn/start ne contient pas de turnId")));
            return;
        }
        completion(result->at("turnId").get<std::string>());
    });
}

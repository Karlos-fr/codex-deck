# Codex Deck App-Server Client Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fournir un moteur C++23 robuste qui lance un unique `codex app-server`, dialogue en JSON-RPC/JSONL, expose les opérations de session nécessaires à la V1 et résiste aux crashs/reconnexions.

**Architecture:** La couche est séparée en résolution/lancement du processus, transport JSON-RPC générique, façade Codex typée et registre runtime multi-session. Les tests utilisent un faux `app-server` compilé avec le projet afin de couvrir stdin/stdout, notifications, server requests, crash et reconnexion sans consommer Codex.

**Tech Stack:** C++23, Win32 process/pipes/job objects, nlohmann/json, `std::expected`, `std::jthread`, CMake, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- Un seul `codex app-server` pour toutes les sessions.
- Transport stdio en messages JSON-RPC sérialisés sur une ligne JSON suivie de `\n`.
- Aucun `ReadFile`, `WriteFile`, parsing JSON ou attente RPC sur le thread UI.
- Parsing tolérant : ignorer les champs inconnus ; échouer seulement si un champ nécessaire au contrat interne est absent/invalide.
- Les server requests d’approbation sont corrélées par leur `id` JSON-RPC, pas seulement par `threadId`.
- Ne jamais modifier directement `~/.codex`, `session_index.jsonl` ou la base interne Codex.
- Les méthodes V2 utilisées par la V1 incluent au minimum `thread/list`, `thread/read`, `thread/start`, `thread/resume`, `thread/name/set`, `thread/archive` et `turn/start`.

---

### Task 1: Définir les types internes et le parsing tolérant du protocole

**Files:**
- Create: `src/codex/CodexError.h`
- Create: `src/codex/CodexTypes.h`
- Create: `src/codex/CodexProtocol.h`
- Create: `src/codex/CodexProtocol.cpp`
- Test: `tests/codex/CodexProtocolTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `using CodexThreadId = std::string;`
- Produces: `ParseThreadSummary(const nlohmann::json&) -> std::expected<CodexThreadSummary, CodexError>`.
- Produces: `ParseServerMessage(const nlohmann::json&) -> std::expected<CodexInboundMessage, CodexError>`.

- [x] **Step 1: Écrire les fixtures de protocole minimales**

Dans `CodexProtocolTests.cpp`, tester ce thread avec des champs inconnus supplémentaires :

```cpp
const nlohmann::json payload = {
    {"id", "thr_123"},
    {"name", "Audio parity"},
    {"cwd", "D:\\VibeCoding\\spotifyamp"},
    {"createdAt", 1'757'000'000},
    {"updatedAt", 1'757'000'120},
    {"status", "idle"},
    {"futureField", { {"ignored", true} }}
};

auto parsed = ParseThreadSummary(payload);
if (!parsed) return 1;
if (parsed->id != "thr_123") return 2;
if (parsed->name != "Audio parity") return 3;
```

Ajouter des tests séparés pour une réponse JSON-RPC, une notification et une server request `item/commandExecution/requestApproval`.

- [x] **Step 2: Vérifier l’échec**

```powershell
cmake --build --preset debug
ctest --preset debug -R CodexProtocolTests --output-on-failure
```

Expected: FAIL car les types n’existent pas.

- [x] **Step 3: Définir les erreurs**

`CodexError.h` :

```cpp
#pragma once
#include <string>

enum class CodexErrorCode {
    ExecutableNotFound,
    ProcessLaunchFailed,
    ProcessExited,
    PipeReadFailed,
    PipeWriteFailed,
    InvalidJson,
    InvalidResponse,
    RpcError,
    RequestTimeout,
    Disconnected,
};

struct CodexError {
    CodexErrorCode code;
    std::wstring message;
    int system_code = 0;
    int rpc_code = 0;
};
```

- [x] **Step 4: Définir les modèles stables**

`CodexTypes.h` :

```cpp
using CodexThreadId = std::string;
using CodexTurnId = std::string;

struct CodexThreadSummary {
    CodexThreadId id;
    std::string name;
    std::filesystem::path cwd;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
    bool archived = false;
};

enum class SessionStatus { Idle, Working, NeedsAttention, Completed, Error };

struct CodexNotification {
    std::string method;
    nlohmann::json params;
};

struct CodexServerRequest {
    nlohmann::json id;
    std::string method;
    nlohmann::json params;
};

using CodexInboundMessage = std::variant<CodexNotification, CodexServerRequest, nlohmann::json>;
```

La variante `nlohmann::json` finale représente une réponse RPC déjà validée au niveau enveloppe et routée ensuite par ID.

- [x] **Step 5: Implémenter le parsing d’enveloppe**

Règles :

```text
objet avec method + id     -> CodexServerRequest
objet avec method sans id  -> CodexNotification
objet avec id + result/error -> réponse RPC brute
sinon                      -> InvalidResponse
```

`ParseThreadSummary` exige uniquement `id`; `name`, `cwd`, timestamps et archive disposent de valeurs par défaut sûres.

- [x] **Step 6: Valider**

```powershell
cmake --build --preset debug
ctest --preset debug -R CodexProtocolTests --output-on-failure
```

- [x] **Step 7: Commit**

```bash
git add src/codex tests/codex CMakeLists.txt
git commit -m "feat: define Codex app-server protocol types"
```

---

### Task 2: Résoudre et lancer `codex app-server` avec pipes et Job Object

**Files:**
- Create: `src/codex/CodexExecutableResolver.h`
- Create: `src/codex/CodexExecutableResolver.cpp`
- Create: `src/codex/CodexProcess.h`
- Create: `src/codex/CodexProcess.cpp`
- Test: `tests/codex/CodexExecutableResolverTests.cpp`
- Test helper: `tests/fakes/FakeAppServer.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `ResolveCodexExecutable(optional<path> override) -> expected<CodexLaunchSpec, CodexError>`.
- Produces: `CodexProcess::Start(const CodexLaunchSpec&) -> expected<void, CodexError>`.
- Produces: `TakeStdoutReadHandle()`, `StdinWriteHandle()`, `IsRunning()`, `Stop()`.

- [x] **Step 1: Écrire les tests de priorité de résolution**

Le resolver applique exactement cet ordre :

```text
1. chemin explicite passé par Codex Deck
2. variable CODEX_DECK_CODEX_PATH
3. SearchPathW("codex.exe")
4. SearchPathW("codex.cmd")
5. erreur ExecutableNotFound
```

Pour les tests, injecter un callback de recherche :

```cpp
using PathLookup = std::function<std::optional<std::filesystem::path>(std::wstring_view)>;
std::expected<CodexLaunchSpec, CodexError> ResolveCodexExecutable(
    const std::optional<std::filesystem::path>& override_path,
    const PathLookup& lookup
);
```

- [x] **Step 2: Vérifier l’échec puis implémenter le resolver**

Pour `.exe`, `CodexLaunchSpec` contient l’exécutable directement. Pour `.cmd`, utiliser `%ComSpec%` comme application et construire :

```text
/d /s /c ""C:\...\codex.cmd" app-server"
```

Ne pas lancer un `.ps1` implicitement.

- [x] **Step 3: Créer le faux app-server**

`tests/fakes/FakeAppServer.cpp` lit stdin avec `std::getline`, parse chaque ligne JSON, puis :

```text
initialize   -> répond avec un objet result minimal
thread/list  -> renvoie {data:[thread fixture], nextCursor:null}
thread/read  -> renvoie le thread demandé
exit         -> termine le processus avec code 17
```

Chaque réponse est écrite sur une seule ligne JSON et flushée.

- [x] **Step 4: Implémenter `CodexProcess`**

Utiliser deux pipes anonymes avec handles enfant héritables, `STARTUPINFOEXW` ou `STARTUPINFOW`, `CreateProcessW(..., CREATE_NO_WINDOW, ...)` et un Job Object configuré avec `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`.

Le parent ferme immédiatement les extrémités de pipe qui appartiennent à l’enfant.

- [x] **Step 5: Ajouter un test de cycle de vie process**

Le test lance `FakeAppServer.exe`, vérifie `IsRunning()`, écrit une ligne, lit une réponse, appelle `Stop()` puis vérifie que le process n’est plus actif.

- [x] **Step 6: Valider et commit**

```powershell
cmake --build --preset debug
ctest --preset debug -R "CodexExecutableResolverTests|CodexProcessTests" --output-on-failure
git add src/codex tests CMakeLists.txt
git commit -m "feat: launch Codex app-server as supervised child process"
```

---

### Task 3: Implémenter le transport JSON-RPC asynchrone

**Files:**
- Create: `src/codex/JsonRpcTransport.h`
- Create: `src/codex/JsonRpcTransport.cpp`
- Test: `tests/codex/JsonRpcTransportTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `using RpcRequestId = std::uint64_t;`
- Produces: `using RpcCompletion = std::move_only_function<void(std::expected<nlohmann::json, CodexError>)>;`
- Produces: `Request(method, params, completion) -> RpcRequestId`.
- Produces: `SendResponse(server_request_id, result)`.
- Produces: callbacks `OnNotification`, `OnServerRequest`, `OnDisconnected`.

- [x] **Step 1: Écrire le test d’entrelacement**

Scénario : envoyer deux requests, faire répondre le fake dans l’ordre inverse, injecter une notification entre les deux, puis vérifier que chaque completion reçoit le bon `id` et que la notification est livrée séparément.

- [x] **Step 2: Vérifier l’échec**

```powershell
cmake --build --preset debug
ctest --preset debug -R JsonRpcTransportTests --output-on-failure
```

- [x] **Step 3: Définir le transport**

`JsonRpcTransport` possède :

```cpp
class JsonRpcTransport {
public:
    using NotificationHandler = std::move_only_function<void(CodexNotification)>;
    using ServerRequestHandler = std::move_only_function<void(CodexServerRequest)>;
    using DisconnectHandler = std::move_only_function<void(CodexError)>;

    std::expected<void, CodexError> Start(HANDLE stdout_read, HANDLE stdin_write);
    RpcRequestId Request(std::string method, nlohmann::json params, RpcCompletion completion);
    std::expected<void, CodexError> SendResponse(const nlohmann::json& id, nlohmann::json result);
    void Stop();
};
```

Un `std::jthread` lit les lignes. L’écriture est sérialisée par mutex. La map des requests pendantes est protégée par mutex et vidée avec `Disconnected` à l’arrêt inattendu.

- [x] **Step 4: Construire les enveloppes JSON-RPC**

Request :

```json
{"jsonrpc":"2.0","id":1,"method":"thread/list","params":{}}
```

Réponse à server request :

```json
{"jsonrpc":"2.0","id":12,"result":{"decision":"accept"}}
```

Toujours écrire `dump()` + `"\n"` puis flush du handle via `WriteFile` complet, en gérant les écritures partielles.

- [x] **Step 5: Valider et commit**

```powershell
cmake --build --preset debug
ctest --preset debug -R JsonRpcTransportTests --output-on-failure
git add src/codex/JsonRpcTransport.* tests/codex/JsonRpcTransportTests.cpp CMakeLists.txt
git commit -m "feat: add asynchronous JSON-RPC transport"
```

---

### Task 4: Ajouter la façade Codex typée et le handshake

**Files:**
- Create: `src/codex/CodexClient.h`
- Create: `src/codex/CodexClient.cpp`
- Test: `tests/codex/CodexClientTests.cpp`

**Interfaces:**
- Produces: `CodexClient::Connect()`.
- Produces: `ListThreads`, `ReadThread`, `StartThread`, `ResumeThread`, `SetThreadName`, `ArchiveThread`, `StartTurn`.
- Consumes: `JsonRpcTransport`.

- [x] **Step 1: Étendre FakeAppServer avec les méthodes V2**

Réponses déterministes :

```text
thread/start    -> thread id thr_new
thread/resume   -> thread demandé
thread/name/set -> {}
thread/archive  -> {}
turn/start      -> turn id turn_1 puis notifications turn/started et turn/completed
```

- [x] **Step 2: Écrire un test de handshake + liste**

`Connect()` doit d’abord envoyer `initialize` avec :

```json
{
  "clientInfo": {
    "name": "codex-deck",
    "title": "Codex Deck",
    "version": "0.1.0"
  }
}
```

Le test échoue si `thread/list` est envoyé avant la réussite de `initialize`.

- [x] **Step 3: Définir les options et completions typées**

```cpp
struct StartThreadOptions {
    std::filesystem::path cwd;
    std::optional<std::string> model;
};

struct StartTurnOptions {
    CodexThreadId thread_id;
    std::string prompt;
};

using ThreadCompletion = std::move_only_function<void(std::expected<CodexThreadSummary, CodexError>)>;
using ThreadListCompletion = std::move_only_function<void(std::expected<std::vector<CodexThreadSummary>, CodexError>)>;
using VoidCompletion = std::move_only_function<void(std::expected<void, CodexError>)>;
```

- [x] **Step 4: Implémenter les wire methods**

Méthodes minimales et paramètres :

```text
thread/list      params {limit:100} + pagination nextCursor
thread/read      params {threadId, includeTurns:true}
thread/start     params {cwd, model seulement si override}
thread/resume    params {threadId, includeTurns:true}
thread/name/set  params {threadId, name}
thread/archive   params {threadId}
turn/start       params {threadId, input:[{type:"text", text:prompt}]}
```

Pour `thread/list`, poursuivre automatiquement les pages jusqu’à `nextCursor == null` afin d’exposer une liste complète au service de synchronisation.

- [x] **Step 5: Valider les opérations typées**

Tester création → renommage → read → turn/start → archive contre FakeAppServer.

- [x] **Step 6: Commit**

```bash
git add src/codex/CodexClient.* tests/codex/CodexClientTests.cpp tests/fakes/FakeAppServer.cpp
git commit -m "feat: add typed Codex app-server client"
```

---

### Task 5: Router notifications, runtime multi-session et approbations

**Files:**
- Create: `src/model/SessionRuntime.h`
- Create: `src/model/SessionRuntimeRegistry.h`
- Create: `src/model/SessionRuntimeRegistry.cpp`
- Create: `src/codex/CodexEventRouter.h`
- Create: `src/codex/CodexEventRouter.cpp`
- Test: `tests/model/SessionRuntimeTests.cpp`
- Test: `tests/codex/CodexApprovalTests.cpp`

**Interfaces:**
- Produces: `SessionRuntimeRegistry::ApplyNotification(const CodexNotification&)`.
- Produces: `PendingApproval` et `ResolveApproval(request_id, wire_decision)`.

- [ ] **Step 1: Définir le runtime**

```cpp
struct PendingApproval {
    nlohmann::json request_id;
    CodexThreadId thread_id;
    std::optional<CodexTurnId> turn_id;
    std::string method;
    std::string title;
    std::string detail;
    std::vector<std::string> available_decisions;
};

struct SessionRuntime {
    CodexThreadId thread_id;
    SessionStatus status = SessionStatus::Idle;
    std::optional<CodexTurnId> current_turn;
    std::optional<PendingApproval> pending_approval;
    std::int64_t latest_activity = 0;
    std::string last_error;
};
```

- [ ] **Step 2: Écrire les transitions de test**

Vérifier :

```text
turn/started   -> Working
approval req   -> NeedsAttention + pending approval
approval reply -> Working
turn/completed -> Completed
turn/error     -> Error
```

- [ ] **Step 3: Parser les deux approvals V1**

Supporter :

```text
item/commandExecution/requestApproval
item/fileChange/requestApproval
```

Si `availableDecisions` est présent, conserver exactement les valeurs annoncées. Sinon exposer `accept` et `decline`. La réponse renvoie `{"decision": wire_decision}` au même `id` JSON-RPC.

- [ ] **Step 4: Conserver les notifications brutes utiles au futur Workbench**

`CodexEventRouter` publie un `CodexTimelineEvent` générique contenant `thread_id`, `method`, `params`, `received_at`; le Workbench spécialisé sera construit dans un plan ultérieur.

- [ ] **Step 5: Valider et commit**

```powershell
cmake --build --preset debug
ctest --preset debug -R "SessionRuntimeTests|CodexApprovalTests" --output-on-failure
git add src/model src/codex tests/model tests/codex
git commit -m "feat: track multi-session Codex runtime state"
```

---

### Task 6: Superviser crash, redémarrage et resynchronisation

**Files:**
- Create: `src/codex/CodexSupervisor.h`
- Create: `src/codex/CodexSupervisor.cpp`
- Test: `tests/codex/CodexSupervisorTests.cpp`
- Modify: `src/app/DeckApp.h`
- Modify: `src/app/DeckApp.cpp`

**Interfaces:**
- Produces: `enum class CodexConnectionState { Starting, Connected, Reconnecting, Unavailable };`
- Produces: callback `OnConnectionStateChanged`.
- Produces: callback `OnResyncRequired` après reconnexion réussie.

- [ ] **Step 1: Étendre le fake avec un mode crash**

Argument `--exit-after-initialize` : après la réponse `initialize`, le fake termine avec code 17.

- [ ] **Step 2: Écrire le test de reconnexion**

Injecter à `CodexSupervisor` une factory de launch specs qui lance d’abord le fake en mode crash puis le fake normal. Vérifier la séquence :

```text
Starting -> Connected -> Reconnecting -> Connected
```

et exactement un `OnResyncRequired` après la deuxième connexion.

- [ ] **Step 3: Implémenter une stratégie bornée**

Backoff de reconnexion : `250 ms`, `500 ms`, `1 s`, `2 s`, puis plafond `5 s`. Tant que l’application reste ouverte, continuer avec plafond 5 s. Un arrêt explicite annule immédiatement le `jthread` de supervision.

- [ ] **Step 4: Intégrer au cycle de vie de `DeckApp`**

Le démarrage du supervisor se fait après création/affichage initial de la fenêtre. La fermeture de l’app appelle `Stop()` avant destruction des ressources graphiques.

Ne jamais faire attendre la première peinture sur `Connect()`.

- [ ] **Step 5: Validation complète**

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Lancer avec un vrai Codex installé : vérifier connexion puis `thread/list` dans les logs de diagnostic, sans afficher encore les sessions dans une UI riche.

- [ ] **Step 6: Commit**

```bash
git add src/codex src/model src/app tests CMakeLists.txt
git commit -m "feat: supervise Codex app-server lifecycle"
```

## Critère de sortie du plan

Codex Deck démarre son `app-server` sans bloquer l’UI, peut lister/lire/créer/reprendre/renommer/archiver des threads et lancer un tour. Plusieurs runtimes sont suivis simultanément, les approvals sont routées par request ID et un crash du serveur déclenche une reconnexion puis une demande de resynchronisation. Tous les tests fonctionnent sans accès réel à Codex grâce à `FakeAppServer`.

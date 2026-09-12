# Codex Deck — Contrats transverses normatifs

Ce document complète les plans d’implémentation V1 et **prend priorité sur une signature ou un détail de toolchain contradictoire dans un plan individuel**. Il ne constitue pas un septième chantier : il verrouille les interfaces partagées que tous les plans doivent respecter.

**Spec de référence :** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## 1. C++23 strict côté MSVC

La cible produit est C++23, pas « C++23 ou plus récent ».

En septembre 2026, MSVC expose encore C++23 via `/std:c++23preview`, tandis que CMake peut traduire `cxx_std_23` en `/std:c++latest` pour MSVC. Codex Deck ne doit donc pas s’appuyer uniquement sur `target_compile_features(... cxx_std_23)` avec MSVC.

Le CMake final doit sélectionner explicitement le premier switch supporté parmi `/std:c++23` puis `/std:c++23preview`, et refuser de tomber sur `/std:c++latest` :

```cmake
include(CheckCXXCompilerFlag)

function(codex_deck_enable_cpp23 target_name)
    if(MSVC)
        check_cxx_compiler_flag("/std:c++23" CODEX_DECK_HAS_STD_CPP23)
        if(CODEX_DECK_HAS_STD_CPP23)
            target_compile_options(${target_name} PRIVATE /std:c++23)
        else()
            check_cxx_compiler_flag("/std:c++23preview" CODEX_DECK_HAS_STD_CPP23_PREVIEW)
            if(NOT CODEX_DECK_HAS_STD_CPP23_PREVIEW)
                message(FATAL_ERROR "Codex Deck requires an MSVC toolchain with C++23 support")
            endif()
            target_compile_options(${target_name} PRIVATE /std:c++23preview)
        endif()
    else()
        target_compile_features(${target_name} PRIVATE cxx_std_23)
    endif()
endfunction()
```

Appliquer cette fonction à `CodexDeck` et à chaque cible C++ de test/benchmark qui compile les headers du projet.

Pour MSVC, conserver également `/permissive-`, `/Zc:__cplusplus`, `/utf-8` et `/W4`.

## 2. Types Codex : summary vs detail

Les listes de threads et l’historique d’un thread n’ont pas le même contrat.

```cpp
struct CodexThreadSummary {
    CodexThreadId id;
    std::string name;
    std::filesystem::path cwd;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
    bool archived = false;
};

struct CodexTurnSnapshot {
    std::string id;
    std::string status;
    std::vector<nlohmann::json> items;
};

struct CodexThreadDetail {
    CodexThreadSummary summary;
    std::vector<CodexTurnSnapshot> turns;
};
```

Les `nlohmann::json` des `items` restent confinés à la frontière Codex/Workbench : ils ne sont jamais persistés dans SQLite.

Contrats obligatoires :

```cpp
using ThreadSummaryCompletion = std::move_only_function<
    void(std::expected<CodexThreadSummary, CodexError>)>;

using ThreadDetailCompletion = std::move_only_function<
    void(std::expected<CodexThreadDetail, CodexError>)>;

using ThreadListCompletion = std::move_only_function<
    void(std::expected<std::vector<CodexThreadSummary>, CodexError>)>;
```

- `thread/list` → `CodexThreadSummary`.
- `thread/start` → `CodexThreadSummary`.
- `thread/read(includeTurns=true)` → `CodexThreadDetail`.
- `thread/resume(includeTurns=true)` → `CodexThreadDetail`.
- `ThreadHistoryMapper` consomme `CodexThreadDetail`, pas une réponse RPC JSON arbitraire.

## 3. Liste active vs archives

`thread/list` retourne les threads non archivés lorsque `archived` est `false` ou absent. Les archives nécessitent une requête avec `archived: true`.

La façade utilise donc :

```cpp
struct ThreadListOptions {
    bool archived = false;
    std::optional<std::filesystem::path> cwd;
};

void ListThreads(ThreadListOptions options, ThreadListCompletion completion);
```

Pour chaque appel, `CodexClient` suit automatiquement tous les `nextCursor` jusqu’à `null`.

- La synchronisation de démarrage charge les threads **non archivés**.
- La vue `Archive` charge les threads archivés à la demande avec `archived=true` et les met en cache mémoire tant que la vue est ouverte.
- Après `thread/archive`, retirer la session de la vue active seulement après succès RPC ; invalider le cache Archive.

## 4. Un seul état de session partagé

`SessionRuntimeRegistry` est l’unique source runtime pour :

```text
Idle
Working
NeedsAttention
Completed
Error
```

Tree, ActivityBar, Workbench, fenêtres détachées et notifications observent ce registre. Aucun de ces composants n’entretient une seconde machine d’état concurrente.

## 5. Historique et live events

L’ouverture d’un thread part d’un `CodexThreadDetail` autoritatif puis applique les notifications live :

```text
turn/started
item/started
item/* deltas
item/completed
turn/diff/updated
turn/completed
error
```

Les `turn/started` et `turn/completed` peuvent contenir `items: []` ; les `item/*` sont donc la source canonique des items live. `item/completed` remplace la représentation partielle construite par les deltas pour le même `item.id`.

## 6. Inputs du composer

La structure wire V2 à respecter est :

```json
{"type":"text","text":"...","textElements":[]}
{"type":"localImage","path":"C:\\...\\image.png"}
{"type":"localAudio","path":"C:\\...\\audio.wav"}
{"type":"mention","name":"file.txt","path":"C:\\...\\file.txt"}
```

Les URLs HTTP distantes ne sont pas fabriquées à partir de fichiers locaux. Les pièces jointes non supportées hors workspace restent refusées explicitement en V1.

## 7. Diffs

Pour la vue Review changes, utiliser en priorité `turn/diff/updated.diff`, qui est le snapshot unified diff agrégé du tour. Les événements `item/fileChange/patchUpdated` servent de repli lorsqu’un turn-level diff n’est pas disponible. Ne pas reconstruire silencieusement la vérité Codex via `git diff`.

## 8. Persistance

SQLite contient seulement organisation et cache léger. Les structures suivantes restent mémoire-only :

```text
CodexThreadDetail.turns
TimelineDocument
sorties complètes de commandes
patches/diffs complets
texte complet des prompts/réponses
PendingApproval
```

## 9. Thread UI

Les seuls travaux autorisés directement dans le thread fenêtre sont :

- dispatch Win32 ;
- mutation légère de l’état de présentation ;
- hit testing ;
- layout/rendu des éléments visibles ;
- publication de commandes vers les services.

Process, pipes, RPC, parsing de gros payloads, SQLite, détection Git et recherche globale volumineuse restent hors thread UI.

## 10. Ordre de démarrage cache-first

Pour concilier restauration fidèle et démarrage perçu rapide, l’ordre normatif est :

```text
1. résoudre %LOCALAPPDATA% et ouvrir/migrer la petite base SQLite locale
2. lire WorkspaceState + cache léger projets/sessions
3. créer puis ShowWindow avec position, thème et cache restaurés
4. démarrer CodexSupervisor sur worker
5. à Connected, lancer la synchronisation Codex
6. après premier catalogue réconcilié, restaurer les Workbenches détachés encore valides
```

Les étapes 1–2 ne lancent **aucun** processus externe, Git, réseau ou parsing d’historique Codex. Elles doivent rester bornées au stockage local léger. En cas d’échec SQLite, afficher quand même la fenêtre avec un état par défaut puis signaler l’erreur localement.

La fenêtre ne doit jamais attendre `codex app-server` ou une synchronisation réseau avant son premier affichage.

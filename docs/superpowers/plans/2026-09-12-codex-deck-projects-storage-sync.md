# Codex Deck Projects, Storage & Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ajouter la couche d’organisation propre à Codex Deck : SQLite, projets logiques, `Unassigned`, associations automatiques/manuelles, cache de démarrage et réconciliation non bloquante avec les threads Codex.

**Architecture:** SQLite stocke uniquement organisation et cache léger. `ProjectDetector` associe les threads à partir du `cwd`, puis éventuellement de l’identité Git, sans gérer Git lui-même. `SessionSyncService` fusionne les threads Codex avec les métadonnées locales et publie des snapshots immuables utilisables par l’UI.

**Tech Stack:** C++23, SQLite, Win32 filesystem/process helpers, `std::expected`, CMake, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- Base : `%LOCALAPPDATA%\CodexDeck\codex-deck.db`.
- Aucune conversation, réponse complète, stdout complet ou diff complet dans SQLite.
- Une association manuelle a toujours priorité sur une association automatique.
- `Unassigned` n’est pas un faux projet SQLite : c’est l’absence de `project_id`.
- Les chemins Windows sont comparés sous forme normalisée et insensible à la casse, sans altérer la casse affichée.
- Les opérations SQLite se font hors thread UI.
- Les projets et sessions sont triés par activité récente au niveau modèle, mais le re-tri n’est déclenché que sur événements significatifs.

---

### Task 1: Créer la base, les migrations et le wrapper RAII

**Files:**
- Create: `src/storage/StorageError.h`
- Create: `src/storage/SqliteDatabase.h`
- Create: `src/storage/SqliteDatabase.cpp`
- Create: `src/storage/SchemaMigrator.h`
- Create: `src/storage/SchemaMigrator.cpp`
- Test: `tests/storage/SqliteDatabaseTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `OpenDatabase(path) -> expected<SqliteDatabase, StorageError>`.
- Produces: `SchemaMigrator::Migrate(SqliteDatabase&) -> expected<void, StorageError>`.

- [x] **Step 1: Écrire le test de migration sur fichier temporaire**

Le test crée `%TEMP%\CodexDeckTests\storage-<pid>.db`, appelle `Migrate`, puis vérifie :

```sql
SELECT value FROM app_meta WHERE key='schema_version';
```

Expected: `1`.

- [x] **Step 2: Vérifier l’échec**

```powershell
cmake --build --preset debug
ctest --preset debug -R SqliteDatabaseTests --output-on-failure
```

- [x] **Step 3: Définir l’erreur de stockage**

```cpp
enum class StorageErrorCode { OpenFailed, SqlFailed, MigrationFailed, ConstraintFailed };
struct StorageError {
    StorageErrorCode code;
    int sqlite_code = 0;
    std::string message;
};
```

- [x] **Step 4: Implémenter un wrapper move-only**

`SqliteDatabase` possède `sqlite3*`, interdit la copie, autorise move, ferme en destructeur et expose :

```cpp
std::expected<void, StorageError> Execute(std::string_view sql);
sqlite3* handle() const;
```

Activer à l’ouverture :

```sql
PRAGMA foreign_keys=ON;
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
```

- [x] **Step 5: Créer le schéma v1**

```sql
CREATE TABLE app_meta(
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE TABLE projects(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  git_remote TEXT,
  created_at INTEGER NOT NULL,
  updated_at INTEGER NOT NULL
);

CREATE TABLE project_roots(
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  root_path TEXT NOT NULL,
  normalized_path TEXT NOT NULL,
  PRIMARY KEY(project_id, normalized_path)
);
CREATE INDEX idx_project_roots_normalized ON project_roots(normalized_path);

CREATE TABLE session_metadata(
  thread_id TEXT PRIMARY KEY,
  project_id INTEGER REFERENCES projects(id) ON DELETE SET NULL,
  assignment_source INTEGER NOT NULL DEFAULT 0,
  favorite INTEGER NOT NULL DEFAULT 0,
  cached_title TEXT NOT NULL DEFAULT '',
  cached_cwd TEXT NOT NULL DEFAULT '',
  last_activity INTEGER NOT NULL DEFAULT 0,
  last_known_status INTEGER NOT NULL DEFAULT 0,
  updated_at INTEGER NOT NULL
);
CREATE INDEX idx_session_project ON session_metadata(project_id);
CREATE INDEX idx_session_activity ON session_metadata(last_activity DESC);

CREATE TABLE workspace_state(
  key TEXT PRIMARY KEY,
  value_json TEXT NOT NULL
);
```

`assignment_source`: `0=automatic`, `1=manual`.

- [x] **Step 6: Valider puis commit**

```powershell
cmake --build --preset debug
ctest --preset debug -R SqliteDatabaseTests --output-on-failure
git add src/storage tests/storage CMakeLists.txt
git commit -m "feat: add Codex Deck SQLite schema"
```

---

### Task 2: Ajouter les repositories projets et métadonnées de session

**Files:**
- Create: `src/projects/ProjectTypes.h`
- Create: `src/projects/ProjectRepository.h`
- Create: `src/projects/ProjectRepository.cpp`
- Create: `src/storage/SessionMetadataRepository.h`
- Create: `src/storage/SessionMetadataRepository.cpp`
- Test: `tests/projects/ProjectRepositoryTests.cpp`
- Test: `tests/storage/SessionMetadataRepositoryTests.cpp`

**Interfaces:**
- Produces: `using ProjectId = std::int64_t;`.
- Produces: `ProjectRepository::{Create, List, AddRoot, SetGitRemote, Delete}`.
- Produces: `SessionMetadataRepository::{Get, UpsertCache, SetAssignment, SetFavorite, ListAll}`.

- [ ] **Step 1: Définir les types**

```cpp
using ProjectId = std::int64_t;

enum class AssignmentSource : int { Automatic = 0, Manual = 1 };

struct Project {
    ProjectId id = 0;
    std::string name;
    std::vector<std::filesystem::path> roots;
    std::optional<std::string> git_remote;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
};

struct SessionMetadata {
    CodexThreadId thread_id;
    std::optional<ProjectId> project_id;
    AssignmentSource assignment_source = AssignmentSource::Automatic;
    bool favorite = false;
    std::string cached_title;
    std::filesystem::path cached_cwd;
    std::int64_t last_activity = 0;
    SessionStatus last_known_status = SessionStatus::Idle;
};
```

- [ ] **Step 2: Écrire les tests transactionnels**

Cas projet : créer `SpotifyAmp`, ajouter deux roots, relire, supprimer et vérifier cascade roots.

Cas session : upsert cache, assignation auto, puis assignation manuelle vers un autre projet ; relire et vérifier `Manual`.

- [ ] **Step 3: Implémenter les repositories avec statements préparés**

Aucune concaténation SQL de valeurs utilisateur. Les opérations multi-table `Create` + roots utilisent `BEGIN IMMEDIATE` / `COMMIT` avec rollback RAII en erreur.

- [ ] **Step 4: Ajouter normalisation de chemin**

Créer dans `ProjectRepository.cpp` une fonction privée réutilisable via `src/projects/PathNormalization.h/.cpp` :

```cpp
std::wstring NormalizeWindowsPath(const std::filesystem::path& path);
```

Règles : `weakly_canonical` si possible, séparateurs `\`, suppression du slash terminal hors racine, conversion en minuscules via `LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, ...)`.

- [ ] **Step 5: Valider et commit**

```powershell
ctest --preset debug -R "ProjectRepositoryTests|SessionMetadataRepositoryTests" --output-on-failure
git add src/projects src/storage tests
git commit -m "feat: persist projects and session metadata"
```

---

### Task 3: Détecter automatiquement un projet par cwd puis identité Git

**Files:**
- Create: `src/projects/GitProjectProbe.h`
- Create: `src/projects/GitProjectProbe.cpp`
- Create: `src/projects/ProjectDetector.h`
- Create: `src/projects/ProjectDetector.cpp`
- Test: `tests/projects/ProjectDetectorTests.cpp`

**Interfaces:**
- Produces: `GitProjectProbe::Inspect(cwd) -> expected<optional<GitProjectIdentity>, ProjectDetectionError>`.
- Produces: `ProjectDetector::Detect(thread, projects, metadata) -> optional<ProjectId>`.

- [ ] **Step 1: Définir l’identité Git sans moteur Git**

```cpp
struct GitProjectIdentity {
    std::filesystem::path root;
    std::optional<std::string> origin_remote;
};
```

`GitProjectProbe` exécute seulement :

```text
git -C <cwd> rev-parse --show-toplevel
git -C <root> remote get-url origin
```

via un petit process capturé interne. Un Git absent ou un cwd hors repo renvoie `std::nullopt`, pas une erreur fatale de synchronisation.

- [ ] **Step 2: Écrire les tests de priorité**

Construire trois projets synthétiques et vérifier :

```text
manual project_id présent       -> toujours conservé
cwd sous root A                 -> A
cwd sous root A/subroot B       -> B (root le plus long)
aucun root, remote identique C  -> C
aucun match                     -> nullopt (Unassigned)
```

Le `GitProjectProbe` est injecté par interface dans le test pour ne pas lancer Git.

- [ ] **Step 3: Implémenter la détection**

Signature :

```cpp
std::optional<ProjectId> DetectProject(
    const CodexThreadSummary& thread,
    const std::vector<Project>& projects,
    const std::optional<SessionMetadata>& existing_metadata,
    IGitProjectProbe& git_probe
);
```

Si `existing_metadata.assignment_source == Manual`, retourner exactement son `project_id`, y compris `nullopt` pour un déplacement manuel vers `Unassigned`.

- [ ] **Step 4: Valider et commit**

```powershell
ctest --preset debug -R ProjectDetectorTests --output-on-failure
git add src/projects tests/projects
git commit -m "feat: detect logical projects from Codex workspaces"
```

---

### Task 4: Construire le catalogue de sessions et la synchronisation Codex ↔ local

**Files:**
- Create: `src/model/SessionRecord.h`
- Create: `src/model/SessionCatalog.h`
- Create: `src/model/SessionCatalog.cpp`
- Create: `src/sync/SessionSyncService.h`
- Create: `src/sync/SessionSyncService.cpp`
- Test: `tests/model/SessionCatalogTests.cpp`
- Test: `tests/sync/SessionSyncTests.cpp`

**Interfaces:**
- Produces: `SessionCatalogSnapshot` immutable par valeur.
- Produces: `SessionSyncService::LoadCachedState()` puis `RefreshFromCodex()`.

- [ ] **Step 1: Définir le record fusionné**

```cpp
struct SessionRecord {
    CodexThreadSummary codex;
    std::optional<ProjectId> project_id;
    AssignmentSource assignment_source = AssignmentSource::Automatic;
    bool favorite = false;
    SessionStatus status = SessionStatus::Idle;
    bool present_in_codex = true;
};

struct SessionCatalogSnapshot {
    std::vector<Project> projects;
    std::vector<SessionRecord> sessions;
    std::uint64_t revision = 0;
};
```

- [ ] **Step 2: Écrire le test de démarrage cache-first**

Précharger SQLite avec deux sessions. `LoadCachedState()` doit publier immédiatement un snapshot contenant les deux sessions, `present_in_codex=false`, avant tout appel à `CodexClient`.

- [ ] **Step 3: Écrire le test de réconciliation**

Fake Codex renvoie : session A renommée, session B absente, session C nouvelle. Vérifier :

```text
A -> titre/cwd/updatedAt Codex actualisés, project manuel conservé
B -> reste en cache mais present_in_codex=false et n'est pas affichable comme session active
C -> créée, projet auto détecté ou Unassigned
```

- [ ] **Step 4: Implémenter `SessionCatalog` thread-safe**

Toutes les mutations sont réalisées sur le worker de sync ; la publication UI se fait par copie/move d’un `std::shared_ptr<const SessionCatalogSnapshot>` avec revision monotone. Aucun objet SQLite ne fuit vers l’UI.

- [ ] **Step 5: Implémenter `SessionSyncService`**

Ordre :

```text
LoadCachedState
  -> repositories
  -> publication snapshot cache

RefreshFromCodex
  -> CodexClient.ListThreads
  -> merge metadata
  -> auto-detection seulement si non-manual
  -> upserts SQLite dans transaction
  -> publication nouvelle revision
```

- [ ] **Step 6: Brancher `OnResyncRequired` du supervisor**

Après reconnexion app-server, déclencher `RefreshFromCodex()` ; si une sync est déjà active, poser un booléen `refresh_requested_again` et effectuer un second passage unique à la fin plutôt que lancer deux scans concurrents.

- [ ] **Step 7: Valider et commit**

```powershell
ctest --preset debug -R "SessionCatalogTests|SessionSyncTests" --output-on-failure
git add src/model src/sync tests/model tests/sync
git commit -m "feat: synchronize Codex sessions with local organization"
```

---

### Task 5: Implémenter tri significatif et mutations locales optimistes

**Files:**
- Create: `src/model/ActivityOrdering.h`
- Create: `src/model/ActivityOrdering.cpp`
- Create: `src/projects/ProjectAssignmentService.h`
- Create: `src/projects/ProjectAssignmentService.cpp`
- Test: `tests/model/ActivityOrderingTests.cpp`
- Test: `tests/projects/ProjectAssignmentServiceTests.cpp`

**Interfaces:**
- Produces: `SortProjectsAndSessions(snapshot) -> OrderedCatalogView`.
- Produces: `AssignManual(thread_id, optional<ProjectId>)`.
- Produces: `RevertToAutomatic(thread_id)`.

- [ ] **Step 1: Écrire le test de tri**

Vérifier : sessions décroissantes par `last_activity`; projet trié selon la session la plus récente; `Unassigned` hors classement et placé dans sa zone fixe par la future UI.

- [ ] **Step 2: Définir les événements qui modifient `last_activity`**

Mettre à jour l’activité uniquement sur :

```text
création/reprise de thread
prompt envoyé
turn/completed
approval reçue
sync externe avec updatedAt plus récent
```

Ne pas mettre à jour sur delta de streaming, ligne stdout ou repaint UI.

- [ ] **Step 3: Implémenter l’association manuelle optimiste**

`AssignManual` publie d’abord un snapshot avec le nouveau `project_id`, persiste ensuite SQLite sur le worker ; en cas d’erreur, republie l’ancienne valeur et expose `StorageError` au callback d’erreur UI.

`AssignManual(thread, nullopt)` signifie explicitement « garder cette session dans Unassigned » et stocke `assignment_source=Manual`.

- [ ] **Step 4: Valider et commit**

```powershell
ctest --preset debug -R "ActivityOrderingTests|ProjectAssignmentServiceTests" --output-on-failure
git add src/model src/projects tests
git commit -m "feat: order and assign Codex Deck sessions"
```

---

### Task 6: Intégrer le démarrage cache-first dans l’application

**Files:**
- Modify: `src/app/DeckApp.h`
- Modify: `src/app/DeckApp.cpp`
- Create: `src/storage/AppDataPaths.h`
- Create: `src/storage/AppDataPaths.cpp`
- Test: `tests/storage/AppDataPathsTests.cpp`

**Interfaces:**
- Produces: `CodexDeckDatabasePath() -> filesystem::path`.
- Consumes: `SessionSyncService`, `CodexSupervisor`.

- [ ] **Step 1: Tester le chemin applicatif**

Avec une racine LocalAppData injectée `C:\Users\Test\AppData\Local`, attendre :

```text
C:\Users\Test\AppData\Local\CodexDeck\codex-deck.db
```

- [ ] **Step 2: Implémenter `AppDataPaths` avec `SHGetKnownFolderPath(FOLDERID_LocalAppData)`**

Créer le répertoire `CodexDeck` avec `std::filesystem::create_directories` avant ouverture DB.

- [ ] **Step 3: Orchestrer l’ordre de lancement**

Dans `DeckApp::Run` :

```text
1. créer/afficher la fenêtre
2. ouvrir SQLite sur worker initial
3. LoadCachedState et publier
4. démarrer CodexSupervisor
5. à Connected, RefreshFromCodex
```

Le renderer peut afficher un état local vide pendant 2–4 ; aucune attente réseau/process avant `ShowWindow`.

- [ ] **Step 4: Validation complète**

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Test manuel : lancer une fois avec Codex disponible, fermer, relancer avec Codex volontairement indisponible ; le cache de sessions/projets doit rester chargeable sans écran bloquant.

- [ ] **Step 5: Commit**

```bash
git add src/app src/storage tests/storage
git commit -m "feat: restore cached Codex Deck organization at startup"
```

## Critère de sortie du plan

Au démarrage, Codex Deck peut afficher immédiatement un catalogue mis en cache, puis le réconcilier avec `thread/list`. Les projets sont logiques, `Unassigned` représente une absence d’association, une assignation manuelle n’est jamais écrasée par l’auto-détection, et aucun contenu complet de conversation n’est dupliqué en SQLite.

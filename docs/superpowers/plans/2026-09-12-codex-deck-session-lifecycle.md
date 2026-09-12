# Codex Deck Session & Project Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compléter la navigation par les flux indispensables au quotidien : créer/gérer un projet logique, créer une session globale ou dans le projet courant, ouvrir un dossier directement, gérer favoris/archives et détecter les changements effectués depuis d’autres clients Codex pendant que Deck reste ouvert.

**Architecture:** Les overlays de création restent minces et délèguent à des controllers testables. `SessionCreationController` appelle `CodexClient::StartThread`; `ProjectManagementController` écrit uniquement via les repositories locaux. `ExternalSessionRefreshScheduler` ne lit jamais les fichiers internes Codex : il déclenche des `thread/list` légers puis une sync complète uniquement si nécessaire.

**Tech Stack:** C++23, Win32 `IFileDialog`, DirectComposition overlays, Codex app-server V2, SQLite repositories existants, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

**Contrats:** `docs/superpowers/plans/2026-09-12-codex-deck-shared-contracts.md`

## Global Constraints

- `Ctrl+Shift+N` crée immédiatement un thread dans le projet courant, sans wizard.
- Depuis le bouton `+ New session` d’un projet, le projet/workspace sont déjà connus ; créer puis ouvrir le Workbench directement.
- `Ctrl+N` ouvre un overlay global compact permettant projet/workspace/modèle/prompt initial.
- `Open folder…` crée un thread sur ce cwd ; la sync l’associe automatiquement à un projet connu ou le laisse dans `Unassigned`.
- Une création de projet ne modifie jamais un repo Git et ne crée aucun fichier dans le workspace.
- Les changements faits par VS Code/CLI sont détectés uniquement par les API `app-server`, via refresh périodique/activation ; pas de parsing direct de `~/.codex`.

---

### Task 1: Étendre le client Codex avec catalogue modèles et options de listing rapides

**Files:**
- Modify: `src/codex/CodexTypes.h`
- Modify: `src/codex/CodexClient.h`
- Modify: `src/codex/CodexClient.cpp`
- Modify: `tests/fakes/FakeAppServer.cpp`
- Test: `tests/codex/CodexModelListTests.cpp`
- Test: `tests/codex/CodexThreadListOptionsTests.cpp`

**Interfaces:**
- Produces: `ListModels()`.
- Extends: `ThreadListOptions` avec tri, limite et `use_state_db_only` pour les refresh probes.

- [ ] **Step 1: Définir un modèle minimal**

```cpp
struct CodexModelInfo {
    std::string id;
    std::string display_name;
    bool is_default = false;
    std::vector<std::string> supported_efforts;
};
```

Le parser ignore les champs modèle inconnus et garde uniquement les valeurs réellement disponibles.

- [ ] **Step 2: Écrire le test `model/list` paginé**

Le fake renvoie deux pages. `ListModels()` doit concaténer et préserver l’ordre serveur.

Wire initial :

```json
{"method":"model/list","params":{"limit":100}}
```

Puis réutiliser `nextCursor` jusqu’à `null`.

- [ ] **Step 3: Étendre `ThreadListOptions`**

Le contrat final devient :

```cpp
struct ThreadListOptions {
    bool archived = false;
    std::optional<std::filesystem::path> cwd;
    std::string sort_key = "recency_at";
    std::string sort_direction = "desc";
    std::optional<std::size_t> max_items;
    bool use_state_db_only = false;
};
```

`max_items == nullopt` signifie pagination exhaustive. Une valeur `100` permet un probe récent sans scanner toutes les pages.

- [ ] **Step 4: Encoder les options exactement**

```json
{
  "limit":100,
  "sortKey":"recency_at",
  "sortDirection":"desc",
  "archived":false,
  "useStateDbOnly":true
}
```

Pour un appel exhaustif, poursuivre `nextCursor`. Pour un probe `max_items=100`, arrêter après 100 entrées même si un cursor existe.

- [ ] **Step 5: Valider et commit**

```powershell
cmake --build --preset debug
ctest --preset debug -R "CodexModelListTests|CodexThreadListOptionsTests" --output-on-failure
git add src/codex tests/codex tests/fakes
git commit -m "feat: expose Codex model and recent thread catalogs"
```

---

### Task 2: Ajouter la gestion locale des projets depuis l’UI

**Files:**
- Create: `src/projects/ProjectManagementController.h`
- Create: `src/projects/ProjectManagementController.cpp`
- Create: `src/projects/ProjectEditorModel.h`
- Create: `src/projects/ProjectEditorView.h`
- Create: `src/projects/ProjectEditorView.cpp`
- Create: `src/platform/FolderPicker.h`
- Create: `src/platform/FolderPicker.cpp`
- Test: `tests/projects/ProjectManagementControllerTests.cpp`
- Modify: `src/app/DeckCommand.h`
- Modify: `src/navigation/CommandPaletteModel.cpp`

**Interfaces:**
- Produces: Create/Rename/Delete logical project.
- Produces: `PickFolder(HWND owner) -> expected<optional<path>, PlatformError>`.

- [ ] **Step 1: Ajouter les commandes**

```cpp
enum class DeckCommandKind {
    // existantes…
    NewProject,
    RenameProject,
    DeleteProject,
    OpenFolderAsSession,
};
```

- [ ] **Step 2: Écrire les tests controller**

Cas : créer `SpotifyAmp` avec root `D:\VibeCoding\spotifyamp`, renommer en `SpotifyAmp Native`, supprimer. Après suppression, les `session_metadata.project_id` deviennent `NULL` via FK ; si leur association était manuelle, conserver `assignment_source=Manual`, ce qui signifie Unassigned explicite.

- [ ] **Step 3: Implémenter le folder picker natif**

Utiliser `IFileOpenDialog`, `FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST`. Annulation utilisateur retourne `std::nullopt`, pas une erreur.

- [ ] **Step 4: Construire l’overlay projet**

Champs V1 : `Name`, `Root folder`. Le nom par défaut est `root.filename()`. Après création réussie : rafraîchir catalogue, développer le nouveau projet et le sélectionner dans l’arbre.

- [ ] **Step 5: Suppression avec confirmation non système**

Overlay Codex Deck : expliquer que seul le projet logique local est supprimé ; aucun fichier/repo/session Codex n’est supprimé. Actions `Cancel` / `Remove project`.

- [ ] **Step 6: Commit**

```bash
git add src/projects src/platform src/app src/navigation tests/projects
git commit -m "feat: manage logical Codex Deck projects"
```

---

### Task 3: Construire le controller de création de session

**Files:**
- Create: `src/sessions/SessionCreationTypes.h`
- Create: `src/sessions/SessionCreationController.h`
- Create: `src/sessions/SessionCreationController.cpp`
- Test: `tests/sessions/SessionCreationControllerTests.cpp`
- Modify: `src/codex/CodexClient.h`
- Modify: `src/codex/CodexClient.cpp`

**Interfaces:**
- Produces: `CreateSession(CreateSessionRequest, completion)`.

- [ ] **Step 1: Définir la requête**

```cpp
struct CreateSessionRequest {
    std::optional<ProjectId> project_id;
    std::filesystem::path cwd;
    std::optional<std::string> model;
    std::optional<std::string> initial_prompt;
};

struct CreatedSession {
    CodexThreadSummary thread;
    std::optional<ProjectId> project_id;
};
```

- [ ] **Step 2: Écrire le test projet courant**

Input : project SpotifyAmp avec root principal, model null, prompt null. Vérifier `thread/start` avec cwd et sans override model. Au succès : métadonnée locale auto associée au project, session injectée dans catalogue, callback sélection session.

- [ ] **Step 3: Écrire le test prompt initial**

Si `initial_prompt` non vide : attendre succès `thread/start`, puis appeler `turn/start` avec input texte. Si `turn/start` échoue, la session créée reste valide/ouverte et le prompt optimiste est marqué en erreur ; ne pas supprimer le thread.

- [ ] **Step 4: Implémenter création et association**

`SessionCreationController` ne duplique pas la logique de `ProjectDetector`. Si project explicite : `ProjectAssignmentService::AssignManual(thread_id, project_id)` après création. Si aucun projet explicite : laisser la sync auto détecter à partir de `cwd`.

- [ ] **Step 5: Commit**

```bash
git add src/sessions src/codex tests/sessions
git commit -m "feat: create Codex sessions from Deck"
```

---

### Task 4: Implémenter les trois flux de création validés

**Files:**
- Create: `src/sessions/NewSessionOverlayModel.h`
- Create: `src/sessions/NewSessionOverlayModel.cpp`
- Create: `src/sessions/NewSessionOverlayView.h`
- Create: `src/sessions/NewSessionOverlayView.cpp`
- Modify: `src/navigation/ProjectTreeView.cpp`
- Modify: `src/navigation/CommandPaletteModel.cpp`
- Modify: `src/input/KeyboardShortcuts.cpp`
- Modify: `src/app/DeckApp.cpp`
- Test: `tests/sessions/NewSessionOverlayModelTests.cpp`

**Interfaces:**
- Produces: global `Ctrl+N` overlay.
- Consumes: `ListModels`, `SessionCreationController`, `FolderPicker`.

- [ ] **Step 1: Tester le modèle global**

État initial : project courant pré-sélectionné si disponible, sinon `Unassigned`; workspace = root principal du projet sélectionné, sinon vide ; model = `Default`; prompt vide.

Changer project recalcule workspace seulement si l’utilisateur ne l’avait pas modifié manuellement.

- [ ] **Step 2: Créer le global `Ctrl+N` overlay**

Disposition compacte :

```text
New session
Project    [ SpotifyAmp ▾ ]
Workspace  [ D:\VibeCoding\spotifyamp ] [Browse]
Model      [ Default ▾ ]
Prompt     [ optional multiline field ]

[Cancel]                         [Create]
```

Le chargement `model/list` est asynchrone ; l’overlay reste utilisable avec `Default` si la liste n’est pas encore disponible ou échoue.

- [ ] **Step 3: Implémenter `Ctrl+Shift+N`**

Précondition : projet courant avec au moins un root. Appeler immédiatement `CreateSessionRequest{project_id,currentRoot,nullopt,nullopt}`. À succès, sélectionner la session et focaliser son Workbench/composer. Si aucun projet courant : ouvrir le global `Ctrl+N` plutôt que faire échouer silencieusement.

- [ ] **Step 4: Ajouter `+ New session` sur Project row/context menu**

Même flux immédiat que `Ctrl+Shift+N`, avec project id de la ligne. Aucun dialogue intermédiaire.

- [ ] **Step 5: Ajouter `Open folder…`**

FolderPicker → `CreateSessionRequest{nullopt, folder, nullopt, nullopt}`. Après création, la session est sélectionnée. La sync lui attribue un projet connu si possible ; sinon `Unassigned`.

- [ ] **Step 6: Valider et commit**

```powershell
ctest --preset debug -R "NewSessionOverlayModelTests|SessionCreationControllerTests" --output-on-failure
git add src/sessions src/navigation src/input src/app tests/sessions
git commit -m "feat: add fast Codex session creation flows"
```

---

### Task 5: Finaliser favoris et vue Archive

**Files:**
- Create: `src/sessions/FavoriteService.h`
- Create: `src/sessions/FavoriteService.cpp`
- Create: `src/sessions/ArchiveSessionService.h`
- Create: `src/sessions/ArchiveSessionService.cpp`
- Create: `src/navigation/ArchiveViewModel.h`
- Create: `src/navigation/ArchiveViewModel.cpp`
- Test: `tests/sessions/FavoriteServiceTests.cpp`
- Test: `tests/sessions/ArchiveSessionServiceTests.cpp`
- Modify: `src/codex/CodexClient.h`
- Modify: `src/codex/CodexClient.cpp`
- Modify: `src/navigation/CommandPaletteModel.cpp`

**Interfaces:**
- Produces: `ToggleFavorite(thread_id)` optimistic + rollback.
- Produces: lazy `LoadArchive()` et `RestoreArchivedThread()`.

- [ ] **Step 1: Écrire le test favori**

Toggle false→true publie snapshot immédiatement, persiste SQLite, et rollback si repository renvoie erreur.

- [ ] **Step 2: Implémenter filtre Favorites**

L’entrée globale `★ Favorites` réutilise le catalogue actif et ne fait aucun RPC.

- [ ] **Step 3: Étendre le fake et client pour `thread/unarchive`**

Ajouter :

```cpp
void UnarchiveThread(CodexThreadId thread_id, VoidCompletion completion);
```

Wire : `thread/unarchive` `{threadId}`.

- [ ] **Step 4: Charger Archive à la demande**

`ListThreads({.archived=true})` exhaustif à l’ouverture de la vue. Afficher liste virtualisée par activité récente. La vue n’insère pas ces threads dans l’arbre principal.

- [ ] **Step 5: Restaurer**

Après succès `thread/unarchive`, invalider archive cache puis `SessionSyncService::RequestRefresh()`. Le thread réapparaît dans son association locale précédente si la métadonnée existe, sinon auto-détection.

- [ ] **Step 6: Commit**

```bash
git add src/sessions src/navigation src/codex tests/sessions
git commit -m "feat: complete favorites and archive lifecycle"
```

---

### Task 6: Détecter les changements des autres clients sans lire les fichiers Codex

**Files:**
- Create: `src/sync/ExternalSessionRefreshScheduler.h`
- Create: `src/sync/ExternalSessionRefreshScheduler.cpp`
- Modify: `src/sync/SessionSyncService.h`
- Modify: `src/sync/SessionSyncService.cpp`
- Modify: `src/app/DeckApp.cpp`
- Test: `tests/sync/ExternalSessionRefreshSchedulerTests.cpp`
- Test: `tests/sync/ExternalSessionProbeTests.cpp`

**Interfaces:**
- Produces: foreground interval 15 s, background interval 60 s, immediate probe on app activation.
- Produces: cheap first-page probe, full refresh only on detected change.

- [ ] **Step 1: Écrire le test scheduler déterministe**

Avec clock injectée :

```text
foreground -> probe à t=15,30,45...
background -> probe à t=60,120...
activation background→foreground -> probe immédiat
sync/probe déjà actif -> coalescer, jamais deux appels simultanés
```

- [ ] **Step 2: Définir le fingerprint récent**

Probe :

```cpp
ThreadListOptions{
    .archived = false,
    .sort_key = "recency_at",
    .sort_direction = "desc",
    .max_items = 100,
    .use_state_db_only = true,
};
```

Fingerprint = hash stable des tuples `(thread.id, thread.name, thread.cwd, thread.updated_at)` des 100 plus récents.

- [ ] **Step 3: Implémenter probe → full sync**

Si fingerprint identique au précédent : rien. Si différent : appeler `RequestRefresh()` exhaustif. Après full sync réussi, recalculer le fingerprint depuis les 100 sessions actives les plus récentes du snapshot local.

- [ ] **Step 4: Brancher activation fenêtre**

Sur `WM_ACTIVATEAPP(TRUE)`, appeler `RequestImmediateProbe()`. Sur passage background, changer seulement l’intervalle ; ne pas arrêter les sessions Codex locales.

- [ ] **Step 5: Tester création/rename externes**

Fake : probe1 A/B ; probe2 C/A-renamed/B → full sync attendu une fois, catalogue contient C et nom mis à jour pour A, association manuelle éventuelle conservée.

- [ ] **Step 6: Commit**

```bash
git add src/sync src/app tests/sync
git commit -m "feat: detect Codex sessions changed by other clients"
```

---

### Task 7: Ajouter les contrôles globaux de thème et préférences utiles

**Files:**
- Create: `src/settings/DeckPreferences.h`
- Create: `src/settings/DeckPreferencesService.h`
- Create: `src/settings/DeckPreferencesService.cpp`
- Modify: `src/navigation/CommandPaletteModel.cpp`
- Modify: `src/theme/Theme.cpp`
- Test: `tests/settings/DeckPreferencesServiceTests.cpp`

**Interfaces:**
- Produces: `SetThemeMode(System|Light|Dark)` avec persistance locale.
- Prépare les flags notification du plan hardening sans dépendre de ce module futur.

- [ ] **Step 1: Définir préférences**

```cpp
struct DeckPreferences {
    ThemeMode theme_mode = ThemeMode::System;
    bool notify_approvals = true;
    bool notify_errors = true;
    bool notify_completions = true;
    bool reduced_motion_override = false;
};
```

Stocker sous `workspace_state['preferences']` en JSON versionné séparé de la géométrie fenêtres.

- [ ] **Step 2: Ajouter actions palette**

```text
Theme: Follow system
Theme: Light
Theme: Dark
```

L’action change immédiatement le thème global et persiste hors thread UI via le save scheduler.

- [ ] **Step 3: Ajouter Toggle Favorite à la palette/context menu**

Le command mapping existant appelle `FavoriteService`, pas directement SQLite.

- [ ] **Step 4: Valider et commit**

```bash
git add src/settings src/navigation src/theme tests/settings
git commit -m "feat: add Codex Deck user preferences"
```

## Critère de sortie du plan

Toutes les fonctions de cycle de vie V1 sont accessibles : projets locaux, création ultra-rapide de sessions, dossier direct, favoris, archive/restauration et thèmes. Une session créée ou renommée depuis VS Code/CLI est détectée sans redémarrer Codex Deck grâce aux probes `thread/list`, sans lecture directe des fichiers internes Codex.

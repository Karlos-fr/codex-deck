# Codex Deck V1 Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transformer l’application fonctionnelle issue des plans précédents en V1 fiable et agréable au quotidien : fenêtres détachées, restauration complète, notifications Windows, diagnostics, Reduced Motion, accessibilité de base, benchmarks et packaging portable.

**Architecture:** `AppServices` possède les services partagés process/Codex/catalogue/stockage ; `WindowManager` possède la fenêtre principale et les Workbench détachés. L’état restaurable est sérialisé dans `workspace_state`. Les notifications et diagnostics restent des adapters Windows isolés. Les métriques de performance sont instrumentées mais ne polluent pas le chemin de rendu normal.

**Tech Stack:** C++23, Win32, Direct2D/DirectWrite/DirectComposition, Shell APIs, SQLite, CMake/CTest/CPack-style ZIP command, ETW/`QueryPerformanceCounter`-style internal timing.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- Une seule instance de `CodexSupervisor`, `CodexClient`, `SessionCatalog` et base SQLite pour toutes les fenêtres.
- Les fenêtres détachées contiennent uniquement un Workbench.
- Les notifications Windows sont émises uniquement si Codex Deck n’est pas au premier plan et sur `NeedsAttention`, `Error` ou fin de travail significative.
- Pas de notification par commande intermédiaire.
- `System / Light / Dark` reste global à l’application.
- `Reduced Motion` désactive les animations non nécessaires sans supprimer les changements d’état visuels.
- Les benchmarks sont reproductibles ; les chiffres cibles sont signalés comme objectifs et non comme garanties matérielles universelles.
- Le package V1 reste portable et ne requiert pas de droits administrateur.

---

### Task 1: Introduire `AppServices` et un vrai `WindowManager` multi-fenêtres

**Files:**
- Create: `src/app/AppServices.h`
- Create: `src/app/AppServices.cpp`
- Create: `src/window/WindowManager.h`
- Create: `src/window/WindowManager.cpp`
- Create: `src/window/WorkbenchWindow.h`
- Create: `src/window/WorkbenchWindow.cpp`
- Modify: `src/app/DeckApp.h`
- Modify: `src/app/DeckApp.cpp`
- Test: `tests/window/WindowManagerTests.cpp`

**Interfaces:**
- Produces: `AppServices` partagé par `std::shared_ptr`.
- Produces: `WindowManager::DetachThread`, `FocusThread`, `CloseDetachedThread`.

- [ ] **Step 1: Définir les services partagés**

```cpp
struct AppServices {
    std::shared_ptr<CodexSupervisor> codex_supervisor;
    std::shared_ptr<SessionSyncService> session_sync;
    std::shared_ptr<SessionCatalog> session_catalog;
    std::shared_ptr<ProjectAssignmentService> project_assignment;
    std::shared_ptr<WorkbenchController> workbench_controller;
};
```

`AppServices` ne contient aucune ressource HWND/Direct2D propre à une fenêtre.

- [ ] **Step 2: Écrire le test de politique de détachement**

Le modèle `WindowManager` doit garantir un seul Workbench détaché par `thread_id`. Deux appels `DetachThread("thr_a")` renvoient/focalisent le même enregistrement logique au lieu d’en créer deux.

Pour rendre le test sans fenêtre réelle, isoler :

```cpp
struct WindowRecord {
    CodexThreadId thread_id;
    HWND hwnd = nullptr;
    bool detached = false;
};
```

et tester la table de records via une factory HWND injectée.

- [ ] **Step 3: Créer `WorkbenchWindow`**

Une fenêtre détachée possède :

```text
header session
----------------
timeline/diff review
----------------
composer
```

Elle ne possède jamais ProjectTree, ActivityBar ou Command Palette globale. Elle utilise le même `WorkbenchController` et un `WorkbenchView` propre à son HWND/CompositionHost.

- [ ] **Step 4: Router `Ctrl+Shift+D`**

Depuis la fenêtre principale : si thread sélectionné, `WindowManager::DetachThread`. Depuis une fenêtre détachée : raccourci ramène le focus à la fenêtre principale et laisse la fenêtre détachée ouverte.

- [ ] **Step 5: Gérer la fermeture**

Fermer une fenêtre détachée détruit uniquement ses ressources graphiques et son état de scroll. Fermer la fenêtre principale ferme toutes les fenêtres, stoppe les services puis quitte le process.

- [ ] **Step 6: Valider et commit**

```powershell
cmake --build --preset debug
ctest --preset debug -R WindowManagerTests --output-on-failure
git add src/app src/window tests/window
git commit -m "feat: support detached Codex workbench windows"
```

---

### Task 2: Persister et restaurer tout l’espace de travail utile

**Files:**
- Create: `src/storage/WorkspaceState.h`
- Create: `src/storage/WorkspaceStateRepository.h`
- Create: `src/storage/WorkspaceStateRepository.cpp`
- Create: `src/window/WindowPlacement.h`
- Create: `src/window/WindowPlacement.cpp`
- Modify: `src/window/WindowManager.cpp`
- Modify: `src/app/DeckApp.cpp`
- Test: `tests/storage/WorkspaceStateTests.cpp`
- Test: `tests/window/WindowPlacementTests.cpp`

**Interfaces:**
- Produces: versioned `WorkspaceState` JSON stocké sous `workspace_state['main']`.

- [ ] **Step 1: Définir le schéma runtime**

```cpp
struct SavedWindowRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    bool maximized = false;
};

struct DetachedWorkbenchState {
    CodexThreadId thread_id;
    SavedWindowRect window;
    float timeline_scroll = 0.0f;
};

struct WorkspaceState {
    int version = 1;
    SavedWindowRect main_window;
    float tree_width = 300.0f;
    std::vector<ProjectId> expanded_projects;
    bool unassigned_expanded = true;
    std::optional<CodexThreadId> selected_thread;
    float selected_timeline_scroll = 0.0f;
    std::vector<DetachedWorkbenchState> detached;
    ThemeMode theme_mode = ThemeMode::System;
};
```

- [ ] **Step 2: Écrire round-trip + compatibilité**

Sérialiser puis désérialiser un état complet et comparer tous les champs. Tester un JSON `version=1` contenant un champ inconnu : il doit être ignoré. JSON invalide : retourner état par défaut sans écraser immédiatement la valeur corrompue ; logger l’erreur.

- [ ] **Step 3: Implémenter la visibilité multi-écrans**

`RestoreVisibleWindowRect` vérifie l’intersection avec `EnumDisplayMonitors`. Si aucune intersection d’au moins 64x64 px, replacer sur le moniteur principal avec taille par défaut. Appliquer DPI au moment de créer la fenêtre via `AdjustWindowRectExForDpi`.

- [ ] **Step 4: Sauvegarder de manière debounce**

Les mouvements/resizes/scroll ne doivent pas écrire SQLite à chaque message. `WorkspaceStateSaveScheduler` déclenche une sauvegarde 500 ms après la dernière mutation, plus une sauvegarde synchrone finale contrôlée au shutdown.

- [ ] **Step 5: Restaurer dans l’ordre sûr**

```text
1. charger WorkspaceState depuis SQLite/cache local
2. créer et montrer la fenêtre principale
3. restaurer tree width/expanded/selected si IDs encore connus
4. démarrer Codex/sync
5. après premier catalogue réconcilié, recréer seulement les fenêtres détachées dont le thread existe encore
```

Un thread absent n’empêche jamais le démarrage.

- [ ] **Step 6: Commit**

```bash
git add src/storage src/window src/app tests
git commit -m "feat: restore Codex Deck workspace across launches"
```

---

### Task 3: Ajouter les notifications Windows et le routage vers la session

**Files:**
- Create: `src/notifications/NotificationTypes.h`
- Create: `src/notifications/NotificationPolicy.h`
- Create: `src/notifications/NotificationPolicy.cpp`
- Create: `src/notifications/WindowsNotificationService.h`
- Create: `src/notifications/WindowsNotificationService.cpp`
- Create: `src/notifications/NotificationController.h`
- Create: `src/notifications/NotificationController.cpp`
- Modify: `src/resources/ResourceIds.h`
- Modify: `src/resources/strings.fr.rc`
- Modify: `src/resources/strings.en.rc`
- Test: `tests/notifications/NotificationPolicyTests.cpp`

**Interfaces:**
- Produces: `ShouldNotify(event, app_foreground, preferences) -> bool`.
- Produces: click callback contenant `thread_id`.

- [ ] **Step 1: Définir la politique par tests**

Table exacte :

```text
foreground + approval     -> false
background + approval     -> true
background + error        -> true
background + completed    -> true si tour lancé par l'utilisateur et durée >= 3 s
background + command done -> false
background + streaming    -> false
catégorie désactivée      -> false
```

- [ ] **Step 2: Définir les préférences**

```cpp
struct NotificationPreferences {
    bool approvals = true;
    bool errors = true;
    bool completions = true;
};
```

Les stocker dans `workspace_state` ou une clé settings JSON globale, pas dans config Codex.

- [ ] **Step 3: Implémenter une notification native sans Windows App SDK**

Réutiliser une icône tray permanente via `Shell_NotifyIconW`. Pour une alerte, envoyer `NIM_MODIFY` avec `NIF_INFO | NIF_MESSAGE | NIF_ICON`, `NIIF_INFO`/`NIIF_WARNING` selon type et `szInfo`/`szInfoTitle` bornés.

Le callback tray moderne `NOTIFYICON_VERSION_4` reçoit `NIN_BALLOONUSERCLICK`; `NotificationController` mémorise le `thread_id` associé à la dernière notification active et appelle `WindowManager::FocusThread(thread_id)` au clic.

Si plusieurs alertes arrivent avant clic, préférer dans l’ordre `NeedsAttention > Error > Completed` et conserver une file bornée à 16 ; une notification suivante est émise après timeout/callback de la précédente.

- [ ] **Step 4: Localiser les textes**

Ajouter FR/EN pour : besoin d’approbation, erreur de session, travail terminé. Ne jamais injecter un prompt complet dans la notification ; titre session tronqué à 80 caractères, résumé à 180.

- [ ] **Step 5: Valider manuellement**

Mettre Codex Deck en arrière-plan, simuler avec le fake : approval, error, completion. Vérifier qu’un clic revient sur la bonne session. Remettre l’app au premier plan : aucune notification shell.

- [ ] **Step 6: Commit**

```bash
git add src/notifications src/resources tests/notifications
git commit -m "feat: add focused Windows notifications"
```

---

### Task 4: Ajouter diagnostics locaux et crash breadcrumbs sans données de conversation

**Files:**
- Create: `src/diagnostics/Logger.h`
- Create: `src/diagnostics/Logger.cpp`
- Create: `src/diagnostics/PerformanceMetrics.h`
- Create: `src/diagnostics/PerformanceMetrics.cpp`
- Create: `src/diagnostics/DiagnosticPaths.h`
- Create: `src/diagnostics/DiagnosticPaths.cpp`
- Test: `tests/diagnostics/LoggerTests.cpp`
- Modify: `src/app/DeckApp.cpp`
- Modify: `src/codex/CodexSupervisor.cpp`

**Interfaces:**
- Produces: logs rotatifs texte UTF-8 sous `%LOCALAPPDATA%\CodexDeck\logs`.
- Interdit: prompt/réponse/stdout/diff brut dans les logs normaux.

- [ ] **Step 1: Écrire le test de redaction**

Entrée diagnostic structurée :

```cpp
DiagnosticEvent{
  .category="codex",
  .name="rpc_failed",
  .fields={{"method","thread/read"},{"threadId","thr_123"},{"payload","SECRET PROMPT"}}
}
```

Le logger doit rejeter/omettre le champ `payload`. Autoriser seulement une allowlist : `method`, `threadId`, `turnId`, `errorCode`, `durationMs`, `count`, `state`.

- [ ] **Step 2: Implémenter rotation bornée**

`codex-deck.log` max 2 MiB ; rotations `.1`, `.2`, `.3`. Mutex interne, écritures append, timestamps ISO locaux et thread id système. Pas de worker bloquant obligatoire : les messages doivent rester courts.

- [ ] **Step 3: Ajouter métriques de timing**

Mesurer via `QueryPerformanceCounter` :

```text
process_start -> first_window_show
first_window_show -> cache_snapshot_published
app_server_start -> connected
connected -> first_sync_complete
frame render duration
search duration
```

`PerformanceMetrics` agrège count/min/max/mean et p95 via reservoir borné à 256 valeurs par métrique.

- [ ] **Step 4: Journaliser les états de supervision**

Seulement `Starting/Connected/Reconnecting/Unavailable`, codes d’erreur, méthode RPC échouée et IDs techniques. Aucun texte utilisateur.

- [ ] **Step 5: Commit**

```bash
git add src/diagnostics src/app src/codex tests/diagnostics
git commit -m "feat: add privacy-conscious diagnostics"
```

---

### Task 5: Respecter Reduced Motion et finaliser Motion/Glass utile

**Files:**
- Create: `src/theme/MotionPreferences.h`
- Create: `src/theme/MotionPreferences.cpp`
- Create: `src/ui/UiAnimation.h`
- Create: `src/ui/UiAnimation.cpp`
- Modify: `src/navigation/CommandPaletteView.cpp`
- Modify: `src/workbench/WorkbenchView.cpp`
- Modify: `src/rendering/DeckRenderer.cpp`
- Test: `tests/ui/UiAnimationTests.cpp`

**Interfaces:**
- Produces: `SystemAnimationsEnabled()` via `SPI_GETCLIENTAREAANIMATION`.
- Produces: `AnimationPolicy {enabled, micro_duration, structural_duration}`.

- [ ] **Step 1: Tester les politiques**

```text
system animations true  -> micro 100ms, structural 180ms
system animations false -> both 0ms
explicit reduced motion -> both 0ms
```

- [ ] **Step 2: Implémenter les préférences Windows**

`SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, ...)`. Réévaluer sur `WM_SETTINGCHANGE`.

- [ ] **Step 3: Centraliser les timings**

Aucun composant n’encode directement `100` ou `180` ms. Utiliser `AnimationPolicy` pour hover/status/palette/panneaux.

- [ ] **Step 4: Appliquer Glass avec parcimonie**

V1 : autoriser Glass uniquement sur surfaces flottantes ciblées : Command Palette et overlays/approvals si GPU disponible. Le Workbench principal et Tree gardent un fond mat/thématique. Un échec du pipeline D3D/Glass désactive l’effet et conserve toutes les fonctions.

- [ ] **Step 5: Éviter les timers à l’idle**

Aucun timer animation ne reste actif lorsqu’il n’existe aucune animation ou session running nécessitant un indicateur. Vérifier via debugger/perf que CPU idle revient proche de zéro.

- [ ] **Step 6: Commit**

```bash
git add src/theme src/ui src/navigation src/workbench src/rendering tests/ui
git commit -m "feat: polish Codex Deck motion and glass behavior"
```

---

### Task 6: Ajouter l’accessibilité de fondation et valider DPI/clavier

**Files:**
- Create: `src/accessibility/AccessibleNode.h`
- Create: `src/accessibility/AccessibilityTree.h`
- Create: `src/accessibility/AccessibilityTree.cpp`
- Create: `src/accessibility/UiaProvider.h`
- Create: `src/accessibility/UiaProvider.cpp`
- Test: `tests/accessibility/AccessibilityTreeTests.cpp`
- Modify: `src/app/DeckApp.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: arbre sémantique indépendant du rendu.
- V1 UIA couvre au minimum fenêtre principale, Tree, sessions visibles, Workbench actif, composer et boutons approval.

- [ ] **Step 1: Définir l’arbre sémantique**

```cpp
enum class AccessibleRole { Window, Tree, TreeItem, Document, Edit, Button, Status };

struct AccessibleNode {
    std::string id;
    AccessibleRole role;
    std::wstring name;
    D2D1_RECT_F bounds;
    bool focusable = false;
    bool focused = false;
    std::vector<AccessibleNode> children;
};
```

- [ ] **Step 2: Écrire le test de Tree accessible**

Un modèle visible avec un projet + deux sessions doit produire `Tree -> TreeItem(project) -> TreeItem(session...)` ou une structure plate cohérente documentée ; la session sélectionnée est focusable/focused et porte son titre.

- [ ] **Step 3: Implémenter UI Automation minimal**

Répondre à `WM_GETOBJECT` avec un provider racine implémentant `IRawElementProviderSimple`/fragment root et providers enfants générés depuis `AccessibilityTree`. Exposer Name, ControlType, BoundingRectangle, IsKeyboardFocusable/HasKeyboardFocus ; `Invoke` sur boutons approval et `SelectionItem` sur sessions visibles.

- [ ] **Step 4: Vérifier navigation clavier sans souris**

Parcours manuel : ouvrir app, tree Up/Down/Enter, Ctrl+K, fermer palette Esc, ouvrir session, Tab vers composer, envoyer, traiter approval, détacher fenêtre. Le focus visible doit toujours être dessiné.

- [ ] **Step 5: Vérifier DPI**

Tester Windows scaling 100 %, 125 %, 150 %, 200 % ; déplacement entre deux écrans de DPI différents. Aucune taille codée en pixels non DPI-scalée dans layout/rendu hors API système explicitement pixel-based.

- [ ] **Step 6: Commit**

```bash
git add src/accessibility src/app tests/accessibility CMakeLists.txt
git commit -m "feat: add Codex Deck accessibility foundation"
```

---

### Task 7: Ajouter benchmarks reproductibles et budgets de performance

**Files:**
- Create: `benchmarks/ModelBenchmarks.cpp`
- Create: `benchmarks/LayoutBenchmarks.cpp`
- Create: `benchmarks/README.md`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: exécutables benchmark sans framework externe.

- [ ] **Step 1: Ajouter une option CMake**

```cmake
option(CODEX_DECK_BUILD_BENCHMARKS "Build Codex Deck microbenchmarks" OFF)
```

- [ ] **Step 2: Benchmark modèle 500/10 000**

Construire 500 projets + 10 000 sessions, chronométrer `BuildProjectTreeRows` et une recherche fuzzy courante. Imprimer durée, résultat count, allocations si instrumentation debug disponible.

- [ ] **Step 3: Benchmark timeline 50 000**

Construire 50 000 hauteurs et exécuter 100 000 lookups d’offset aléatoires dans `VirtualTimelineLayout`. Vérifier fonctionnellement que chaque lookup renvoie un index valide ; afficher ns/op moyen.

- [ ] **Step 4: Documenter les budgets**

`benchmarks/README.md` :

```text
Objectif first visual: < 300 ms sur machine de développement de référence
Frame UI: < 16.7 ms à 60 Hz
Tree/search: perceptuellement instantané
Idle CPU: proche de 0 %
```

Préciser que ce sont des objectifs à observer, pas des asserts CI cross-machine.

- [ ] **Step 5: Mesurer Release**

```powershell
cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DCODEX_DECK_BUILD_BENCHMARKS=ON
cmake --build build-bench
build-bench\ModelBenchmarks.exe
build-bench\LayoutBenchmarks.exe
```

Enregistrer les résultats de référence datés dans `benchmarks/README.md` avec CPU/Windows/compilateur de la machine utilisée.

- [ ] **Step 6: Commit**

```bash
git add benchmarks CMakeLists.txt
git commit -m "perf: add Codex Deck native benchmarks"
```

---

### Task 8: Finaliser README, build portable et validation V1

**Files:**
- Modify: `README.md`
- Create: `doc/README.en.md`
- Create: `cmake/PackagePortable.cmake`
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Create: `docs/RELEASE_CHECKLIST.md`

**Interfaces:**
- Produces: archive `CodexDeck-<version>-win-x64.zip`.

- [ ] **Step 1: Compléter README français**

Inclure : vision Tree + Workbench, prérequis Windows 11 x64, dépendance à une installation/authentification Codex existante, build Debug/Release, emplacement DB/logs, confidentialité (organisation/cache seulement), raccourcis clavier, capture d’écran lorsque disponible.

- [ ] **Step 2: Ajouter README anglais synchronisé**

`doc/README.en.md` couvre les mêmes sections et limitations, sans promettre Windows 10.

- [ ] **Step 3: Créer packaging portable**

`PackagePortable.cmake` reçoit :

```text
CODEX_DECK_EXE
CODEX_DECK_VERSION
CODEX_DECK_OUTPUT_DIR
```

Il crée un staging propre, copie `CodexDeck.exe` et les fichiers runtime réellement nécessaires s’ils ne sont pas liés statiquement, puis :

```cmake
file(ARCHIVE_CREATE OUTPUT ".../CodexDeck-${CODEX_DECK_VERSION}-win-x64.zip" PATHS ... FORMAT zip)
```

Ne jamais embarquer DB utilisateur, settings locaux, logs ou binaire `codex`.

- [ ] **Step 4: Ajouter cible `package-portable`**

Après un build Release :

```powershell
cmake --build --preset release --target package-portable
```

Expected: ZIP sous `build/release/dist/`.

- [ ] **Step 5: Écrire la checklist de release**

`docs/RELEASE_CHECKLIST.md` contient cases :

```text
[ ] Debug configure/build/tests
[ ] Release configure/build/tests
[ ] connexion vrai app-server
[ ] liste > 100 threads pagination
[ ] création/reprise/renommage/archive
[ ] plusieurs sessions actives
[ ] approval commande et fichier
[ ] streaming long
[ ] diff review
[ ] drag session -> projet
[ ] Unassigned manuel conservé après resync
[ ] thème System/Light/Dark
[ ] 100/125/150/200 % DPI
[ ] fenêtre détachée + restauration
[ ] notification background + click
[ ] app-server crash/reconnect
[ ] lancement sans app-server disponible
[ ] Reduced Motion
[ ] keyboard-only smoke
[ ] package portable sur dossier vierge
```

- [ ] **Step 6: Exécuter la validation finale**

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure

cmake --build --preset release --target package-portable
```

Puis exécuter intégralement `docs/RELEASE_CHECKLIST.md` et cocher uniquement les étapes réellement validées.

- [ ] **Step 7: Commit final V1-ready**

```bash
git add README.md doc cmake CMakeLists.txt CMakePresets.json docs/RELEASE_CHECKLIST.md
git commit -m "chore: prepare Codex Deck V1 portable release"
```

## Critère de sortie du plan

Codex Deck restaure son espace de travail, supporte plusieurs fenêtres Workbench, notifie seulement quand utile, respecte Reduced Motion, possède un socle UI Automation minimal, dispose de diagnostics respectueux du contenu et de benchmarks reproductibles. Une archive portable Release est produite et la checklist V1 est entièrement vérifiée avant publication.

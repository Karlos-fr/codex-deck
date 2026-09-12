# Codex Deck Tree & Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Construire la navigation principale Tree + barre d’activité + recherche/Command Palette avec virtualisation, clavier et drag & drop, sans transformer l’UI en clone de sidebar ChatGPT.

**Architecture:** Le Tree est alimenté par un modèle aplati indépendant du rendu. `VirtualListLayout` calcule uniquement les lignes visibles ; `ProjectTreeView` dessine via Direct2D et traduit pointeur/clavier en commandes applicatives. La recherche et la Command Palette partagent un matcher fuzzy mais possèdent des modèles distincts.

**Tech Stack:** C++23, Direct2D/DirectWrite via `CompositionHost`, Win32 input, modèle immutable issu de `SessionCatalogSnapshot`, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- Projets et sessions fusionnés dans **un seul arbre**.
- Pas de sous-dossiers de sessions en V1.
- `Unassigned` est une section fixe.
- Sessions triées par activité décroissante ; projets triés par leur session la plus récente.
- Un événement de streaming ne réordonne pas l’arbre.
- Maximum initial affiché : **8 sessions récentes par projet** ; au-delà, ligne `… N more`.
- La liste doit rester virtualisée avec 10 000 sessions synthétiques.
- Pas de cartes massives ; densité et lisibilité priment.

---

### Task 1: Construire le modèle aplati du Tree

**Files:**
- Create: `src/navigation/ProjectTreeModel.h`
- Create: `src/navigation/ProjectTreeModel.cpp`
- Test: `tests/navigation/ProjectTreeModelTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `TreeRow`, `ProjectTreeState`, `BuildProjectTreeRows(...)`.

- [ ] **Step 1: Définir les lignes**

```cpp
enum class TreeRowKind { Project, Session, More, UnassignedHeader, ArchiveEntry };

struct TreeRow {
    TreeRowKind kind;
    std::string stable_id;
    int depth = 0;
    std::wstring primary_text;
    std::wstring secondary_text;
    SessionStatus status = SessionStatus::Idle;
    std::optional<ProjectId> project_id;
    std::optional<CodexThreadId> thread_id;
    std::size_t hidden_count = 0;
};

struct ProjectTreeState {
    std::unordered_set<ProjectId> expanded_projects;
    bool unassigned_expanded = true;
    std::optional<CodexThreadId> selected_thread;
};
```

- [ ] **Step 2: Écrire les tests de structure et tri**

Cas exact : deux projets, 10 sessions dans le premier, 1 dans le second, 2 Unassigned. Vérifier projet le plus récent en premier, 8 sessions + `More(hidden_count=2)`, puis second projet, puis section Unassigned fixe.

- [ ] **Step 3: Vérifier l’échec**

```powershell
cmake --build --preset debug
ctest --preset debug -R ProjectTreeModelTests --output-on-failure
```

- [ ] **Step 4: Implémenter `BuildProjectTreeRows`**

Signature :

```cpp
std::vector<TreeRow> BuildProjectTreeRows(
    const SessionCatalogSnapshot& catalog,
    const ProjectTreeState& state,
    std::size_t max_sessions_per_project = 8
);
```

Les IDs stables utilisent `project:<id>`, `thread:<threadId>`, `more:<projectId>`, `unassigned`.

- [ ] **Step 5: Valider et commit**

```powershell
ctest --preset debug -R ProjectTreeModelTests --output-on-failure
git add src/navigation tests/navigation CMakeLists.txt
git commit -m "feat: build project and session tree model"
```

---

### Task 2: Créer une liste virtualisée générique à géométrie fixe

**Files:**
- Create: `src/ui/VirtualListLayout.h`
- Create: `src/ui/VirtualListLayout.cpp`
- Create: `src/ui/ScrollState.h`
- Create: `src/ui/ScrollState.cpp`
- Test: `tests/ui/VirtualListLayoutTests.cpp`

**Interfaces:**
- Produces: `VisibleRange ComputeVisibleRange(...)`.
- Produces: `ScrollState::ScrollBy`, `EnsureVisible`, `Clamp`.

- [ ] **Step 1: Écrire les tests sur 10 000 lignes**

```cpp
const auto range = ComputeVisibleRange(10'000, 30.0f, 12'000.0f, 600.0f, 2);
if (range.first >= range.last) return 1;
if ((range.last - range.first) > 25) return 2;
```

Tester haut, milieu, bas, viewport vide et overscan.

- [ ] **Step 2: Implémenter le calcul O(1)**

```cpp
struct VisibleRange { std::size_t first; std::size_t last; };
VisibleRange ComputeVisibleRange(
    std::size_t item_count,
    float row_height,
    float scroll_offset,
    float viewport_height,
    std::size_t overscan_rows
);
```

Ne jamais itérer sur `item_count` pour calculer la plage visible.

- [ ] **Step 3: Implémenter `ScrollState`**

Stocker `offset`, `content_extent`, `viewport_extent`. `Clamp()` borne entre `0` et `max(0, content - viewport)`. `EnsureVisible(index,rowHeight)` ne déplace que si la ligne est hors viewport.

- [ ] **Step 4: Valider et commit**

```bash
git add src/ui tests/ui
git commit -m "feat: add virtual list and scroll primitives"
```

---

### Task 3: Rendre et interagir avec le Project Tree

**Files:**
- Create: `src/navigation/ProjectTreeView.h`
- Create: `src/navigation/ProjectTreeView.cpp`
- Create: `src/navigation/TreeHitTesting.h`
- Create: `src/navigation/TreeHitTesting.cpp`
- Test: `tests/navigation/TreeHitTestingTests.cpp`
- Modify: `src/rendering/DeckRenderer.h`
- Modify: `src/rendering/DeckRenderer.cpp`

**Interfaces:**
- Produces: `ProjectTreeView::Render(...)`.
- Produces: `HitTestTreeRow(point, rect, scroll, row_height, count) -> optional<size_t>`.
- Produces: callbacks `OnSelectThread`, `OnToggleProject`, `OnOpenMore`.

- [ ] **Step 1: Tester le hit testing**

Avec tree rect `(0,0,300,600)`, row height `30`, scroll `60`, un point y=15 doit correspondre à la ligne logique 2. Un point hors rect renvoie `nullopt`.

- [ ] **Step 2: Implémenter le rendu visible uniquement**

`ProjectTreeView::Render` :

```cpp
void Render(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& bounds,
    std::span<const TreeRow> rows,
    const ScrollState& scroll,
    const ThemePalette& palette
);
```

Utiliser `ComputeVisibleRange` avec overscan 2. Dessiner indentation, chevron projet, point/statut session, titre, âge secondaire et sélection. Les lignes non visibles ne créent aucun `IDWriteTextLayout`.

- [ ] **Step 3: Ajouter cache léger de layouts texte**

Cache LRU borné à 256 entrées par `(stable_id, width, theme_revision, text_revision)`. Invalider au changement de DPI/thème/texte.

- [ ] **Step 4: Brancher souris, wheel et clavier**

Supporter clic, double-clic session, molette, Up/Down, Left/Right pour collapse/expand, Enter pour ouvrir, Home/End.

- [ ] **Step 5: Valider manuellement avec données synthétiques**

Ajouter un mode de debug interne qui injecte 500 projets/10 000 sessions dans le modèle sans toucher SQLite. Scroller rapidement : pas de freeze perceptible.

- [ ] **Step 6: Commit**

```bash
git add src/navigation src/ui src/rendering tests
git commit -m "feat: render virtualized project tree"
```

---

### Task 4: Ajouter la barre d’activité et les filtres globaux

**Files:**
- Create: `src/navigation/ActivityBarModel.h`
- Create: `src/navigation/ActivityBarModel.cpp`
- Create: `src/navigation/ActivityBarView.h`
- Create: `src/navigation/ActivityBarView.cpp`
- Test: `tests/navigation/ActivityBarModelTests.cpp`

**Interfaces:**
- Produces: `ActivityCounts BuildActivityCounts(span<SessionRecord>)`.
- Produces: `enum class SessionFilter { All, Working, NeedsAttention, CompletedToday, Favorites, Archived };`.

- [ ] **Step 1: Tester les compteurs**

Construire 6 sessions avec états variés et vérifier exactement `working`, `needs_attention`, `completed_today`.

- [ ] **Step 2: Implémenter modèle et filtre**

`Favorites` s’appuie sur métadonnée locale. `Archived` s’appuie sur les threads archivés retournés/lus par Codex ; ne pas marquer localement une conversation comme archivée sans succès `thread/archive`.

- [ ] **Step 3: Rendre la barre**

Disposition compacte en haut :

```text
● 3 working   ◐ 1 attention   ✓ 4 completed today   Ctrl+K
```

Cliquer sur un compteur active/désactive le filtre correspondant puis reconstruit les `TreeRow` sans modifier le catalogue source.

- [ ] **Step 4: Commit**

```bash
git add src/navigation tests/navigation
git commit -m "feat: add live activity filters"
```

---

### Task 5: Ajouter recherche fuzzy et Command Palette

**Files:**
- Create: `src/search/FuzzyMatcher.h`
- Create: `src/search/FuzzyMatcher.cpp`
- Create: `src/navigation/CommandPaletteModel.h`
- Create: `src/navigation/CommandPaletteModel.cpp`
- Create: `src/navigation/CommandPaletteView.h`
- Create: `src/navigation/CommandPaletteView.cpp`
- Test: `tests/search/FuzzyMatcherTests.cpp`
- Test: `tests/navigation/CommandPaletteModelTests.cpp`

**Interfaces:**
- Produces: `FuzzyMatch(query, candidate) -> optional<FuzzyScore>`.
- Produces: `PaletteEntry {kind,id,title,subtitle,score,command}`.

- [ ] **Step 1: Écrire les tests de scoring**

`"spa aud"` doit classer `SpotifyAmp / Audio parity` devant `OpenRemi / Audio notes`. Les correspondances préfixes de mot et séquences contiguës reçoivent un bonus ; les trous longs une pénalité.

- [ ] **Step 2: Implémenter matcher sans dépendance**

Algorithme déterministe sur chaînes Unicode déjà normalisées en minuscules ; pas de bibliothèque fuzzy tierce.

- [ ] **Step 3: Définir les commandes applicatives**

Créer `src/app/DeckCommand.h` :

```cpp
enum class DeckCommandKind {
    OpenThread,
    NewSession,
    NewSessionInCurrentProject,
    RenameThread,
    ArchiveThread,
    ToggleFavorite,
    OpenWorkspace,
    DetachWorkbench,
};

struct DeckCommand {
    DeckCommandKind kind;
    std::optional<ProjectId> project_id;
    std::optional<CodexThreadId> thread_id;
};
```

- [ ] **Step 4: Construire les entrées palette**

Entrées groupées `SESSIONS`, `PROJECTS`, `ACTIONS`; maximum 50 résultats affichés, mais score calculé sur tout le snapshot sur worker si le volume dépasse 2 000 entrées.

- [ ] **Step 5: Rendre l’overlay via DirectComposition**

La palette est une surface/visual indépendante avec apparition 150 ms maximum, input focus capturé, Esc pour fermer, Up/Down/Enter pour sélectionner.

- [ ] **Step 6: Commit**

```bash
git add src/search src/navigation src/app tests
git commit -m "feat: add Codex Deck command palette"
```

---

### Task 6: Raccourcis, renommage, archive et drag & drop session → projet

**Files:**
- Create: `src/input/KeyboardShortcuts.h`
- Create: `src/input/KeyboardShortcuts.cpp`
- Create: `src/navigation/TreeDragController.h`
- Create: `src/navigation/TreeDragController.cpp`
- Modify: `src/app/DeckApp.cpp`
- Modify: `src/navigation/ProjectTreeView.cpp`
- Test: `tests/input/KeyboardShortcutsTests.cpp`
- Test: `tests/navigation/TreeDragControllerTests.cpp`

**Interfaces:**
- Produces: `TranslateShortcut(const KeyChord&) -> optional<DeckCommandKind>`.
- Produces: `TreeDragController` qui ne modifie jamais directement SQLite.

- [ ] **Step 1: Tester les raccourcis**

Mapping exact :

```text
Ctrl+K       Command Palette
Ctrl+P       recherche
Ctrl+N       NewSession
Ctrl+Shift+N NewSessionInCurrentProject
F2           RenameThread
Delete       ArchiveThread
Ctrl+Shift+D DetachWorkbench
```

`Ctrl+1..9` et `Ctrl+Tab` sont réservés à la navigation des sessions runtime actives et routés via commandes applicatives.

- [ ] **Step 2: Implémenter le renommage optimiste**

Sur F2, éditer inline le titre. À validation : publier titre optimiste, appeler `CodexClient::SetThreadName`; succès conserve, échec restaure l’ancien titre et publie une erreur non modale.

- [ ] **Step 3: Implémenter ArchiveThread**

Après `thread/archive` réussi, retirer la session de la vue normale et rafraîchir les métadonnées. En cas d’échec, aucun changement persistant de statut archive local.

- [ ] **Step 4: Implémenter drag interne**

À partir d’un mouvement > 6 DIPs sur une ligne Session, `TreeDragController` conserve le `thread_id`. Sur survol d’une ligne Project ou Unassigned, afficher la cible. Au drop appeler `ProjectAssignmentService::AssignManual`.

Ne pas utiliser OLE pour ce drag interne ; OLE sera réservé au dépôt de fichiers externes dans le Workbench.

- [ ] **Step 5: Commit**

```bash
git add src/input src/navigation src/app tests
git commit -m "feat: add keyboard and tree session actions"
```

---

### Task 7: Intégrer le layout principal Tree + Workbench placeholder

**Files:**
- Create: `src/ui/MainLayout.h`
- Create: `src/ui/MainLayout.cpp`
- Modify: `src/rendering/DeckRenderer.cpp`
- Modify: `src/app/DeckApp.cpp`
- Test: `tests/ui/MainLayoutTests.cpp`

**Interfaces:**
- Produces: `MainLayoutRects ComputeMainLayout(SizeF client, float tree_width, float activity_height, float composer_height)`.

- [ ] **Step 1: Tester le layout**

À `1440x900`, tree width `300`, activity bar `42`, vérifier zéro chevauchement et workbench width `1140`.

- [ ] **Step 2: Implémenter séparateur redimensionnable**

Tree width bornée `[220, 480]` DIPs. Le splitter fait 4 DIPs avec zone de hit 8 DIPs. Persistante seulement dans le plan de hardening ; pour l’instant état mémoire.

- [ ] **Step 3: Rendre la structure finale de navigation**

La zone droite affiche un Workbench placeholder indiquant session sélectionnée ou `Select a session`. Le vrai Workbench arrive au plan suivant.

- [ ] **Step 4: Validation complète**

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Tester clavier seul, recherche, palette, rename, archive, drag vers projet, 10 000 sessions synthétiques.

- [ ] **Step 5: Commit**

```bash
git add src/ui src/navigation src/rendering src/app tests
git commit -m "feat: complete Codex Deck tree navigation"
```

## Critère de sortie du plan

Codex Deck possède son identité de navigation : Tree dense projets/sessions + activité globale + palette. Les projets et sessions restent rapides sur de gros volumes, les commandes courantes fonctionnent au clavier, et aucune conversation n’est encore rendue comme une messagerie classique.

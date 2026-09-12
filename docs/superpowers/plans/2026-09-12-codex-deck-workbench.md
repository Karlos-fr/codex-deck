# Codex Deck Workbench Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transformer la zone droite de Codex Deck en vrai Workbench Codex : timeline native virtualisée, streaming, commandes/outils, approbations, fichiers/diffs, Markdown et composer capable d’envoyer texte et pièces jointes supportées par `app-server`.

**Architecture:** Le Workbench ne rend jamais directement le JSON brut. `TimelineReducer` transforme l’historique `thread/read` et les notifications live en `TimelineItem` stables ; `VirtualTimelineLayout` virtualise des hauteurs variables ; des renderers spécialisés dessinent les blocs. Le composer s’appuie sur un contrôle RichEdit Win32 sous-classé pour bénéficier d’un éditeur texte natif robuste sans introduire de navigateur ou framework UI.

**Tech Stack:** C++23, Win32, Direct2D/DirectWrite, DirectComposition, RichEdit (`Msftedit.dll`), OLE drag & drop, Codex app-server V2, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- Aucune bulle gauche/droite de messagerie ; une timeline de travail unique.
- Les types visuels V1 sont `YOU`, `CODEX`, `COMMAND`, `TOOL`, `FILES`, `DIFF`, `TESTS`, `APPROVAL`, `ERROR`.
- Les commandes et sorties volumineuses sont repliables.
- La timeline doit rester fluide avec **50 000 événements synthétiques**.
- Le streaming modifie un item existant ; il ne crée pas un item par delta.
- Les invalidations visuelles de streaming sont regroupées à une cadence maximale de 60 Hz.
- Pas de moteur HTML/WebView.
- Les images locales sont envoyées à `turn/start` comme `{type:"localImage", path:...}` ; l’audio local comme `{type:"localAudio", path:...}`. Les autres fichiers déposés deviennent des `mention` uniquement lorsqu’ils sont dans le workspace courant ; sinon ils restent refusés avec un message non modal explicite.
- L’historique complet reste chez Codex ; le Workbench conserve uniquement son modèle runtime en mémoire.

---

### Task 1: Définir le modèle de timeline et convertir `thread/read`

**Files:**
- Create: `src/workbench/TimelineTypes.h`
- Create: `src/workbench/ThreadHistoryMapper.h`
- Create: `src/workbench/ThreadHistoryMapper.cpp`
- Test: `tests/workbench/ThreadHistoryMapperTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `TimelineItem`, `TimelineDocument`, `MapThreadHistory(const json&)`.
- Consumes: réponse brute validée de `CodexClient::ReadThread` avec `turns/items`.

- [ ] **Step 1: Définir les types stables**

`TimelineTypes.h` :

```cpp
enum class TimelineItemKind {
    UserMessage,
    AgentMessage,
    Command,
    Tool,
    Files,
    Tests,
    Approval,
    Error,
};

enum class TimelineItemState { Pending, Running, Completed, Failed };

struct TimelineFileChange {
    std::filesystem::path path;
    std::string kind;
    std::string patch;
};

struct TimelineItem {
    std::string id;
    TimelineItemKind kind;
    TimelineItemState state = TimelineItemState::Completed;
    std::int64_t timestamp = 0;
    std::wstring title;
    std::wstring summary;
    std::wstring body;
    std::wstring secondary;
    std::vector<TimelineFileChange> files;
    bool collapsible = false;
    bool expanded = false;
};

struct TimelineDocument {
    CodexThreadId thread_id;
    std::vector<TimelineItem> items;
    std::uint64_t revision = 0;
};
```

- [ ] **Step 2: Écrire une fixture représentative d’historique Codex**

Le test contient un `userMessage`, un `agentMessage`, un `commandExecution`, un `fileChange` et un `mcpToolCall`, avec des champs inconnus additionnels. Vérifier l’ordre et la conversion des cinq items.

Exemple command fixture :

```json
{
  "type":"commandExecution",
  "id":"cmd_1",
  "command":"cmake --build build",
  "cwd":"D:\\VibeCoding\\spotifyamp",
  "status":"completed",
  "commandActions":[],
  "aggregatedOutput":"Build succeeded\n",
  "exitCode":0,
  "durationMs":4200
}
```

- [ ] **Step 3: Vérifier l’échec**

```powershell
cmake --build --preset debug
ctest --preset debug -R ThreadHistoryMapperTests --output-on-failure
```

- [ ] **Step 4: Implémenter le mapping des items V2**

Règles minimales :

```text
userMessage      -> UserMessage / titre YOU
agentMessage     -> AgentMessage / titre CODEX
commandExecution -> Command / commande dans summary, aggregatedOutput dans body
fileChange       -> Files / liste de TimelineFileChange
mcpToolCall      -> Tool / server + tool + résultat/erreur
functionCallOutput/dynamicToolCall -> Tool
```

Les variantes non reconnues sont ignorées par le mapper V1 mais consignées via le logger de diagnostic ; ne jamais faire échouer l’ouverture d’une session entière pour un nouveau type d’item.

- [ ] **Step 5: Valider et commit**

```powershell
ctest --preset debug -R ThreadHistoryMapperTests --output-on-failure
git add src/workbench tests/workbench CMakeLists.txt
git commit -m "feat: map Codex thread history to workbench timeline"
```

---

### Task 2: Réduire les notifications live et agréger le streaming

**Files:**
- Create: `src/workbench/TimelineReducer.h`
- Create: `src/workbench/TimelineReducer.cpp`
- Create: `src/workbench/StreamingInvalidationCoalescer.h`
- Create: `src/workbench/StreamingInvalidationCoalescer.cpp`
- Test: `tests/workbench/TimelineReducerTests.cpp`
- Test: `tests/workbench/StreamingInvalidationTests.cpp`

**Interfaces:**
- Produces: `TimelineReducer::Apply(CodexTimelineEvent)`.
- Produces: callback `OnTimelineChanged(thread_id, revision)` au maximum une fois par frame pour les deltas.

- [ ] **Step 1: Écrire les transitions live**

Fixtures à couvrir exactement :

```text
item/started
item/completed
item/agentMessage/delta
item/commandExecution/outputDelta
item/fileChange/outputDelta
item/fileChange/patchUpdated
turn/diff/updated
turn/completed
error
```

Cas agent : `item/started` crée `CODEX` vide/running ; trois `item/agentMessage/delta` concatènent dans **le même item id** ; `item/completed` remplace le contenu par la version autoritative si fournie.

Cas commande : output deltas concaténés dans `body`, puis `item/completed` fixe exit code/durée/status.

- [ ] **Step 2: Vérifier l’échec**

```powershell
ctest --preset debug -R TimelineReducerTests --output-on-failure
```

- [ ] **Step 3: Implémenter le reducer par `item.id`**

Conserver un index runtime :

```cpp
std::unordered_map<std::string, std::size_t> item_index_by_id_;
```

L’ordre d’arrivée reste l’ordre visuel. `item/completed` ne déplace jamais l’item ; il met à jour sa position existante.

- [ ] **Step 4: Ajouter l’agrégation d’invalidation**

`StreamingInvalidationCoalescer` reçoit autant de `Request()` que nécessaire, mais poste au maximum un message Win32 privé `WM_APP + 40` par fenêtre tant que le précédent n’a pas été consommé. Le handler rend le dernier snapshot, pas chaque delta.

- [ ] **Step 5: Tester 10 000 deltas**

Injecter 10 000 deltas dans le même agent message. Vérifier : un seul TimelineItem, texte final complet, nombre de notifications UI strictement inférieur au nombre de deltas et borné par les ticks simulés du test.

- [ ] **Step 6: Commit**

```bash
git add src/workbench tests/workbench
git commit -m "feat: reduce live Codex events into timeline state"
```

---

### Task 3: Virtualiser une timeline à hauteurs variables

**Files:**
- Create: `src/workbench/VirtualTimelineLayout.h`
- Create: `src/workbench/VirtualTimelineLayout.cpp`
- Create: `src/workbench/TimelineMeasurementCache.h`
- Create: `src/workbench/TimelineMeasurementCache.cpp`
- Test: `tests/workbench/VirtualTimelineLayoutTests.cpp`

**Interfaces:**
- Produces: `TimelineVisibleRange` calculé par recherche binaire sur préfixes de hauteur.
- Produces: `TimelineMeasurementCache::SetMeasuredHeight(item_id,width,revision,height)`.

- [ ] **Step 1: Écrire le test 50 000 items**

Créer 50 000 hauteurs alternant `48`, `96`, `180` DIPs. À un scroll au milieu, vérifier que le calcul visible renvoie moins de 40 éléments et que l’item de départ est trouvé sans scan linéaire.

- [ ] **Step 2: Définir les données de layout**

```cpp
struct TimelineLayoutEntry {
    std::string item_id;
    float height = 72.0f;
    float start_y = 0.0f;
};

struct TimelineVisibleRange {
    std::size_t first = 0;
    std::size_t last = 0;
};
```

Le layout maintient un tableau de hauteurs et un Fenwick tree (Binary Indexed Tree) pour mettre à jour une hauteur en `O(log n)`, obtenir un préfixe en `O(log n)` et trouver l’index d’un offset en `O(log n)`.

- [ ] **Step 3: Implémenter le cache de mesure**

Clé : `(item_id, quantized_width, content_revision, theme_revision, dpi)`. Maximum 1 024 mesures ; LRU. Une largeur est quantifiée au DIP entier pour éviter les misses pendant un resize sub-pixel.

- [ ] **Step 4: Ajouter le comportement auto-follow**

`WorkbenchScrollState` expose :

```cpp
bool IsNearBottom(float threshold = 96.0f) const;
void OnContentGrowth(float old_extent, float new_extent);
```

Si le scroll était à moins de 96 DIPs du bas, rester collé au bas. Sinon garder l’offset et afficher plus tard un bouton `Jump to latest`.

- [ ] **Step 5: Valider et commit**

```powershell
ctest --preset debug -R VirtualTimelineLayoutTests --output-on-failure
git add src/workbench tests/workbench
git commit -m "feat: virtualize variable-height workbench timeline"
```

---

### Task 4: Implémenter le Markdown natif essentiel

**Files:**
- Create: `src/markdown/MarkdownDocument.h`
- Create: `src/markdown/MarkdownParser.h`
- Create: `src/markdown/MarkdownParser.cpp`
- Create: `src/markdown/MarkdownLayout.h`
- Create: `src/markdown/MarkdownLayout.cpp`
- Create: `src/markdown/MarkdownRenderer.h`
- Create: `src/markdown/MarkdownRenderer.cpp`
- Test: `tests/markdown/MarkdownParserTests.cpp`
- Test: `tests/markdown/MarkdownLayoutTests.cpp`

**Interfaces:**
- Produces: AST compact pour paragraphes, titres, listes, code, table, lien, citation.
- Consumes: `IDWriteFactory`, `ID2D1DeviceContext`, `ThemePalette`.

- [ ] **Step 1: Définir le sous-ensemble AST**

```cpp
enum class MarkdownBlockKind { Paragraph, Heading, UnorderedList, OrderedList, CodeBlock, Table, Quote };
enum class MarkdownSpanKind { Text, Strong, Emphasis, InlineCode, Link };

struct MarkdownSpan {
    MarkdownSpanKind kind;
    std::wstring text;
    std::wstring target;
};

struct MarkdownBlock {
    MarkdownBlockKind kind;
    int level = 0;
    std::vector<MarkdownSpan> spans;
    std::vector<std::vector<MarkdownSpan>> rows;
    std::wstring code_language;
};
```

- [ ] **Step 2: Écrire un golden test de parsing**

Entrée :

```markdown
## Build

**Done** with `cmake`.

- one
- two

> note

```cpp
int x = 1;
```

| File | State |
|---|---|
| a.cpp | M |
```

Vérifier les blocs et spans attendus. Les tags HTML restent du texte brut, jamais interprétés.

- [ ] **Step 3: Implémenter un parser ligne-par-ligne sans dépendance externe**

Ordre de reconnaissance : fence code, table, heading, quote, list, paragraph. L’inline reconnaît `**strong**`, `*emphasis*`, backticks et `[label](target)` sans exécuter de HTML.

- [ ] **Step 4: Implémenter le layout DirectWrite**

Le layout retourne une hauteur mesurée et des commandes de dessin. Les blocs de code utilisent `Cascadia Mono` avec repli `Consolas`. Le texte normal utilise `Segoe UI Variable` avec repli `Segoe UI`.

Les tableaux ont colonnes calculées à partir des largeurs mesurées, bornées au viewport et wrap des cellules longues.

- [ ] **Step 5: Implémenter liens et sélection minimale**

Le renderer expose des rectangles de liens hit-testables. Un clic appelle `ShellExecuteW` seulement pour `http`/`https`; toute autre URI est rendue comme texte non cliquable en V1.

La sélection/copie de réponse est ajoutée par item via un `TextSelectionModel` qui copie le texte source Markdown/plaintext, pas les commandes de dessin.

- [ ] **Step 6: Valider et commit**

```powershell
ctest --preset debug -R "MarkdownParserTests|MarkdownLayoutTests" --output-on-failure
git add src/markdown tests/markdown
git commit -m "feat: render native Markdown in Codex Deck"
```

---

### Task 5: Rendre les blocs Workbench et les commandes/outils

**Files:**
- Create: `src/workbench/TimelineBlockRenderer.h`
- Create: `src/workbench/TimelineBlockRenderer.cpp`
- Create: `src/workbench/WorkbenchView.h`
- Create: `src/workbench/WorkbenchView.cpp`
- Create: `src/workbench/TimelineInteraction.h`
- Create: `src/workbench/TimelineInteraction.cpp`
- Test: `tests/workbench/TimelineInteractionTests.cpp`
- Modify: `src/rendering/DeckRenderer.cpp`

**Interfaces:**
- Produces: `WorkbenchView::Render(...)` et hit regions typées.
- Consumes: `TimelineDocument`, `VirtualTimelineLayout`, `MarkdownRenderer`.

- [ ] **Step 1: Définir les hit regions**

```cpp
enum class TimelineActionKind { ToggleExpanded, ReviewChanges, ResolveApproval, OpenLink, JumpLatest };
struct TimelineHitRegion {
    D2D1_RECT_F rect;
    TimelineActionKind action;
    std::string item_id;
    std::string argument;
};
```

- [ ] **Step 2: Écrire un test d’interaction repli/dépli**

Un item Command replié expose exactement un hit region `ToggleExpanded`; après activation, sa hauteur cache est invalidée et le body complet devient mesurable.

- [ ] **Step 3: Implémenter le style des catégories**

Règles visuelles :

```text
YOU      label discret, texte principal
CODEX    label discret + Markdown
COMMAND  bandeau compact, monospace, état/durée, output repliable
TOOL     serveur/outil + statut + résultat repliable
FILES    liste path + type, bouton Review changes
APPROVAL surface warning plus contrastée et actions explicites
ERROR    accent error + détail repliable
```

Pas de fond arrondi massif pour YOU/CODEX. Les blocs techniques peuvent avoir une surface légère pour séparer leur contenu.

- [ ] **Step 4: Afficher uniquement les items visibles**

`WorkbenchView` demande la plage à `VirtualTimelineLayout`, mesure les nouveaux items visibles, met à jour les hauteurs puis effectue au maximum une seconde passe de layout si une mesure a changé pendant le frame.

- [ ] **Step 5: Ajouter l’en-tête de session**

Afficher `project › title`, statut, cwd raccourci, modèle/effort si disponibles dans le thread runtime. Le header reste fixe au-dessus de la timeline.

- [ ] **Step 6: Valider 50 000 items**

Mode synthétique : 50 000 items mixtes. Vérifier que le nombre d’items rendus pour un viewport 900 DIPs reste inférieur à 50 et que le scroll ne déclenche pas de création de ressources pour tous les items.

- [ ] **Step 7: Commit**

```bash
git add src/workbench src/rendering tests/workbench
git commit -m "feat: render Codex workbench timeline blocks"
```

---

### Task 6: Intégrer les approbations dans la timeline

**Files:**
- Create: `src/workbench/ApprovalPresenter.h`
- Create: `src/workbench/ApprovalPresenter.cpp`
- Modify: `src/workbench/TimelineReducer.cpp`
- Modify: `src/workbench/WorkbenchView.cpp`
- Test: `tests/workbench/ApprovalPresenterTests.cpp`

**Interfaces:**
- Produces: `ApprovalPresentation BuildApprovalPresentation(const PendingApproval&)`.
- Consumes: `CodexEventRouter::ResolveApproval` du plan app-server.

- [ ] **Step 1: Écrire les tests de présentation**

Command approval : title `Needs your approval`, commande et cwd visibles, décisions wire exactes. File change approval : liste de fichiers et raison.

- [ ] **Step 2: Construire les libellés UI sans altérer les wire decisions**

Mapping d’affichage :

```text
accept            -> Allow
acceptForSession  -> Always allow for this session
 decline           -> Refuse
cancel             -> Refuse and stop turn
```

Toute décision inconnue reçue dans `availableDecisions` est affichée avec son identifiant wire comme fallback et renvoyée telle quelle si choisie.

- [ ] **Step 3: Injecter un TimelineItem Approval**

À server request d’approbation : créer/upsert `approval:<requestId>`, status Pending. Lors de `serverRequest/resolved` ou réponse utilisateur réussie : status Completed et bloc repliable en résumé.

- [ ] **Step 4: Résoudre depuis le clic**

Le clic ne bloque pas l’UI. Désactiver visuellement les boutons dès l’envoi ; si `SendResponse` échoue, les réactiver et afficher l’erreur dans le bloc.

- [ ] **Step 5: Commit**

```bash
git add src/workbench tests/workbench
git commit -m "feat: handle Codex approvals inside the workbench"
```

---

### Task 7: Ajouter la revue native des fichiers et diffs

**Files:**
- Create: `src/diff/DiffTypes.h`
- Create: `src/diff/UnifiedDiffParser.h`
- Create: `src/diff/UnifiedDiffParser.cpp`
- Create: `src/diff/DiffReviewModel.h`
- Create: `src/diff/DiffReviewModel.cpp`
- Create: `src/diff/DiffReviewView.h`
- Create: `src/diff/DiffReviewView.cpp`
- Test: `tests/diff/UnifiedDiffParserTests.cpp`
- Test: `tests/diff/DiffReviewModelTests.cpp`
- Modify: `src/workbench/WorkbenchView.cpp`

**Interfaces:**
- Produces: `ParseUnifiedDiff(string_view) -> expected<DiffDocument, DiffParseError>`.
- Produces: Workbench mode `Timeline | DiffReview`.

- [ ] **Step 1: Écrire une fixture unified diff**

```diff
--- a/src/audio.cpp
+++ b/src/audio.cpp
@@ -10,2 +10,3 @@
-old
+new
+extra
 keep
```

Vérifier path, hunk, numéros gauche/droite, line kinds context/removal/addition.

- [ ] **Step 2: Implémenter le parser strict sur structure, tolérant sur texte**

Un diff malformé renvoie une erreur locale au fichier concerné ; il ne ferme pas le Workbench.

- [ ] **Step 3: Construire le modèle de revue depuis Codex**

Priorité des sources en mémoire :

```text
1. `turn/diff/updated` le plus récent
2. `item/fileChange/patchUpdated`
3. patch contenu dans le `fileChange` historique
```

Ne jamais appeler `git diff` pour reconstruire silencieusement ce que Codex n’a pas fourni ; la V1 affiche `Diff unavailable` si aucune source Codex n’est disponible.

- [ ] **Step 4: Rendre la vue temporaire**

Layout : liste des fichiers modifiés à gauche (180–260 DIPs), diff principal à droite. Rendu unified diff natif avec numéros de lignes, ajout/retrait/context et virtualisation par lignes. `Esc` retourne à la timeline et restaure son scroll.

- [ ] **Step 5: Commit**

```bash
git add src/diff src/workbench tests/diff
git commit -m "feat: add native diff review to workbench"
```

---

### Task 8: Construire le composer natif avec RichEdit

**Files:**
- Create: `src/composer/PromptEditor.h`
- Create: `src/composer/PromptEditor.cpp`
- Create: `src/composer/ComposerModel.h`
- Create: `src/composer/ComposerModel.cpp`
- Create: `src/composer/AttachmentClassifier.h`
- Create: `src/composer/AttachmentClassifier.cpp`
- Test: `tests/composer/ComposerModelTests.cpp`
- Test: `tests/composer/AttachmentClassifierTests.cpp`
- Modify: `src/ui/MainLayout.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `PromptEditor` wrapper autour de `MSFTEDIT_CLASS`.
- Produces: `ComposerSubmission {text, inputs}` prêt pour `CodexClient::StartTurn`.

- [ ] **Step 1: Tester la classification des pièces jointes**

Règles exactes :

```text
.png .jpg .jpeg .webp .gif -> localImage
.wav .mp3 .m4a .flac       -> localAudio
autre fichier sous cwd     -> mention {name: filename, path: absolute path}
autre fichier hors cwd     -> UnsupportedOutsideWorkspace
```

Comparaison `sous cwd` via normalisation Windows du plan projets.

- [ ] **Step 2: Étendre `CodexClient::StartTurn` pour des inputs structurés**

Remplacer `StartTurnOptions::prompt` par :

```cpp
struct CodexUserInput {
    enum class Kind { Text, LocalImage, LocalAudio, Mention };
    Kind kind;
    std::string text_or_name;
    std::filesystem::path path;
};

struct StartTurnOptions {
    CodexThreadId thread_id;
    std::vector<CodexUserInput> input;
};
```

Wire encoding :

```json
{"type":"text","text":"...","textElements":[]}
{"type":"localImage","path":"C:\\...\\image.png"}
{"type":"localAudio","path":"C:\\...\\audio.wav"}
{"type":"mention","name":"file.txt","path":"C:\\...\\file.txt"}
```

Adapter les tests `CodexClientTests` du plan app-server.

- [ ] **Step 3: Créer `PromptEditor`**

Au démarrage UI, `LoadLibraryW(L"Msftedit.dll")`. Créer un child window `MSFTEDIT_CLASS` multi-line sans bord natif, fond aligné sur `ThemePalette::surface`, police Segoe UI Variable 10.5pt, scrollbar verticale automatique après hauteur max.

Sous-classer avec `SetWindowSubclass` :

```text
Enter       -> callback Submit si pas de Shift
Shift+Enter -> laisser RichEdit insérer CRLF
Esc         -> retirer focus si aucun overlay prioritaire
```

- [ ] **Step 4: Auto-expansion**

Mesurer le contenu avec `EM_REQUESTRESIZE`; hauteur minimale 44 DIPs, maximale 180 DIPs. Au-delà, conserver 180 et activer scroll interne. Recalculer `MainLayout` sans animation bloquante.

- [ ] **Step 5: Implémenter `ComposerModel::BuildSubmission`**

Le texte non vide devient toujours le premier input `Text`; chaque attachment supporté suit dans l’ordre d’ajout. Une soumission sans texte et sans attachment est rejetée localement.

- [ ] **Step 6: Ajouter OLE `IDropTarget` pour fichiers externes**

Enregistrer le Workbench via `RegisterDragDrop`. Accepter seulement `CF_HDROP`. Au drop, classifier chaque path ; les unsupported apparaissent dans un message inline sous le composer, sans modale.

- [ ] **Step 7: Commit**

```bash
git add src/composer src/codex src/ui tests/composer tests/codex CMakeLists.txt
git commit -m "feat: add native prompt composer and attachments"
```

---

### Task 9: Envoyer les prompts, gérer l’optimisme et finaliser le Workbench

**Files:**
- Create: `src/workbench/WorkbenchController.h`
- Create: `src/workbench/WorkbenchController.cpp`
- Modify: `src/workbench/TimelineReducer.cpp`
- Modify: `src/workbench/WorkbenchView.cpp`
- Modify: `src/app/DeckApp.cpp`
- Test: `tests/workbench/WorkbenchControllerTests.cpp`

**Interfaces:**
- Produces: sélection session -> `ReadThread`/`ResumeThread` lazy.
- Produces: `SubmitComposer()` -> optimistic user item -> `StartTurn`.

- [ ] **Step 1: Écrire le test d’ouverture lazy**

Sélectionner session A : `ReadThread(includeTurns=true)` exactement une fois. Re-sélectionner A sans invalidation : réutiliser document mémoire. Après reconnexion/resync marquant A stale : prochain open relit A.

- [ ] **Step 2: Écrire le test d’envoi optimiste**

À Submit : ajouter immédiatement `UserMessage` id local `client:<uuid>`, vider le composer, appeler `StartTurn`. Succès remappe/valide l’item lorsque l’historique/notification autoritative arrive. Échec marque l’item `Failed` et expose `Retry`.

- [ ] **Step 3: Implémenter le contrôleur**

Le contrôleur possède un document par thread ouvert, borné à 8 documents chauds. Éviction LRU supprime le modèle de timeline mémoire mais jamais la conversation Codex ; relecture future via `thread/read`.

- [ ] **Step 4: Synchroniser état Tree/Workbench**

`PendingApproval` -> arbre `NeedsAttention`. `turn/started` -> Working. `turn/completed` -> Completed. Les changements passent par `SessionRuntimeRegistry`; le Workbench ne duplique pas une seconde machine d’état de session.

- [ ] **Step 5: Ajouter Jump to latest et indicateur nouveau contenu**

Si utilisateur n’est pas near-bottom pendant streaming, ne pas déplacer le scroll ; afficher `↓ New activity`. Un clic appelle `ScrollToBottom()`.

- [ ] **Step 6: Validation complète**

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Avec un vrai thread Codex : ouvrir, lire l’historique, envoyer un prompt, observer streaming, commande, approval, fichiers modifiés et Review changes.

- [ ] **Step 7: Commit**

```bash
git add src/workbench src/composer src/app tests/workbench
git commit -m "feat: complete Codex Deck workbench"
```

## Critère de sortie du plan

Une session Codex peut être ouverte et pilotée de bout en bout depuis Codex Deck. La conversation est rendue comme timeline de travail native ; streaming, commandes, outils, approbations, fichiers et diffs restent interactifs et virtualisés. Le composer gère texte, images/audio locaux et mentions workspace sans navigateur embarqué.

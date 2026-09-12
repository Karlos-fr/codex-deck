# Codex Deck — Spécification de conception

Date : 12 septembre 2026

## 1. Vision

Codex Deck est un client Windows natif, rapide et léger, destiné à piloter plusieurs sessions Codex depuis une interface unique, dense et soignée.

L’objectif n’est pas de reproduire l’application ChatGPT ou l’interface Codex officielle. Codex Deck doit se comporter comme un **poste de pilotage pour sessions et agents Codex** : projets, sessions, états d’exécution, approbations, commandes, fichiers modifiés, diffs et conversations sont regroupés dans un même Workbench.

Le produit vise en priorité les utilisateurs qui travaillent avec plusieurs sessions Codex en parallèle et qui veulent :

- retrouver immédiatement leurs sessions ;
- voir quelles sessions travaillent, attendent une action, ont terminé ou sont en erreur ;
- reprendre, créer, renommer et archiver des sessions ;
- regrouper les sessions dans des projets logiques ;
- piloter Codex sans dépendre du terminal ou de VS Code pour les opérations courantes ;
- conserver une interface Windows native, réactive et peu gourmande.

Description courte du dépôt :

> A fast, lightweight native Windows client for managing and running multiple Codex sessions, projects, and agents from one polished workbench.

## 2. Principes directeurs

1. **Codex reste la source de vérité pour Codex.**
   Les threads, conversations, noms de sessions, statuts, outils, approbations et paramètres Codex proviennent de `codex app-server`.

2. **Codex Deck est la source de vérité pour l’organisation locale.**
   Les projets logiques, associations manuelles, favoris, état de l’interface et préférences propres à Codex Deck sont conservés localement.

3. **Aucune duplication inutile de l’historique Codex.**
   SQLite ne devient pas une copie de la base de conversations Codex.

4. **Une interface de travail, pas une messagerie.**
   Le cœur visuel est une timeline d’activité mêlant texte, commandes, outils, diffs, fichiers, tests, erreurs et approbations.

5. **Rapidité perçue avant chargement exhaustif.**
   La fenêtre et le dernier état local connu apparaissent immédiatement ; la synchronisation Codex se fait ensuite sans bloquer l’UI.

6. **Pas de framework UI lourd.**
   Le projet utilise C++23, Win32, Direct2D, DirectWrite et DirectComposition, avec une UI custom.

7. **Le “juice” doit informer.**
   Les animations servent à communiquer un état ou une transition. Elles ne ralentissent jamais une interaction.

## 3. Socle technique et stratégie de bootstrap

### 3.1 Point de départ

Codex Deck ne dépend pas du dépôt `native-win32-glass-kit` actuel pour sa V1.

Le bootstrap est réalisé à partir du code éprouvé de `Karlos-fr/codex-glass`, car Codex Glass contient déjà un socle réellement utilisé en production personnelle :

- cycle de vie Win32 ;
- fenêtre custom ;
- DPI ;
- Direct2D / DirectWrite ;
- D3D11 / DXGI ;
- effets Glass ;
- effets Motion ;
- SQLite ;
- menus owner-drawn ;
- shell / tray ;
- gestion de thème et ressources Windows.

Codex Deck est cependant un **nouveau projet autonome**. Il ne doit pas conserver la logique métier spécifique de Codex Glass : quotas, tokens, heatmap, graphiques d’usage, modes de widget, click-through, docking métier ou provider de quota.

### 3.2 Migration vers C++23

Codex Deck est compilé en C++23 dès son bootstrap.

Le passage de la base Codex Glass à C++23 doit d’abord être minimal : changement du standard, compilation Debug/Release et correction des incompatibilités éventuelles. Les nouveautés C++23 sont introduites lorsqu’elles améliorent réellement la lisibilité ou la robustesse.

`std::expected` est notamment recommandé pour les frontières susceptibles d’échouer : processus, JSON-RPC, stockage, parsing, requêtes Codex et synchronisation.

### 3.3 Glass Kit v2

Codex Deck sert de second consommateur réel pour faire émerger un nouveau socle générique.

Le sens d’évolution est :

```text
Codex Glass
    ↓
Codex Deck
    ↓
Glass Kit v2
    ↓
Codex Glass + Codex Deck
```

Les abstractions réutilisables sont extraites **après** avoir été éprouvées dans Codex Deck. Le dépôt `native-win32-glass-kit` actuel pourra alors être remplacé ou profondément remanié avec cette nouvelle base.

Aucun framework générique complet ne doit être conçu à l’avance.

## 4. Plateforme et pile technique

### 4.1 Cible principale

La V1 cible **Windows 11 x64**. Windows 10 n’est pas une cible de compatibilité garantie pour la V1.

### 4.2 Pile

- C++23 ;
- Win32 ;
- Direct2D ;
- DirectWrite ;
- DirectComposition ;
- D3D11 / DXGI pour les effets Glass hérités ;
- SQLite pour les données propres à Codex Deck ;
- CMake ;
- tests CTest.

### 4.3 Thèmes

Trois modes :

- `System` — valeur par défaut ;
- `Light` ;
- `Dark`.

Le mode `System` suit le thème Windows.

## 5. Intégration Codex

### 5.1 App-server

Codex Deck lance et supervise son propre processus enfant :

```text
Codex Deck
    ↓ stdin/stdout JSON-RPC
codex app-server
```

Un seul `codex app-server` est utilisé pour l’ensemble des sessions.

Codex Deck doit gérer :

- démarrage ;
- handshake ;
- lecture et écriture JSON-RPC ;
- notifications ;
- crash ;
- reconnexion ;
- resynchronisation après redémarrage.

Aucune opération d’IO Codex ne doit tourner sur le thread UI.

### 5.2 Fonctions Codex attendues

La V1 doit pouvoir, via les API Codex disponibles :

- lister les threads ;
- lire une session ;
- reprendre une session ;
- créer une session ;
- envoyer un prompt ;
- recevoir le streaming ;
- renommer une session côté Codex ;
- archiver une session ;
- recevoir les changements d’état ;
- traiter les demandes d’approbation ;
- afficher les commandes et outils ;
- afficher les fichiers modifiés et diffs disponibles ;
- fonctionner avec plusieurs sessions actives en parallèle.

### 5.3 Configuration Codex

La configuration Codex existante reste la source de vérité : authentification, modèle, effort, permissions, plugins/MCP et paramètres globaux.

Codex Deck peut proposer des overrides ponctuels au niveau d’une session lorsque `app-server` les supporte, mais ne crée pas une seconde configuration globale concurrente.

### 5.4 Renommage

Le renommage d’une session doit appeler l’API Codex correspondante. Il ne s’agit pas d’un alias local.

L’UI utilise un comportement optimiste : le nouveau nom est affiché immédiatement, puis confirmé par Codex. En cas d’échec, l’ancien nom est restauré avec une erreur discrète.

## 6. Modèle projets / sessions

### 6.1 Projet logique

Un projet Codex Deck n’est ni un simple `cwd`, ni uniquement un dépôt Git.

Il s’agit d’un objet local pouvant être associé à :

- un ou plusieurs chemins racine ;
- un dépôt Git / remote détecté ;
- plusieurs sessions Codex.

Le `cwd` et le dépôt Git servent d’indices pour l’association automatique.

### 6.2 Association automatique et manuelle

Lorsqu’une nouvelle session est découverte :

1. tentative d’identification par `cwd` ;
2. détection du dépôt Git si utile ;
3. association à un projet connu ;
4. sinon placement dans `Unassigned`.

Une association manuelle effectuée par l’utilisateur a toujours priorité sur l’association automatique.

### 6.3 Unassigned

`Unassigned` est une section fixe de l’arbre destinée aux sessions qui ne peuvent pas être associées automatiquement à un projet.

Une session peut être déplacée manuellement de `Unassigned` vers un projet ou inversement.

### 6.4 Arbre simple

Un projet contient directement ses sessions.

Pas de sous-dossiers arbitraires dans la V1.

```text
▾ SpotifyAmp
   ● Audio parity
   ◐ Skin engine
   ✓ Winamp research
```

### 6.5 Tri

Les sessions d’un projet sont triées par activité récente décroissante.

Les projets sont triés par l’activité de leur session la plus récente.

L’arbre ne doit pas se réordonner à chaque token ou ligne de sortie. Le tri est recalculé lors d’événements significatifs : nouveau prompt, création/reprise, fin de tour, demande d’attention ou synchronisation externe.

`Unassigned` garde une position fixe.

## 7. Interface principale — Tree + Workbench

### 7.1 Structure

La fenêtre principale comporte deux zones majeures :

```text
┌──────────────────────┬─────────────────────────────────────────────┐
│ TREE                 │ WORKBENCH                                   │
│ projets + sessions   │ session active                             │
└──────────────────────┴─────────────────────────────────────────────┘
```

Les projets et sessions sont fusionnés dans un seul arbre.

La conversation n’est pas présentée comme une messagerie à bulles.

### 7.2 Barre d’activité globale

Une barre compacte résume l’activité :

```text
● 3 working   ◐ 1 attention   ✓ 4 completed today   Ctrl+K
```

Un clic sur un état applique un filtre temporaire à l’arbre ou affiche les sessions concernées.

### 7.3 États de session

États visuels minimaux :

- `● Working` ;
- `◐ Needs attention` ;
- `○ Idle` ;
- `✓ Completed` ;
- `! Error`.

Le même langage visuel est utilisé dans l’arbre, le Workbench et les notifications.

### 7.4 Sessions anciennes

L’arbre ne doit pas afficher indéfiniment des centaines de sessions par projet.

Après un nombre raisonnable d’entrées récentes, une ligne `… N more` ouvre une vue de recherche filtrée sur le projet.

Les archives sont accessibles via une vue globale séparée.

## 8. Workbench

### 8.1 Timeline de travail

Le Workbench affiche une timeline unifiée contenant des blocs typés :

- `YOU` ;
- `CODEX` ;
- `COMMAND` ;
- `TOOL` ;
- `FILES` ;
- `DIFF` ;
- `TESTS` ;
- `APPROVAL` ;
- `ERROR`.

Les blocs lourds sont repliables.

### 8.2 Commandes et outils

Les commandes affichent :

- commande ;
- état ;
- durée ;
- sortie condensée ;
- stdout/stderr complet sur demande.

Une sortie terminal massive ne doit pas bloquer le rendu ni gonfler inutilement la surface UI.

### 8.3 Approbations

Les approbations doivent être visibles immédiatement et casser visuellement le flux normal de la timeline sans passer par une modale système générique.

Le statut `Needs attention` est reflété simultanément dans l’arbre.

### 8.4 Fichiers et diffs

La timeline affiche un résumé des fichiers modifiés.

L’action `Review changes` ouvre une vue temporaire de diff dans le Workbench. Cette vue peut comporter une liste de fichiers à gauche et le diff principal à droite.

Elle ne crée pas une troisième colonne permanente dans la fenêtre principale.

### 8.5 Composer

La zone de prompt reste disponible en bas du Workbench.

Comportement attendu :

- auto-expansion jusqu’à une hauteur maximale ;
- scroll ensuite ;
- `Enter` pour envoyer ;
- `Shift+Enter` pour une nouvelle ligne ;
- pièces jointes ;
- affichage compact du modèle, effort et permissions actifs.

Le prochain prompt peut être préparé pendant que Codex travaille.

### 8.6 Markdown natif

Pas de navigateur embarqué.

Le moteur natif couvre au minimum :

- paragraphes ;
- titres ;
- gras / italique ;
- listes ;
- code inline ;
- blocs de code ;
- tableaux ;
- liens ;
- citations.

## 9. Navigation et productivité

### 9.1 Command Palette

`Ctrl+K` ouvre une palette globale permettant de rechercher et d’exécuter des actions.

Elle couvre :

- projets ;
- sessions ;
- actions ;
- création de session ;
- navigation ;
- favoris ;
- ouverture de workspace.

Recherche fuzzy et instantanée.

### 9.2 Raccourcis initiaux

- `Ctrl+K` — Command Palette ;
- `Ctrl+P` — recherche sessions/projets ;
- `Ctrl+N` — nouvelle session ;
- `Ctrl+Shift+N` — nouvelle session dans le projet courant ;
- `Ctrl+Tab` — session active suivante/précédente ;
- `Ctrl+1…9` — accès rapide aux sessions actives ;
- `F2` — renommer ;
- `Delete` — archiver ;
- `Ctrl+Shift+D` — détacher le Workbench.

### 9.3 Drag & drop

Usages limités :

- session vers projet ;
- session vers `Unassigned` ;
- session vers Archive ;
- fichier déposé dans le Workbench comme pièce jointe.

Pas de réorganisation arbitraire de l’arbre.

## 10. Nouvelle session

### 10.1 Depuis un projet

Une action `+ New session` sur un projet utilise automatiquement le projet et son workspace principal.

### 10.2 Création globale

`Ctrl+N` permet de sélectionner rapidement :

- projet ;
- workspace ;
- modèle si override souhaité ;
- prompt initial.

Les autres paramètres héritent de Codex.

### 10.3 Session depuis un dossier

Une session peut être créée directement depuis un dossier sans créer préalablement de projet logique. Elle apparaît alors dans `Unassigned` si aucune association n’est trouvée.

## 11. Multi-session et multi-fenêtres

### 11.1 Sessions actives

Plusieurs threads peuvent être actifs simultanément sur le même `app-server`.

Le modèle runtime conserve un état séparé par `threadId` :

```text
SessionRuntime
├─ threadId
├─ status
├─ currentTurn
├─ pendingApproval
└─ latestActivity
```

Les notifications Codex mettent uniquement à jour la session concernée.

### 11.2 Fenêtres détachées

La V1 est conçue pour supporter plusieurs fenêtres.

Une fenêtre détachée contient uniquement un Workbench, sans arbre projets/sessions.

Toutes les fenêtres partagent :

- le même `app-server` ;
- le même modèle en mémoire ;
- le même stockage SQLite.

## 12. Stockage local

### 12.1 SQLite

SQLite conserve uniquement les données propres à Codex Deck et le cache minimal nécessaire à un démarrage instantané.

Exemples :

### Project

- `id` ;
- `name` ;
- racines ;
- remote Git détecté ;
- date de création.

### SessionMetadata

- `codexThreadId` ;
- `projectId` ;
- source d’association automatique/manuelle ;
- favori ;
- état UI utile ;
- dernière activité connue ;
- dernier titre et `cwd` connus pour le cache de lancement.

### WorkspaceState

- fenêtre principale ;
- position et taille ;
- session sélectionnée ;
- projets développés ;
- position de scroll utile ;
- fenêtres détachées.

### 12.2 Données non dupliquées

La base Codex Deck ne conserve pas une copie complète :

- des prompts ;
- des réponses ;
- des commandes ;
- des diffs ;
- de l’historique de conversation.

## 13. Synchronisation

### 13.1 Démarrage

Ordre attendu :

```text
lancement
  ↓
lecture SQLite
  ↓
affichage immédiat du dernier état
  ↓
lancement/connexion app-server
  ↓
resynchronisation
  ↓
mises à jour ciblées
```

Pas d’écran bloquant attendant que toutes les sessions Codex soient chargées.

### 13.2 Sessions créées ailleurs

Une session créée depuis VS Code, CLI ou autre client doit apparaître lors de la synchronisation.

Elle est automatiquement rattachée à un projet lorsque possible, sinon placée dans `Unassigned`.

### 13.3 Modifications externes

Les changements Codex détectés ailleurs doivent être reflétés : titre, état, activité et autres métadonnées disponibles.

Les associations manuelles Codex Deck ne sont jamais écrasées par une nouvelle détection automatique.

## 14. Robustesse

### 14.1 App-server indisponible

La déconnexion du serveur ne ferme pas Codex Deck.

Les derniers états connus restent visibles. L’application tente de relancer/reconnecter `app-server`, puis resynchronise.

États globaux possibles :

- `Connected` ;
- `Starting` ;
- `Reconnecting` ;
- `Codex unavailable`.

### 14.2 Erreurs

Les erreurs liées à une session apparaissent principalement dans la timeline.

Les modales sont réservées aux situations qui exigent une décision bloquante de l’utilisateur.

### 14.3 Erreurs typées

Les couches techniques utilisent des erreurs structurées, par exemple :

- `ProcessNotFound` ;
- `ProcessCrashed` ;
- `ProtocolError` ;
- `RequestTimeout` ;
- `CodexError` ;
- `InvalidResponse` ;
- `StorageError`.

## 15. Rendu et performances

### 15.1 Pipeline

```text
Win32
  ↓
Direct2D + DirectWrite
  ↓
DirectComposition
  ↓
Motion / Glass ponctuel
```

Direct2D/DirectWrite dessinent l’essentiel de l’interface.

DirectComposition est utilisé lorsque des surfaces ou animations indépendantes apportent un gain réel : overlays, palette, panneaux, transitions, scrolling et compositions ciblées.

Le Glass reste un matériau d’accent, pas une surcharge omniprésente.

### 15.2 Virtualisation

Doivent être virtualisés dès la conception :

- arbre projets/sessions ;
- timeline ;
- longues sorties terminal ;
- listes de recherche.

### 15.3 Objectifs de performance

Cibles d’ingénierie initiales :

- affichage visuel initial à froid : objectif inférieur à 300 ms sur machine de développement courante ;
- interaction UI compatible 60 Hz, sans frame bloquée par l’IO Codex ;
- CPU au repos proche de 0 % ;
- navigation dans l’arbre perceptuellement instantanée ;
- recherche perceptuellement instantanée ;
- mémoire nettement inférieure à un client desktop basé sur Chromium à fonctionnalités comparables.

Ces valeurs sont des objectifs de conception et devront être validées par benchmarks.

## 16. Motion, Glass et polish

### 16.1 Animation

Trois familles :

- micro-interactions : environ 80–120 ms ;
- transitions structurelles : environ 150–220 ms ;
- animation d’activité continue mais discrète.

Aucune animation ne bloque l’interaction suivante.

### 16.2 Principe visuel

Pas de grandes bulles de chat.

Pas de multiplication de cartes arrondies purement décoratives.

Le polish vient de :

- la densité maîtrisée ;
- les transitions courtes ;
- les états temps réel ;
- la typographie ;
- le Glass utilisé avec parcimonie ;
- la fluidité du scrolling ;
- les retours immédiats sur les actions.

## 17. Notifications

Notifications internes temps réel via les badges de statut.

Notification Windows lorsque l’application n’est pas au premier plan et qu’une session :

- demande une intervention ;
- échoue ;
- termine un travail important.

Pas de notification pour chaque commande intermédiaire.

Chaque catégorie peut être désactivée.

Un clic sur une notification ouvre directement la session concernée.

## 18. Restauration de l’espace de travail

Au lancement, Codex Deck restaure :

- position et taille de la fenêtre principale ;
- thème ;
- projet(s) développé(s) ;
- session sélectionnée ;
- position de scroll utile ;
- fenêtres Workbench détachées ;
- derniers filtres utiles si leur restauration ne crée pas de confusion.

Aucun écran d’accueil intermédiaire n’est imposé.

## 19. Git

Codex Deck n’essaie pas de remplacer la gestion Git de Codex.

La V1 peut afficher les informations Git disponibles :

- dépôt ;
- branche ;
- fichiers modifiés ;
- diffs.

Elle ne crée pas elle-même de branches ou worktrees et ne construit pas un moteur Git concurrent.

## 20. Accessibilité

L’UI custom doit préserver la possibilité d’ajouter ou maintenir :

- navigation clavier complète ;
- focus visible ;
- scaling DPI ;
- contraste suffisant ;
- prise en compte de Reduced Motion ;
- UI Automation pour les contrôles structurants.

La V1 n’exige pas une couverture d’accessibilité exhaustive, mais l’architecture ne doit pas la rendre impossible.

## 21. Tests

### 21.1 Séparation métier / UI

La logique doit être testable sans créer de fenêtre.

Organisation cible :

```text
tests/
├─ codex/
│  ├─ JsonRpcTransportTests
│  ├─ ProtocolParsingTests
│  └─ SessionSyncTests
├─ projects/
│  ├─ ProjectDetectionTests
│  ├─ AssignmentTests
│  └─ SortingTests
├─ storage/
│  └─ SQLiteTests
├─ model/
│  ├─ ActivityTests
│  └─ TimelineTests
└─ ui/
   └─ LayoutAndGeometryTests
```

### 21.2 Fake app-server

Le client Codex doit pouvoir être testé avec un serveur JSON-RPC simulé afin de reproduire sans coût ni accès réseau :

- plusieurs threads actifs ;
- streaming ;
- approbations ;
- commandes terminées ;
- nouvelle session externe ;
- renommage externe ;
- crash serveur ;
- reconnexion ;
- erreurs de protocole.

### 21.3 Tests de charge UI

Jeux de données synthétiques à prévoir :

- 500 projets ;
- 10 000 sessions ;
- 50 000 événements dans une timeline.

L’objectif est de forcer l’architecture à virtualiser et à éviter les coûts proportionnels au volume total affiché.

### 21.4 Validation

Debug et Release doivent compiler et les tests doivent passer avant de considérer une étape comme terminée.

## 22. Périmètre V1

### Inclus

#### Socle

- bootstrap depuis Codex Glass ;
- C++23 ;
- Win32 / Direct2D / DirectWrite ;
- DirectComposition ;
- thème System / Light / Dark ;
- SQLite ;
- `codex app-server` supervisé.

#### Sessions

- liste ;
- recherche ;
- création ;
- ouverture / reprise ;
- renommage côté Codex ;
- archive ;
- statut temps réel ;
- multi-session.

#### Projets

- projet logique ;
- auto-détection ;
- association manuelle ;
- `Unassigned` ;
- drag & drop session → projet ;
- tri automatique par activité récente.

#### Workbench

- timeline native ;
- réponses streaming ;
- commandes / outils ;
- stdout/stderr ;
- approbations ;
- fichiers modifiés ;
- diff review ;
- composer natif ;
- Markdown essentiel.

#### UX

- Command Palette ;
- raccourcis clavier ;
- notifications Windows ;
- restauration complète ;
- Workbench détachable.

## 23. Hors périmètre V1

- éditeur de code complet type IDE ;
- terminal généraliste ;
- moteur Git propre ;
- création de worktrees ;
- moteur HTML complet ;
- système de plugins propre à Codex Deck ;
- synchronisation cloud des projets/tags locaux ;
- Mission Control avancé ;
- personnalisation visuelle infinie ;
- réécriture préventive d’un framework UI générique complet.

## 24. Architecture logique cible

```text
CodexDeck.exe
│
├─ platform/
│  ├─ win32
│  ├─ window
│  ├─ graphics
│  ├─ composition
│  ├─ input
│  └─ theme
│
├─ ui/
│  ├─ tree
│  ├─ timeline
│  ├─ composer
│  ├─ command_palette
│  ├─ diff_viewer
│  ├─ markdown
│  └─ controls
│
├─ codex/
│  ├─ process
│  ├─ json_rpc
│  ├─ protocol
│  └─ client
│
├─ sessions/
│  ├─ model
│  ├─ runtime
│  └─ sync
│
├─ projects/
│  ├─ model
│  ├─ detection
│  └─ assignment
│
├─ storage/
│  └─ sqlite
│
├─ notifications/
│
└─ app/
   ├─ orchestration
   ├─ windows
   └─ workspace_state
```

Ce découpage est indicatif. Les fichiers et modules finaux doivent rester petits, cohérents et orientés responsabilités plutôt que reproduire cette arborescence mécaniquement.

## 25. Critères de réussite de la V1

La V1 est réussie lorsque :

1. Codex Deck démarre comme une vraie application Windows native, rapidement et sans runtime UI lourd.
2. Les sessions Codex existantes sont retrouvées et navigables.
3. Une nouvelle session peut être créée, pilotée et reprise depuis Codex Deck.
4. Plusieurs sessions peuvent travailler en parallèle sans bloquer l’UI.
5. Les approbations et erreurs sont visibles immédiatement.
6. Les projets organisent efficacement les sessions sans casser leur compatibilité avec les autres clients Codex.
7. Une session créée ou renommée ailleurs est réconciliée correctement.
8. Le Workbench permet de suivre une tâche sans donner l’impression d’utiliser une messagerie.
9. Les listes et timelines restent fluides avec des jeux de données volumineux.
10. Le socle générique réellement éprouvé peut ensuite servir de base à Glass Kit v2 et être réinjecté dans Codex Glass.

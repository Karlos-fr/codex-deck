# Codex Deck V1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Livrer Codex Deck V1 comme client Windows natif C++23 capable d’organiser, piloter et superviser plusieurs sessions Codex depuis un Tree + Workbench.

**Architecture:** La V1 est découpée en six plans exécutables pour éviter un chantier monolithique. Le premier bootstrappe depuis Codex Glass au commit `d66821aa28c8c5293604949dad48ad9e9e99b434`; les suivants ajoutent l’intégration `codex app-server`, les projets/SQLite, la navigation, le Workbench, puis les fonctions multi-fenêtres et le durcissement final.

**Tech Stack:** C++23, Win32, Direct2D, DirectWrite, DirectComposition, D3D11/DXGI, SQLite, nlohmann/json, CMake, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- La V1 cible **Windows 11 x64**.
- Le code applicatif est compilé en **C++23**.
- Aucun framework UI lourd ni navigateur embarqué.
- Codex est la source de vérité pour les threads, conversations, noms et états Codex.
- SQLite ne duplique pas l’historique complet des conversations.
- Un seul processus enfant `codex app-server` est partagé par toutes les sessions.
- Aucun IO Codex, parsing JSON-RPC ou accès SQLite ne bloque le thread UI.
- Les commentaires source et en-têtes de fonctions restent en français, selon la convention héritée de Codex Glass.
- Les modules restent petits et focalisés ; pas de fichier fourre-tout.
- Debug et Release doivent compiler et CTest doit passer à chaque jalon.
- Le dépôt `native-win32-glass-kit` actuel n’est pas une dépendance de la V1.

---

## Ordre d’exécution

1. `2026-09-12-codex-deck-bootstrap-foundation.md`
   - Importe le socle éprouvé de Codex Glass.
   - Renomme l’application, passe en C++23, retire le métier quotas/tokens et ajoute DirectComposition.
   - Sortie : une coquille Codex Deck native compilable avec thème System/Light/Dark et tests de fondation.

2. `2026-09-12-codex-deck-app-server-client.md`
   - Lance/supervise `codex app-server`.
   - Implémente transport JSON-RPC, threads, streaming, renommage, archive, approbations et reconnexion.
   - Sortie : un moteur Codex testable avec fake app-server, sans UI riche.

3. `2026-09-12-codex-deck-projects-storage-sync.md`
   - Ajoute SQLite, projets logiques, `Unassigned`, association automatique/manuelle, cache de démarrage et synchronisation.
   - Sortie : un modèle local cohérent et réconcilié avec Codex.

4. `2026-09-12-codex-deck-tree-navigation.md`
   - Ajoute Tree virtualisé, tri par activité récente, barre d’activité, recherche, Command Palette, raccourcis et drag & drop ciblé.
   - Sortie : navigation complète sur de grands jeux de données synthétiques.

5. `2026-09-12-codex-deck-workbench.md`
   - Ajoute timeline virtualisée, Markdown natif, commandes/outils, approbations, fichiers/diffs, composer et streaming.
   - Sortie : une session Codex peut être réellement pilotée depuis Codex Deck.

6. `2026-09-12-codex-deck-v1-hardening.md`
   - Ajoute fenêtres détachées, restauration complète, notifications Windows, reduced motion, benchmarks, packaging et validation V1.
   - Sortie : V1 utilisable au quotidien.

## Règle de passage entre plans

Un plan n’est considéré terminé que lorsque :

```text
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

réussissent, et que le critère de sortie du plan est vérifié manuellement lorsque celui-ci implique une UI ou un processus externe.

## Stratégie Git

Chaque tâche du plan détaillé se termine par un commit focalisé. Les plans doivent être exécutés dans l’ordre ; ne pas démarrer le Workbench avant que les contrats `SessionModel`, `ProjectModel` et `CodexClient` soient stabilisés par les plans précédents.

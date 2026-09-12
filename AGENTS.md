# Instructions for Codex

This repository contains Codex Deck, a native Windows desktop client written in C++23 with Win32, Direct2D, DirectWrite, DirectComposition, D3D11/DXGI, SQLite, and CMake.

Before implementing a task, read the relevant project documents:

- `docs/superpowers/specs/2026-09-12-codex-deck-design.md` — validated product and architecture specification;
- `docs/superpowers/plans/2026-09-12-codex-deck-shared-contracts.md` — normative cross-plan contracts;
- `docs/superpowers/plans/2026-09-12-codex-deck-v1-roadmap.md` — implementation order;
- the detailed plan for the subsystem currently being implemented.

When following a plan, keep its checkbox steps up to date as work is completed.

## Project Organization

- Keep the project split into small, focused modules.
- Prefer one file pair per feature or responsibility: `FeatureName.h` and `FeatureName.cpp`.
- Avoid large catch-all files. Extract a module when a file starts mixing unrelated responsibilities.
- Do not add broad utility files unless several modules genuinely share the same logic.
- Keep `DeckApp.*` focused on application orchestration only.
- Keep rendering, Codex protocol, storage, project mapping, settings, localization, and Windows shell integration separated.
- Prefer native Win32/C++ code over adding frameworks or heavy dependencies.
- Do not introduce WinUI, Qt, Electron, Chromium, or an embedded web UI.
- Codex remains the source of truth for Codex threads, conversations, names, and runtime state.
- SQLite stores Codex Deck organization and lightweight cache data only; do not duplicate full Codex conversation history.
- Keep process, pipe, JSON-RPC, SQLite, Git detection, and other blocking I/O off the UI thread.

## Commenting Rules

All source comments must be written in French.

Every source or header file must start with a French file header comment describing:

- the module name;
- the module responsibility;
- any important boundary with other modules.

Every function must have a French header comment describing:

- what the function does;
- its parameters, when relevant;
- its return value, when relevant;
- any important side effect, when relevant.

Every constant must have a dedicated French comment explaining what it represents.

Prefer useful comments that explain intent, boundaries, assumptions, and side effects. Avoid comments that merely repeat the code.

## Editing Guidelines

- Keep changes scoped to the current task and validated plan.
- Follow the existing C++ style and naming conventions.
- Do not rewrite unrelated modules while implementing a feature.
- Do not remove existing user changes unless explicitly requested.
- Treat `docs/superpowers/plans/2026-09-12-codex-deck-shared-contracts.md` as authoritative when a detailed plan conflicts with it.
- Update localized resources when adding user-facing text.
- Update the README when user-visible behavior or build requirements materially change.
- Prefer TDD for new behavior: failing test, minimal implementation, passing test, then refactor if needed.
- Keep commits focused and independently reviewable.

## Build and Validation

Codex Deck targets Windows 11 x64 and strict C++23.

Once the native bootstrap is present, both Debug and Release must build successfully and all CTest tests must pass before a task is considered complete.

Use the repository CMake presets defined by the implementation plan:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

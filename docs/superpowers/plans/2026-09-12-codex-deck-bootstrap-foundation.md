# Codex Deck Bootstrap & Native Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Partir du socle réellement éprouvé de Codex Glass, passer Codex Deck en C++23 et obtenir une coquille Windows 11 native propre avec Direct2D/DirectWrite/DirectComposition, thèmes System/Light/Dark et tests de fondation.

**Architecture:** Le bootstrap importe d’abord Codex Glass **sans le réécrire** au commit `d66821aa28c8c5293604949dad48ad9e9e99b434`, prouve que cette base compile, puis remplace progressivement l’orchestration métier du widget par des modules Codex Deck focalisés. Les anciens fichiers métiers quotas/tokens ne sont supprimés qu’après que la nouvelle coquille rend une fenêtre native fonctionnelle.

**Tech Stack:** C++23, Win32, Direct2D 1.1, DirectWrite, DirectComposition, D3D11/DXGI, SQLite, nlohmann/json, CMake, CTest.

**Spec:** `docs/superpowers/specs/2026-09-12-codex-deck-design.md`

## Global Constraints

- Windows 11 x64 uniquement pour la cible garantie V1.
- `cxx_std_23` pour toutes les cibles C++ Codex Deck et tests.
- Aucun WinUI, Qt, WebView, Electron ou Chromium.
- Le dépôt `native-win32-glass-kit` actuel n’est pas utilisé.
- Tous les commentaires source et en-têtes de fonctions sont en français.
- Les modules restent petits et focalisés ; `DeckApp` orchestre, il ne rend pas et ne persiste pas.
- La logique métier quotas/tokens de Codex Glass ne doit pas survivre dans la coquille finale de ce plan.
- Le code Glass/Motion conservé peut garder ses noms `Widget*` pendant ce premier plan s’il n’est pas encore généralisé ; ne pas lancer une refonte cosmétique massive.

---

### Task 1: Importer un snapshot reproductible de Codex Glass et établir la baseline

**Files:**
- Create from source snapshot: `.gitignore`
- Create from source snapshot: `CMakeLists.txt`
- Create from source snapshot: `src/**`
- Create from source snapshot: `tests/**`
- Create: `docs/bootstrap/CODEX_GLASS_PROVENANCE.md`

**Interfaces:**
- Consumes: Codex Glass commit `d66821aa28c8c5293604949dad48ad9e9e99b434`.
- Produces: une copie autonome compilable de la base Codex Glass dans le repo Codex Deck, avant toute transformation.

- [x] **Step 1: Importer exactement les sources nécessaires depuis le commit de référence**

```powershell
$source = Join-Path $env:TEMP "codex-glass-codex-deck-bootstrap"
Remove-Item $source -Recurse -Force -ErrorAction SilentlyContinue
git clone https://github.com/Karlos-fr/codex-glass.git $source
git -C $source checkout d66821aa28c8c5293604949dad48ad9e9e99b434
Copy-Item "$source\.gitignore" . -Force
Copy-Item "$source\CMakeLists.txt" . -Force
Copy-Item "$source\src" . -Recurse -Force
Copy-Item "$source\tests" . -Recurse -Force
```

- [x] **Step 2: Documenter la provenance avant modification**

Créer `docs/bootstrap/CODEX_GLASS_PROVENANCE.md` :

```markdown
# Provenance du bootstrap Codex Deck

Codex Deck a été bootstrapé depuis `Karlos-fr/codex-glass` au commit :

`d66821aa28c8c5293604949dad48ad9e9e99b434`

Le snapshot initial comprend `src/`, `tests/`, `.gitignore` et `CMakeLists.txt`.
La logique spécifique quotas/tokens n'est conservée que pour établir une baseline compilable avant son retrait contrôlé.
```

- [x] **Step 3: Compiler la baseline Debug**

```powershell
cmake -S . -B build-baseline -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-baseline
ctest --test-dir build-baseline -C Debug --output-on-failure
```

Expected: compilation et tests identiques à Codex Glass au commit importé.

- [x] **Step 4: Compiler la baseline Release**

```powershell
cmake -S . -B build-baseline-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-baseline-release
ctest --test-dir build-baseline-release -C Release --output-on-failure
```

Expected: PASS.

- [x] **Step 5: Commit**

```bash
git add .gitignore CMakeLists.txt src tests docs/bootstrap/CODEX_GLASS_PROVENANCE.md
git commit -m "chore: bootstrap Codex Deck from Codex Glass"
```

---

### Task 2: Renommer le produit et passer toute la base en C++23 sans changer le comportement

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `src/win32/SingleInstance.cpp`
- Rename: `src/resources/CodexGlass.rc` → `src/resources/CodexDeck.rc`
- Rename: `src/resources/CodexGlass.ico` → `src/resources/CodexDeck.ico`
- Rename: `src/resources/CodexGlassTray.ico` → `src/resources/CodexDeckTray.ico`
- Modify: `src/resources/VersionInfo.rc.in`
- Modify: `src/resources/strings.fr.rc`
- Modify: `src/resources/strings.en.rc`
- Modify tests containing `CodexGlass` names/mutexes.
- Create: `AGENTS.md`
- Create: `README.md`

**Interfaces:**
- Consumes: snapshot compilable de Task 1.
- Produces: cible `CodexDeck.exe`, projet CMake `CodexDeck`, standard C++23, mutex `Local\\Karlos-fr.CodexDeck.Instance`.

- [x] **Step 1: Faire échouer un test sur l’identité du mutex**

Dans `tests/SingleInstanceTests.cpp`, remplacer le préfixe de test par :

```cpp
return L"Local\\CodexDeck.SingleInstanceTests." + std::to_wstring(GetCurrentProcessId());
```

Puis ajouter dans `src/win32/SingleInstance.h` l’API testable :

```cpp
const wchar_t* DefaultSingleInstanceMutexName();
```

Dans le test :

```cpp
if (std::wstring(DefaultSingleInstanceMutexName()) != L"Local\\Karlos-fr.CodexDeck.Instance") {
    return 4;
}
```

- [x] **Step 2: Vérifier l’échec**

```powershell
cmake --build build-baseline
ctest --test-dir build-baseline -R SingleInstanceTests --output-on-failure
```

Expected: FAIL car l’API et/ou le nom Codex Deck n’existent pas encore.

- [x] **Step 3: Renommer CMake et activer C++23**

Dans `CMakeLists.txt`, appliquer ces règles exactes :

```cmake
project(CodexDeck
    VERSION 0.1.0
    DESCRIPTION "Codex Deck"
    LANGUAGES C CXX RC
)

set(CODEX_DECK_VERSION_BUILD "1" CACHE STRING "Windows file version build number")
set(CODEX_DECK_VERSION_COMPANY "Codex Deck" CACHE STRING "Windows version metadata company name")
set(CODEX_DECK_VERSION_COPYRIGHT "Copyright (C) 2026 Codex Deck" CACHE STRING "Windows version metadata copyright")

add_executable(CodexDeck WIN32
    # liste de sources existante, renommée uniquement pour la cible
)

target_compile_features(CodexDeck PRIVATE cxx_std_23)
```

Remplacer également `cxx_std_20` par `cxx_std_23` sur **toutes** les cibles de test C++.

- [x] **Step 4: Renommer l’identité Win32**

Dans `src/win32/SingleInstance.cpp` :

```cpp
namespace {
constexpr wchar_t kCodexDeckMutexName[] = L"Local\\Karlos-fr.CodexDeck.Instance";
}

const wchar_t* DefaultSingleInstanceMutexName() {
    return kCodexDeckMutexName;
}

SingleInstance::SingleInstance()
    : SingleInstance(DefaultSingleInstanceMutexName()) {
}
```

Dans `src/resources/VersionInfo.rc.in`, utiliser `CodexDeck`, `CodexDeck.exe` et les variables `CODEX_DECK_*`. Dans les tables de chaînes, `IDS_APP_TITLE` devient `Codex Deck`.

- [x] **Step 5: Renommer les ressources et les références CMake**

```powershell
Rename-Item src/resources/CodexGlass.rc CodexDeck.rc
Rename-Item src/resources/CodexGlass.ico CodexDeck.ico
Rename-Item src/resources/CodexGlassTray.ico CodexDeckTray.ico
```

Mettre `CodexDeck.rc` à jour pour référencer les deux nouveaux noms d’icônes.

- [x] **Step 6: Créer les règles repo Codex Deck**

`AGENTS.md` doit conserver les règles de modularité/commentaires de Codex Glass et remplacer la description par :

```markdown
# Instructions for Codex

Codex Deck is a native Windows desktop client written in C++23 with Win32, Direct2D, DirectWrite, DirectComposition, D3D11/DXGI, SQLite and CMake.

- Keep modules small and focused.
- Keep `DeckApp.*` limited to orchestration.
- Keep rendering, Codex protocol, storage, project mapping and Windows shell integration separated.
- All source comments and function header comments must be written in French.
- Prefer native Win32/C++ over adding dependencies.
- Debug and Release builds plus CTest must pass before completing a task.
```

Créer un `README.md` minimal présentant Codex Deck, le statut pré-alpha, Windows 11 x64, C++23 et les commandes de build.

- [x] **Step 7: Reconfigurer depuis zéro et valider C++23**

```powershell
Remove-Item build -Recurse -Force -ErrorAction SilentlyContinue
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

Expected: `CodexDeck.exe`, PASS.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "refactor: rename bootstrap to Codex Deck and adopt C++23"
```

---

### Task 3: Remplacer le widget métier par une coquille Deck native minimale

**Files:**
- Create: `src/app/DeckApp.h`
- Create: `src/app/DeckApp.cpp`
- Create: `src/window/DeckWindow.h`
- Create: `src/window/DeckWindow.cpp`
- Create: `src/rendering/DeckRenderer.h`
- Create: `src/rendering/DeckRenderer.cpp`
- Create: `src/settings/DeckSettings.h`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/DeckWindowGeometryTests.cpp`
- Delete from build and repository after replacement works: `src/usage/**`, `src/providers/**`, `src/tokens/**`, `src/activity/**`, quota/graph-specific animation/rendering/controllers.

**Interfaces:**
- Produces: `class DeckApp { int Run(HINSTANCE, int); }`.
- Produces: `CreateDeckMainWindow(HINSTANCE, WNDPROC, void*) -> HWND`.
- Produces: `DeckRenderer::Initialize(HWND)`, `Resize(HWND)`, `Render(HWND, const DeckVisualState&)`.

- [ ] **Step 1: Écrire le test de géométrie de fenêtre**

Créer `tests/DeckWindowGeometryTests.cpp` avec un test pur de taille minimale :

```cpp
#include "window/DeckWindow.h"

int main() {
    const SIZE minimum = DeckMinimumClientSize();
    if (minimum.cx != 960 || minimum.cy != 640) {
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Vérifier l’échec**

Ajouter temporairement la cible de test CMake puis :

```powershell
cmake --build build
ctest --test-dir build -R DeckWindowGeometryTests --output-on-failure
```

Expected: FAIL car `DeckWindow.h` n’existe pas.

- [ ] **Step 3: Créer le contrat de fenêtre**

`src/window/DeckWindow.h` :

```cpp
#pragma once
#include <windows.h>

SIZE DeckMinimumClientSize();
bool RegisterDeckWindowClass(HINSTANCE instance, WNDPROC window_proc);
HWND CreateDeckMainWindow(HINSTANCE instance, WNDPROC window_proc, void* create_parameter);
void ApplyDeckWindowTheme(HWND hwnd, bool dark);
```

`DeckMinimumClientSize()` retourne `{960, 640}`. La fenêtre initiale est centrée et créée avec un style desktop redimensionnable normal, sans logique docking/click-through.

- [ ] **Step 4: Créer un modèle visuel minimal et le renderer**

`src/rendering/DeckRenderer.h` :

```cpp
#pragma once
#include <windows.h>
#include <string>

struct DeckVisualState {
    std::wstring title = L"Codex Deck";
    std::wstring subtitle = L"Native Codex workbench";
};

class DeckRenderer {
public:
    bool Initialize(HWND hwnd);
    void Resize(HWND hwnd);
    void Render(HWND hwnd, const DeckVisualState& state);
    void DiscardDeviceResources();
};
```

Pour cette tâche, réutiliser le chemin Direct2D/DirectWrite éprouvé de Codex Glass mais limiter le rendu à un fond, le titre et le sous-titre. Ne pas conserver les structures `UsageSnapshot` ou `TokenUsageSnapshot`.

- [ ] **Step 5: Créer l’orchestrateur**

`src/app/DeckApp.h` expose uniquement :

```cpp
class DeckApp {
public:
    int Run(HINSTANCE instance, int command_show);
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
private:
    LRESULT HandleWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    DeckRenderer renderer_;
    DeckVisualState visual_state_{};
};
```

Le handler couvre au minimum `WM_NCCREATE`, `WM_CREATE`, `WM_SIZE`, `WM_DPICHANGED`, `WM_PAINT`, `WM_ERASEBKGND`, `WM_CLOSE`, `WM_DESTROY`.

- [ ] **Step 6: Basculer `main.cpp` vers `DeckApp`**

```cpp
#include "app/DeckApp.h"
#include "win32/SingleInstance.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int command_show) {
    SingleInstance single_instance;
    if (single_instance.status() == SingleInstanceStatus::AlreadyRunning) return 0;
    if (single_instance.status() == SingleInstanceStatus::Error) return 1;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    DeckApp app;
    return app.Run(instance, command_show);
}
```

- [ ] **Step 7: Réduire la cible CMake au socle réellement utilisé**

Retirer de `add_executable(CodexDeck ...)` les sources `usage`, `providers`, `tokens`, `activity`, graphes/quota et anciens contrôleurs `WidgetApp`. Conserver uniquement le socle encore référencé par `DeckApp`, le rendu minimal, les ressources, le Glass/Motion non métier et les helpers Win32 nécessaires.

- [ ] **Step 8: Supprimer les fichiers métiers devenus orphelins**

Après une compilation réussie, supprimer les répertoires et fichiers métier qui ne sont plus référencés :

```powershell
Remove-Item src/usage -Recurse -Force
Remove-Item src/providers -Recurse -Force
Remove-Item src/tokens -Recurse -Force
Remove-Item src/activity -Recurse -Force
Remove-Item src/animation/WidgetQuotaGraphAnimation.* -Force
Remove-Item src/animation/WidgetRollingNumberAnimation.* -Force
```

Supprimer également les fichiers `WidgetGraph*`, `WidgetRenderQuotaGraph*`, `WidgetRenderToken*`, `WidgetRenderKpiSummary*`, `WidgetRenderUsage*` et leurs contrôleurs seulement lorsqu’ils ne sont plus présents dans `CMakeLists.txt` et qu’aucun include restant ne les référence.

- [ ] **Step 9: Valider**

```powershell
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

Lancer `build\CodexDeck.exe` et vérifier : fenêtre redimensionnable, titre Codex Deck, DPI correct, fermeture propre.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "refactor: replace quota widget with Codex Deck shell"
```

---

### Task 4: Introduire le pipeline DirectComposition natif

**Files:**
- Create: `src/graphics/GraphicsError.h`
- Create: `src/graphics/CompositionHost.h`
- Create: `src/graphics/CompositionHost.cpp`
- Modify: `src/rendering/DeckRenderer.h`
- Modify: `src/rendering/DeckRenderer.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/CompositionSmokeTests.cpp`

**Interfaces:**
- Produces: `CompositionHost::Initialize(HWND) -> std::expected<void, GraphicsError>`.
- Produces: `CompositionHost::Resize(UINT width, UINT height, float dpi)`.
- Produces: `CompositionHost::BeginDraw() -> std::expected<ID2D1DeviceContext*, GraphicsError>`.
- Produces: `CompositionHost::EndDraw() -> std::expected<void, GraphicsError>`.

- [ ] **Step 1: Écrire un smoke test avec une fenêtre Win32 cachée**

Le test crée une classe/fenêtre top-level non affichée, initialise `CompositionHost`, appelle `BeginDraw`, `Clear`, `EndDraw`, puis détruit la fenêtre. Retour non zéro sur chaque échec.

- [ ] **Step 2: Vérifier l’échec**

```powershell
cmake --build build
ctest --test-dir build -R CompositionSmokeTests --output-on-failure
```

Expected: FAIL car `CompositionHost` n’existe pas.

- [ ] **Step 3: Définir les erreurs graphiques**

`GraphicsError.h` :

```cpp
#pragma once
#include <string>

enum class GraphicsErrorCode {
    D3DDeviceCreation,
    D2DDeviceCreation,
    SwapChainCreation,
    CompositionDeviceCreation,
    CompositionTargetCreation,
    SurfaceBinding,
    PresentFailed,
};

struct GraphicsError {
    GraphicsErrorCode code;
    long hresult;
    std::wstring message;
};
```

- [ ] **Step 4: Implémenter `CompositionHost`**

Le module possède : D3D11 device avec `D3D11_CREATE_DEVICE_BGRA_SUPPORT`, DXGI device/factory, swap chain `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` créé via `CreateSwapChainForComposition`, factory/device/context Direct2D 1.1, `IDCompositionDevice`, target HWND et root visual. Le root visual reçoit le swap chain comme contenu puis `Commit()`.

- [ ] **Step 5: Brancher `DeckRenderer` sur le contexte Direct2D de composition**

`DeckRenderer` ne possède plus un `ID2D1HwndRenderTarget`. Il possède un `CompositionHost` et crée ses brosses/formats autour du `ID2D1DeviceContext` exposé par `BeginDraw()`.

- [ ] **Step 6: Lier les bibliothèques nécessaires**

Dans `CMakeLists.txt` :

```cmake
target_link_libraries(CodexDeck PRIVATE
    d2d1 dwrite d3d11 dxgi dcomp dwmapi user32 shell32 gdi32 uxtheme advapi32 sqlite3 nlohmann_json::nlohmann_json
)
```

Ajouter `dcomp` au smoke test.

- [ ] **Step 7: Valider test + application**

```powershell
cmake --build build
ctest --test-dir build -R "CompositionSmokeTests|DeckWindowGeometryTests|SingleInstanceTests" --output-on-failure
```

Puis redimensionner manuellement la fenêtre rapidement : aucun flash blanc ni artefact de resize.

- [ ] **Step 8: Commit**

```bash
git add src/graphics src/rendering CMakeLists.txt tests/CompositionSmokeTests.cpp
git commit -m "feat: add DirectComposition rendering host"
```

---

### Task 5: Ajouter le thème System / Light / Dark et finaliser la fondation

**Files:**
- Create: `src/theme/Theme.h`
- Create: `src/theme/Theme.cpp`
- Create: `src/theme/ThemePalette.h`
- Modify: `src/settings/DeckSettings.h`
- Modify: `src/app/DeckApp.cpp`
- Modify: `src/rendering/DeckRenderer.cpp`
- Test: `tests/ThemeTests.cpp`
- Create: `CMakePresets.json`

**Interfaces:**
- Produces: `enum class ThemeMode { System, Light, Dark };`
- Produces: `ResolveTheme(ThemeMode requested, bool system_dark) -> ResolvedTheme`.
- Produces: `PaletteForTheme(ResolvedTheme) -> ThemePalette`.

- [ ] **Step 1: Écrire les tests de résolution**

```cpp
if (ResolveTheme(ThemeMode::System, true) != ResolvedTheme::Dark) return 1;
if (ResolveTheme(ThemeMode::System, false) != ResolvedTheme::Light) return 2;
if (ResolveTheme(ThemeMode::Dark, false) != ResolvedTheme::Dark) return 3;
if (ResolveTheme(ThemeMode::Light, true) != ResolvedTheme::Light) return 4;
```

- [ ] **Step 2: Vérifier l’échec**

```powershell
cmake --build build
ctest --test-dir build -R ThemeTests --output-on-failure
```

- [ ] **Step 3: Implémenter le contrat thème**

`Theme.h` contient les deux enums et :

```cpp
ResolvedTheme ResolveTheme(ThemeMode requested, bool system_dark);
bool IsSystemDarkTheme();
void ApplySystemWindowTheme(HWND hwnd, ResolvedTheme theme);
```

`IsSystemDarkTheme()` lit `AppsUseLightTheme` sous `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize`, avec repli clair si la valeur n’est pas disponible.

- [ ] **Step 4: Définir une palette stable**

`ThemePalette` contient au minimum `window_background`, `surface`, `surface_hover`, `border`, `text`, `text_muted`, `accent`, `success`, `warning`, `error` en `D2D1_COLOR_F`.

- [ ] **Step 5: Réagir aux changements système**

Dans `DeckApp`, traiter `WM_SETTINGCHANGE` et `WM_THEMECHANGED`; si `ThemeMode::System`, recalculer le thème, appliquer DWM et invalider le renderer. `Light` et `Dark` ignorent les bascules système.

- [ ] **Step 6: Ajouter les presets Debug/Release reproductibles**

`CMakePresets.json` doit fournir `debug` et `release` en x64, avec Ninja si disponible dans l’environnement de développement déjà utilisé par Codex Glass.

- [ ] **Step 7: Validation finale du plan**

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

Vérification manuelle : les trois modes System/Light/Dark changent fond, texte et attribut DWM sans redémarrer.

- [ ] **Step 8: Commit**

```bash
git add src/theme src/settings src/app src/rendering tests CMakeLists.txt CMakePresets.json
git commit -m "feat: establish Codex Deck native foundation"
```

## Critère de sortie du plan

Le dépôt ne contient plus le métier quotas/tokens de Codex Glass dans l’exécutable. `CodexDeck.exe` est une coquille Windows 11 x64 C++23, Direct2D/DirectWrite/DirectComposition, redimensionnable, DPI-aware, avec System/Light/Dark, et les builds/tests Debug + Release passent.

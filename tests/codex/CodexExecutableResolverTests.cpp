// ============================================================================
// Codex Deck - Tests de resolution de l'executable Codex
// ----------------------------------------------------------------------------
// Ce fichier valide l'ordre de priorite sans dependre du PATH reel de la
// machine de developpement.
// ============================================================================

#include "codex/CodexExecutableResolver.h"

#include <cstdlib>
#include <filesystem>
#include <optional>

namespace {

// ----------------------------------------------------------------------------
// Definit une variable d'environnement de test.
//
// Parametres :
// - name : nom de la variable.
// - value : valeur a appliquer.
// ----------------------------------------------------------------------------
void SetEnv(const wchar_t* name, const wchar_t* value) {
    _wputenv_s(name, value);
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie que le chemin explicite prime sur toute autre source.
//
// Retour :
// - zero si le resolver conserve le chemin demande.
// ----------------------------------------------------------------------------
int TestOverridePriority() {
    SetEnv(L"CODEX_DECK_CODEX_PATH", L"C:\\env\\codex.exe");
    const auto resolved = ResolveCodexExecutable(
        std::filesystem::path(L"C:\\explicit\\codex.exe"),
        [](std::wstring_view) -> std::optional<std::filesystem::path> {
            return std::filesystem::path(L"C:\\path\\codex.exe");
        }
    );
    SetEnv(L"CODEX_DECK_CODEX_PATH", L"");
    if (!resolved) {
        return 1;
    }
    if (resolved->application_path != std::filesystem::path(L"C:\\explicit\\codex.exe")) {
        return 2;
    }
    if (resolved->command_line.find(L"app-server") == std::wstring::npos) {
        return 3;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Verifie que la variable d'environnement prime sur la recherche PATH.
//
// Retour :
// - zero si la variable est retenue.
// ----------------------------------------------------------------------------
int TestEnvironmentPriority() {
    SetEnv(L"CODEX_DECK_CODEX_PATH", L"C:\\env\\codex.exe");
    const auto resolved = ResolveCodexExecutable(
        std::nullopt,
        [](std::wstring_view) -> std::optional<std::filesystem::path> {
            return std::filesystem::path(L"C:\\path\\codex.exe");
        }
    );
    SetEnv(L"CODEX_DECK_CODEX_PATH", L"");
    if (!resolved) {
        return 10;
    }
    return resolved->application_path == std::filesystem::path(L"C:\\env\\codex.exe") ? 0 : 11;
}

// ----------------------------------------------------------------------------
// Verifie que codex.exe est cherche avant codex.cmd.
//
// Retour :
// - zero si l'ordre de recherche est correct.
// ----------------------------------------------------------------------------
int TestSearchPriority() {
    int calls = 0;
    const auto resolved = ResolveCodexExecutable(
        std::nullopt,
        [&calls](std::wstring_view name) -> std::optional<std::filesystem::path> {
            ++calls;
            if (name == L"codex.exe") {
                return std::filesystem::path(L"C:\\path\\codex.exe");
            }
            return std::filesystem::path(L"C:\\path\\codex.cmd");
        }
    );
    if (!resolved) {
        return 20;
    }
    if (calls != 1) {
        return 21;
    }
    return resolved->application_path == std::filesystem::path(L"C:\\path\\codex.exe") ? 0 : 22;
}

// ----------------------------------------------------------------------------
// Verifie la construction de lancement pour un wrapper cmd.
//
// Retour :
// - zero si ComSpec est utilise comme application.
// ----------------------------------------------------------------------------
int TestCmdLaunchSpec() {
    SetEnv(L"ComSpec", L"C:\\Windows\\System32\\cmd.exe");
    const auto resolved = ResolveCodexExecutable(
        std::nullopt,
        [](std::wstring_view name) -> std::optional<std::filesystem::path> {
            if (name == L"codex.cmd") {
                return std::filesystem::path(L"C:\\tools\\codex.cmd");
            }
            return std::nullopt;
        }
    );
    if (!resolved) {
        return 30;
    }
    if (resolved->application_path != std::filesystem::path(L"C:\\Windows\\System32\\cmd.exe")) {
        return 31;
    }
    if (resolved->command_line.find(L"/d /s /c") == std::wstring::npos) {
        return 32;
    }
    if (resolved->command_line.find(L"codex.cmd") == std::wstring::npos) {
        return 33;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Execute les tests du resolver.
//
// Retour :
// - zero si toutes les priorites sont respectees.
// ----------------------------------------------------------------------------
int main() {
    if (const int result = TestOverridePriority(); result != 0) {
        return result;
    }
    if (const int result = TestEnvironmentPriority(); result != 0) {
        return result;
    }
    if (const int result = TestSearchPriority(); result != 0) {
        return result;
    }
    if (const int result = TestCmdLaunchSpec(); result != 0) {
        return result;
    }
    return 0;
}

// ============================================================================
// Codex Deck - Implementation de la resolution Codex
// ----------------------------------------------------------------------------
// Ce fichier applique l'ordre explicite, variable d'environnement, codex.exe,
// codex.cmd et refuse les wrappers PowerShell implicites.
// ============================================================================

#include "CodexExecutableResolver.h"

#include <windows.h>

#include <array>

namespace {

// Nom de la variable d'environnement permettant un override utilisateur.
constexpr wchar_t kCodexPathEnvironmentVariable[] = L"CODEX_DECK_CODEX_PATH";

// Nom du shell Windows utilise uniquement pour les wrappers .cmd.
constexpr wchar_t kComSpecEnvironmentVariable[] = L"ComSpec";

// ----------------------------------------------------------------------------
// Construit une erreur indiquant que l'executable est absent.
//
// Retour :
// - erreur ExecutableNotFound.
// ----------------------------------------------------------------------------
CodexError NotFoundError() {
    return CodexError{CodexErrorCode::ExecutableNotFound, L"Impossible de trouver codex.exe ou codex.cmd"};
}

// ----------------------------------------------------------------------------
// Lit une variable d'environnement Windows.
//
// Parametres :
// - name : nom de la variable.
//
// Retour :
// - valeur non vide ou std::nullopt.
// ----------------------------------------------------------------------------
std::optional<std::wstring> ReadEnvironment(const wchar_t* name) {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD count = GetEnvironmentVariableW(name, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (count == 0) {
        return std::nullopt;
    }
    if (count >= buffer.size()) {
        std::wstring large(count, L'\0');
        const DWORD large_count = GetEnvironmentVariableW(name, large.data(), count);
        if (large_count == 0 || large_count >= count) {
            return std::nullopt;
        }
        large.resize(large_count);
        return large.empty() ? std::nullopt : std::optional<std::wstring>{large};
    }
    std::wstring value(buffer.data(), count);
    return value.empty() ? std::nullopt : std::optional<std::wstring>{value};
}

// ----------------------------------------------------------------------------
// Echappe un chemin pour une ligne de commande Windows.
//
// Parametres :
// - path : chemin a citer.
//
// Retour :
// - chemin entoure de guillemets.
// ----------------------------------------------------------------------------
std::wstring QuotePath(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}

// ----------------------------------------------------------------------------
// Construit la specification pour un executable direct.
//
// Parametres :
// - executable : chemin vers codex.exe.
//
// Retour :
// - specification de lancement directe.
// ----------------------------------------------------------------------------
CodexLaunchSpec ExeSpec(const std::filesystem::path& executable) {
    return CodexLaunchSpec{executable, QuotePath(executable) + L" app-server"};
}

// ----------------------------------------------------------------------------
// Construit la specification pour un wrapper cmd.
//
// Parametres :
// - script : chemin vers codex.cmd.
//
// Retour :
// - specification utilisant ComSpec.
// ----------------------------------------------------------------------------
std::expected<CodexLaunchSpec, CodexError> CmdSpec(const std::filesystem::path& script) {
    const auto comspec = ReadEnvironment(kComSpecEnvironmentVariable);
    if (!comspec) {
        return std::unexpected(CodexError{
            CodexErrorCode::ExecutableNotFound,
            L"ComSpec est indisponible pour lancer codex.cmd",
        });
    }
    return CodexLaunchSpec{
        std::filesystem::path(*comspec),
        L"/d /s /c \"\"" + script.wstring() + L"\" app-server\"",
    };
}

// ----------------------------------------------------------------------------
// Construit la specification selon l'extension du chemin.
//
// Parametres :
// - path : executable ou wrapper resolu.
//
// Retour :
// - specification de lancement ou erreur.
// ----------------------------------------------------------------------------
std::expected<CodexLaunchSpec, CodexError> SpecForPath(const std::filesystem::path& path) {
    const std::wstring extension = path.extension().wstring();
    if (_wcsicmp(extension.c_str(), L".cmd") == 0) {
        return CmdSpec(path);
    }
    return ExeSpec(path);
}

// ----------------------------------------------------------------------------
// Cherche un executable avec SearchPathW.
//
// Parametres :
// - name : nom a chercher.
//
// Retour :
// - chemin trouve ou std::nullopt.
// ----------------------------------------------------------------------------
std::optional<std::filesystem::path> SearchPathLookup(std::wstring_view name) {
    std::array<wchar_t, MAX_PATH> buffer{};
    const std::wstring file_name(name);
    const DWORD count = SearchPathW(
        nullptr,
        file_name.c_str(),
        nullptr,
        static_cast<DWORD>(buffer.size()),
        buffer.data(),
        nullptr
    );
    if (count == 0) {
        return std::nullopt;
    }
    if (count < buffer.size()) {
        return std::filesystem::path(buffer.data());
    }
    std::wstring large(count + 1, L'\0');
    const DWORD large_count = SearchPathW(
        nullptr,
        file_name.c_str(),
        nullptr,
        static_cast<DWORD>(large.size()),
        large.data(),
        nullptr
    );
    if (large_count == 0 || large_count >= large.size()) {
        return std::nullopt;
    }
    large.resize(large_count);
    return std::filesystem::path(large);
}

}  // namespace

// ----------------------------------------------------------------------------
// Resolut l'executable Codex avec une recherche injectable.
// ----------------------------------------------------------------------------
std::expected<CodexLaunchSpec, CodexError> ResolveCodexExecutable(
    const std::optional<std::filesystem::path>& override_path,
    const PathLookup& lookup
) {
    if (override_path && !override_path->empty()) {
        return SpecForPath(*override_path);
    }

    if (const auto env_path = ReadEnvironment(kCodexPathEnvironmentVariable)) {
        return SpecForPath(std::filesystem::path(*env_path));
    }

    if (const auto exe = lookup(L"codex.exe")) {
        return SpecForPath(*exe);
    }
    if (const auto cmd = lookup(L"codex.cmd")) {
        return SpecForPath(*cmd);
    }

    return std::unexpected(NotFoundError());
}

// ----------------------------------------------------------------------------
// Resolut l'executable Codex avec SearchPathW.
// ----------------------------------------------------------------------------
std::expected<CodexLaunchSpec, CodexError> ResolveCodexExecutable(
    const std::optional<std::filesystem::path>& override_path
) {
    return ResolveCodexExecutable(override_path, SearchPathLookup);
}

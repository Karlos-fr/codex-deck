// ============================================================================
// Codex Deck - Implementation du probe Git
// ----------------------------------------------------------------------------
// Ce fichier lance uniquement les commandes Git strictement necessaires a
// l'identification d'un projet logique.
// ============================================================================

#include "GitProjectProbe.h"

#include <windows.h>

#include <array>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Supprime les blancs finaux d'une sortie de commande.
//
// Parametres :
// - value : chaine source.
//
// Retour :
// - chaine nettoyee.
// ----------------------------------------------------------------------------
std::string Trim(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

// ----------------------------------------------------------------------------
// Convertit une chaine large en UTF-8 systeme simplifie.
//
// Parametres :
// - value : chaine Windows.
//
// Retour :
// - chaine UTF-8.
// ----------------------------------------------------------------------------
std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size > 0 ? size - 1 : 0), '\0');
    if (!result.empty()) {
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    }
    return result;
}

// ----------------------------------------------------------------------------
// Execute une commande git courte et capture stdout.
//
// Parametres :
// - command_line : ligne de commande complete.
//
// Retour :
// - sortie stdout/stderr ou std::nullopt si le process echoue.
// ----------------------------------------------------------------------------
std::optional<std::string> RunCaptured(std::wstring command_line) {
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &attributes, 0)) {
        return std::nullopt;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;

    PROCESS_INFORMATION info{};
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');
    const BOOL created = CreateProcessW(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &info
    );
    CloseHandle(write_pipe);
    if (!created) {
        CloseHandle(read_pipe);
        return std::nullopt;
    }

    std::string output;
    std::array<char, 256> buffer{};
    DWORD read = 0;
    while (ReadFile(read_pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0) {
        output.append(buffer.data(), read);
    }
    CloseHandle(read_pipe);
    WaitForSingleObject(info.hProcess, 2000);
    DWORD exit_code = 1;
    GetExitCodeProcess(info.hProcess, &exit_code);
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    return exit_code == 0 ? std::optional<std::string>{Trim(output)} : std::nullopt;
}

// ----------------------------------------------------------------------------
// Cite un chemin pour cmd/CreateProcess.
//
// Parametres :
// - path : chemin source.
//
// Retour :
// - chemin entre guillemets.
// ----------------------------------------------------------------------------
std::wstring Quote(const std::filesystem::path& path) {
    return L"\"" + path.wstring() + L"\"";
}

}  // namespace

// ----------------------------------------------------------------------------
// Inspecte un cwd via git rev-parse puis remote get-url.
// ----------------------------------------------------------------------------
std::expected<std::optional<GitProjectIdentity>, ProjectDetectionError> GitProjectProbe::Inspect(const std::filesystem::path& cwd) {
    const std::wstring root_command = L"git -C " + Quote(cwd) + L" rev-parse --show-toplevel";
    const auto root = RunCaptured(root_command);
    if (!root || root->empty()) {
        return std::optional<GitProjectIdentity>{};
    }

    GitProjectIdentity identity{};
    identity.root = std::filesystem::path(*root);
    const std::wstring remote_command = L"git -C " + Quote(identity.root) + L" remote get-url origin";
    if (const auto remote = RunCaptured(remote_command); remote && !remote->empty()) {
        identity.origin_remote = *remote;
    }
    return std::optional<GitProjectIdentity>{identity};
}

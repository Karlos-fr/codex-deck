// ============================================================================
// Codex Deck - Tests du processus app-server
// ----------------------------------------------------------------------------
// Ce fichier valide le demarrage supervise d'un faux serveur JSON-RPC via pipes
// anonymes Win32.
// ============================================================================

#include "codex/CodexProcess.h"

#include <windows.h>

#include <filesystem>
#include <string>

namespace {

// ----------------------------------------------------------------------------
// Lit une ligne UTF-8 depuis un handle Win32.
//
// Parametres :
// - handle : pipe stdout du processus enfant.
//
// Retour :
// - ligne recue sans le saut de ligne final.
// ----------------------------------------------------------------------------
std::string ReadLine(HANDLE handle) {
    std::string line;
    char ch = '\0';
    DWORD read = 0;
    while (ReadFile(handle, &ch, 1, &read, nullptr) && read == 1) {
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
    return line;
}

// ----------------------------------------------------------------------------
// Ecrit une ligne UTF-8 complete dans un handle Win32.
//
// Parametres :
// - handle : pipe stdin du processus enfant.
// - line : contenu sans saut de ligne obligatoire.
//
// Retour :
// - true si tous les octets ont ete ecrits.
// ----------------------------------------------------------------------------
bool WriteLine(HANDLE handle, const std::string& line) {
    const std::string payload = line + "\n";
    DWORD written = 0;
    return WriteFile(handle, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr)
        && written == payload.size();
}

}  // namespace

// ----------------------------------------------------------------------------
// Verifie le cycle de vie minimal d'un app-server enfant.
//
// Retour :
// - zero lorsque le process demarre, repond puis s'arrete.
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }

    CodexLaunchSpec spec{};
    spec.application_path = std::filesystem::path(argv[1]);
    spec.command_line = L"\"" + spec.application_path.wstring() + L"\" app-server";

    CodexProcess process;
    const auto started = process.Start(spec);
    if (!started) {
        return 2;
    }
    if (!process.IsRunning()) {
        return 3;
    }

    HANDLE stdout_read = process.TakeStdoutReadHandle();
    if (stdout_read == nullptr || stdout_read == INVALID_HANDLE_VALUE) {
        return 4;
    }
    if (!WriteLine(process.StdinWriteHandle(), R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})")) {
        CloseHandle(stdout_read);
        return 5;
    }
    const std::string response = ReadLine(stdout_read);
    CloseHandle(stdout_read);
    if (response.find(R"("id":1)") == std::string::npos || response.find(R"("result")") == std::string::npos) {
        return 6;
    }

    process.Stop();
    return process.IsRunning() ? 7 : 0;
}

// ============================================================================
// Codex Glass - Implementation du lancement avec Windows
// ----------------------------------------------------------------------------
// Ce fichier gere la valeur CodexGlass de la cle Run de l'utilisateur courant.
// Il n'ecrit jamais dans le fichier INI de l'application.
// ============================================================================

#include "WindowsStartup.h"

#include <windows.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace {

#if defined(CODEX_GLASS_STARTUP_TEST_REGISTRY)

// Cle utilisateur isolee utilisee uniquement par le binaire de test.
constexpr wchar_t kRunKeyPath[] = L"Software\\CodexGlass\\Tests\\Run";

// Nom de valeur isole utilise uniquement par le binaire de test.
constexpr wchar_t kRunValueName[] = L"CodexGlassStartupTests";

#else

// Cle utilisateur standard des applications lancees a l'ouverture de session.
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Nom de la valeur de demarrage reservee a l'application.
constexpr wchar_t kRunValueName[] = L"CodexGlass";

#endif

// Capacite maximale prise en charge par Windows pour le chemin d'un module.
constexpr DWORD kModulePathCapacity = 32768;

// ----------------------------------------------------------------------------
// Construit la commande citee qui relance l'executable courant.
//
// Retour :
// - commande complete, ou une chaine vide si le chemin est indisponible.
// ----------------------------------------------------------------------------
std::wstring BuildStartupCommand() {
    std::array<wchar_t, kModulePathCapacity> module_path{};
    const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), kModulePathCapacity);
    if (length == 0 || length >= kModulePathCapacity) {
        return {};
    }
    return L"\"" + std::wstring(module_path.data(), length) + L"\"";
}

// ----------------------------------------------------------------------------
// Lit la commande actuellement enregistree dans la valeur Run.
//
// Retour :
// - commande presente de type REG_SZ, ou aucune valeur si elle est absente ou
//   illisible.
// ----------------------------------------------------------------------------
std::optional<std::wstring> ReadStartupCommand() {
    DWORD byte_count = 0;
    const LSTATUS size_status = RegGetValueW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        kRunValueName,
        RRF_RT_REG_SZ,
        nullptr,
        nullptr,
        &byte_count);
    if (size_status != ERROR_SUCCESS || byte_count < sizeof(wchar_t)) {
        return std::nullopt;
    }

    std::vector<wchar_t> value((byte_count / sizeof(wchar_t)) + 1, L'\0');
    const LSTATUS read_status = RegGetValueW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        kRunValueName,
        RRF_RT_REG_SZ,
        nullptr,
        value.data(),
        &byte_count);
    if (read_status != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::wstring(value.data());
}

}  // namespace anonyme

// ----------------------------------------------------------------------------
// Active ou desactive le lancement de Codex Glass avec la session Windows.
//
// Parametres :
// - enabled : true pour enregistrer l'executable courant, false pour le retirer.
//
// Retour :
// - true si l'etat demande a ete applique ou etait deja absent.
// ----------------------------------------------------------------------------
bool SetStartWithWindowsEnabled(bool enabled) {
    HKEY run_key = nullptr;
    const LSTATUS open_status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &run_key,
        nullptr);
    if (open_status != ERROR_SUCCESS) {
        return false;
    }

    LSTATUS operation_status = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring command = BuildStartupCommand();
        if (command.empty()) {
            RegCloseKey(run_key);
            return false;
        }
        operation_status = RegSetValueExW(
            run_key,
            kRunValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        operation_status = RegDeleteValueW(run_key, kRunValueName);
        if (operation_status == ERROR_FILE_NOT_FOUND) {
            operation_status = ERROR_SUCCESS;
        }
    }

    RegCloseKey(run_key);
    return operation_status == ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
// Indique si la commande enregistree pointe vers l'executable courant.
//
// Retour :
// - true si la valeur utilisateur Run contient la commande attendue.
// ----------------------------------------------------------------------------
bool IsStartWithWindowsEnabled() {
    const std::wstring expected_command = BuildStartupCommand();
    if (expected_command.empty()) {
        return false;
    }
    const auto registered_command = ReadStartupCommand();
    return registered_command && *registered_command == expected_command;
}

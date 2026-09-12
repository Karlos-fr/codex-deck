// ============================================================================
// Codex Deck - Tests du lancement avec Windows
// ----------------------------------------------------------------------------
// Ce fichier verifie les operations de registre dans une cle utilisateur de
// test isolee. Il ne cree aucune entree dans la veritable cle Windows Run.
// ============================================================================

#include "startup/WindowsStartup.h"

#include <windows.h>

#include <array>
#include <string>

namespace {

// Cle isolee partagee avec WindowsStartup.cpp en compilation de test.
constexpr wchar_t kTestRunKeyPath[] = L"Software\\CodexDeck\\Tests\\Run";

// Nom de valeur isole partage avec WindowsStartup.cpp en compilation de test.
constexpr wchar_t kTestRunValueName[] = L"CodexDeckStartupTests";

// Capacite maximale prise en charge pour le chemin du binaire de test.
constexpr DWORD kModulePathCapacity = 32768;

// Code de sortie utilise lorsque l'etat initial ne peut pas etre nettoye.
constexpr int kInitialCleanupFailure = 1;

// Code de sortie utilise lorsque l'activation echoue.
constexpr int kEnableFailure = 2;

// Code de sortie utilise lorsque la commande enregistree est incorrecte.
constexpr int kCommandFailure = 3;

// Code de sortie utilise lorsque la desactivation echoue.
constexpr int kDisableFailure = 4;

// ----------------------------------------------------------------------------
// Construit la commande attendue pour le binaire de test courant.
//
// Retour :
// - chemin complet cite, ou une chaine vide en cas d'erreur Windows.
// ----------------------------------------------------------------------------
std::wstring BuildExpectedCommand() {
    std::array<wchar_t, kModulePathCapacity> module_path{};
    const DWORD length = GetModuleFileNameW(nullptr, module_path.data(), kModulePathCapacity);
    if (length == 0 || length >= kModulePathCapacity) {
        return {};
    }
    return L"\"" + std::wstring(module_path.data(), length) + L"\"";
}

// ----------------------------------------------------------------------------
// Lit directement la valeur de test pour verifier son contenu exact.
//
// Retour :
// - contenu REG_SZ, ou une chaine vide si la valeur est absente ou illisible.
// ----------------------------------------------------------------------------
std::wstring ReadTestCommand() {
    std::array<wchar_t, kModulePathCapacity> command{};
    DWORD byte_count = static_cast<DWORD>(command.size() * sizeof(wchar_t));
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kTestRunKeyPath,
        kTestRunValueName,
        RRF_RT_REG_SZ,
        nullptr,
        command.data(),
        &byte_count);
    return status == ERROR_SUCCESS ? std::wstring(command.data()) : std::wstring{};
}

// ----------------------------------------------------------------------------
// Supprime les cles de test devenues vides.
//
// Effet de bord :
// - retire uniquement l'arborescence HKCU reservee au test de demarrage.
// ----------------------------------------------------------------------------
void CleanupTestKeys() {
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\CodexDeck\\Tests\\Run");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\CodexDeck\\Tests");
}

}  // namespace anonyme

// ----------------------------------------------------------------------------
// Execute les scenarios d'activation et de desactivation dans la cle isolee.
//
// Retour :
// - zero si tous les scenarios reussissent, sinon le code du scenario fautif.
//
// Effet de bord :
// - cree puis supprime une valeur sous HKCU\Software\CodexDeck\Tests.
// ----------------------------------------------------------------------------
int main() {
    if (!SetStartWithWindowsEnabled(false)) {
        CleanupTestKeys();
        return kInitialCleanupFailure;
    }
    if (IsStartWithWindowsEnabled()) {
        CleanupTestKeys();
        return kInitialCleanupFailure;
    }

    if (!SetStartWithWindowsEnabled(true) || !IsStartWithWindowsEnabled()) {
        SetStartWithWindowsEnabled(false);
        CleanupTestKeys();
        return kEnableFailure;
    }

    const std::wstring expected_command = BuildExpectedCommand();
    if (expected_command.empty() || ReadTestCommand() != expected_command) {
        SetStartWithWindowsEnabled(false);
        CleanupTestKeys();
        return kCommandFailure;
    }

    if (!SetStartWithWindowsEnabled(false) || IsStartWithWindowsEnabled()) {
        CleanupTestKeys();
        return kDisableFailure;
    }

    CleanupTestKeys();
    return 0;
}

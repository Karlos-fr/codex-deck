// ============================================================================
// Codex Glass - Implementation des actions shell Windows
// ----------------------------------------------------------------------------
// Ce fichier regroupe les appels au shell qui lancent une action externe a
// l'application. Il garde ces details Win32 hors de WidgetApp et du menu.
// ============================================================================

#include "WidgetShellActions.h"

#include <shellapi.h>

namespace {

// URL publique du depot GitHub du projet.
constexpr wchar_t kCodexGlassRepositoryUrl[] = L"https://github.com/Karlos-fr/codex-glass/";

} // namespace

// ----------------------------------------------------------------------------
// Ouvre le depot GitHub de Codex Glass dans le navigateur par defaut.
//
// Parametres :
// - owner : fenetre proprietaire eventuelle de l'action shell.
//
// Retour :
// - true si Windows accepte la demande d'ouverture.
// - false sinon.
// ----------------------------------------------------------------------------
bool OpenCodexGlassRepository(HWND owner) {
    const HINSTANCE result = ShellExecuteW(owner, L"open", kCodexGlassRepositoryUrl, nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

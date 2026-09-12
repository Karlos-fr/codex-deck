// ============================================================================
// Codex Deck - Point d'entree Win32
// ----------------------------------------------------------------------------
// Ce fichier garde uniquement le point d'entree Unicode et delegue le cycle de
// vie complet de l'application a DeckApp.
// ============================================================================

#include "app/DeckApp.h"
#include "win32/SingleInstance.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Active la prise en charge DPI par moniteur avant toute creation de fenetre.
// ----------------------------------------------------------------------------
void EnablePerMonitorDpiAwareness() {
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
}

// ----------------------------------------------------------------------------
// Point d'entree Unicode d'une application Windows sans console.
//
// Parametres :
// - instance : handle de l'instance courante de l'application.
// - command_show : mode d'affichage initial demande par Windows.
//
// Retour :
// - code de sortie du processus.
// ----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int command_show) {
    SingleInstance single_instance;
    if (single_instance.status() == SingleInstanceStatus::AlreadyRunning) {
        return 0;
    }
    if (single_instance.status() == SingleInstanceStatus::Error) {
        return 1;
    }

    EnablePerMonitorDpiAwareness();
    DeckApp app;
    return app.Run(instance, command_show);
}

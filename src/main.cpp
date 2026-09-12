// ============================================================================
// Codex Glass - Point d'entree Win32
// ----------------------------------------------------------------------------
// Ce fichier garde uniquement le point d'entree Unicode et delegue le cycle de
// vie complet du widget a WidgetApp.
// ============================================================================

#include "app/WidgetApp.h"
#include "win32/SingleInstance.h"

#include <windows.h>

#include <memory>

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
    const std::unique_ptr<WidgetApp> app = std::make_unique<WidgetApp>();
    return app->Run(instance, command_show);
}

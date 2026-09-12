// ============================================================================
// Codex Glass - Implementation des messages shell Windows
// ----------------------------------------------------------------------------
// Ce fichier isole les identifiants et enregistrements de messages propres au
// shell Windows afin que le reste de l'application ne manipule pas directement
// ces details Win32.
// ============================================================================

#include "WidgetShellMessages.h"

namespace {

// Message prive reserve au callback de l'icone de notification.
constexpr UINT kTrayIconMessage = WM_APP + 1;

} // namespace

// ----------------------------------------------------------------------------
// Retourne le message prive envoye par l'icone de notification a la fenetre.
//
// Retour :
// - identifiant Win32 du message tray.
// ----------------------------------------------------------------------------
UINT TrayIconMessage() {
    return kTrayIconMessage;
}

// ----------------------------------------------------------------------------
// Retourne le message diffuse par Windows quand la barre des taches est creee.
//
// Retour :
// - identifiant Win32 du message TaskbarCreated.
// ----------------------------------------------------------------------------
UINT TaskbarCreatedMessage() {
    static const UINT taskbar_created_message = RegisterWindowMessageW(L"TaskbarCreated");
    return taskbar_created_message;
}

// ----------------------------------------------------------------------------
// Indique si un message Win32 correspond a la recreation de la barre des taches.
//
// Parametres :
// - message : identifiant du message recu.
//
// Retour :
// - true si le message est TaskbarCreated.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsTaskbarCreatedMessage(UINT message) {
    return message == TaskbarCreatedMessage();
}

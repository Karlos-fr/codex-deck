// ============================================================================
// Codex Glass - Implementation de l'instance unique
// ----------------------------------------------------------------------------
// Ce fichier reserve un mutex Windows nomme pendant la vie du processus. Il ne
// connait ni la fenetre principale, ni l'icone de la zone de notification.
// ============================================================================

#include "SingleInstance.h"

namespace {

// Nom stable du mutex limite a la session Windows de l'utilisateur.
constexpr wchar_t kCodexGlassMutexName[] = L"Local\\Karlos-fr.CodexGlass.Instance";

}  // namespace anonyme

// ----------------------------------------------------------------------------
// Tente de reserver le nom de mutex utilise par Codex Glass.
//
// Effet de bord :
// - cree un mutex Windows nomme lorsque aucune instance ne le possede.
// ----------------------------------------------------------------------------
SingleInstance::SingleInstance()
    : SingleInstance(kCodexGlassMutexName) {
}

// ----------------------------------------------------------------------------
// Tente de reserver un nom de mutex explicite, notamment pour les tests.
//
// Parametres :
// - mutex_name : nom Windows complet du mutex a reserver.
//
// Effet de bord :
// - cree un mutex Windows nomme lorsque ce nom est disponible.
// ----------------------------------------------------------------------------
SingleInstance::SingleInstance(const wchar_t* mutex_name) {
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, mutex_name);
    if (mutex == nullptr) {
        return;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        status_ = SingleInstanceStatus::AlreadyRunning;
        return;
    }

    mutex_ = mutex;
    status_ = SingleInstanceStatus::Acquired;
}

// ----------------------------------------------------------------------------
// Libere le handle du mutex si cette instance l'a acquis.
//
// Effet de bord :
// - permet a un prochain processus de devenir l'instance principale.
// ----------------------------------------------------------------------------
SingleInstance::~SingleInstance() {
    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
    }
}

// ----------------------------------------------------------------------------
// Retourne le resultat de la tentative de reservation.
//
// Retour :
// - etat acquis, deja actif ou erreur systeme.
// ----------------------------------------------------------------------------
SingleInstanceStatus SingleInstance::status() const {
    return status_;
}

// ============================================================================
// Codex Glass - Tests de l'instance unique
// ----------------------------------------------------------------------------
// Ce fichier verifie la reservation d'un mutex Windows isole. Il n'utilise pas
// le nom de production et ne lance ni fenetre ni icone de notification.
// ============================================================================

#include "win32/SingleInstance.h"

#include <windows.h>

#include <string>

namespace {

// Code de sortie utilise lorsque la premiere reservation echoue.
constexpr int kFirstAcquisitionFailure = 1;

// Code de sortie utilise lorsque la seconde instance n'est pas detectee.
constexpr int kDuplicateDetectionFailure = 2;

// Code de sortie utilise lorsque le mutex reste reserve apres destruction.
constexpr int kReleaseFailure = 3;

// ----------------------------------------------------------------------------
// Construit un nom de mutex propre au processus de test courant.
//
// Retour :
// - nom place dans l'espace local de la session Windows.
// ----------------------------------------------------------------------------
std::wstring BuildTestMutexName() {
    return L"Local\\CodexGlass.SingleInstanceTests." + std::to_wstring(GetCurrentProcessId());
}

}  // namespace anonyme

// ----------------------------------------------------------------------------
// Verifie l'acquisition, la detection d'un doublon puis la liberation du mutex.
//
// Retour :
// - zero si tous les scenarios reussissent, sinon le code du scenario fautif.
//
// Effet de bord :
// - cree temporairement un mutex nomme limite au processus de test.
// ----------------------------------------------------------------------------
int main() {
    const std::wstring mutex_name = BuildTestMutexName();

    {
        SingleInstance first(mutex_name.c_str());
        if (first.status() != SingleInstanceStatus::Acquired) {
            return kFirstAcquisitionFailure;
        }

        SingleInstance second(mutex_name.c_str());
        if (second.status() != SingleInstanceStatus::AlreadyRunning) {
            return kDuplicateDetectionFailure;
        }
    }

    SingleInstance after_release(mutex_name.c_str());
    if (after_release.status() != SingleInstanceStatus::Acquired) {
        return kReleaseFailure;
    }

    return 0;
}

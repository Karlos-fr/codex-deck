// ============================================================================
// Codex Deck - Declaration de l'instance unique
// ----------------------------------------------------------------------------
// Ce fichier declare la possession d'un mutex Windows nomme. Il limite son
// role a la detection d'une autre instance et ne demarre pas l'application.
// ============================================================================

#pragma once

#include <windows.h>

// ----------------------------------------------------------------------------
// Retourne le nom de mutex utilise par l'instance principale.
//
// Retour :
// - nom Windows complet du mutex de production.
// ----------------------------------------------------------------------------
const wchar_t* DefaultSingleInstanceMutexName();

// Indique le resultat de la tentative de reservation de l'instance unique.
enum class SingleInstanceStatus {
    // Le processus courant conserve le mutex et peut demarrer l'application.
    Acquired,

    // Une autre instance conserve deja un handle vers le mutex nomme.
    AlreadyRunning,

    // Windows n'a pas pu creer ou ouvrir le mutex nomme.
    Error,
};

// ----------------------------------------------------------------------------
// Conserve le mutex nomme qui identifie le processus Codex Deck principal.
// ----------------------------------------------------------------------------
class SingleInstance {
public:
    // ------------------------------------------------------------------------
    // Tente de reserver le nom de mutex utilise par Codex Deck.
    //
    // Effet de bord :
    // - cree un mutex Windows nomme lorsque aucune instance ne le possede.
    // ------------------------------------------------------------------------
    SingleInstance();

    // ------------------------------------------------------------------------
    // Tente de reserver un nom de mutex explicite, notamment pour les tests.
    //
    // Parametres :
    // - mutex_name : nom Windows complet du mutex a reserver.
    //
    // Effet de bord :
    // - cree un mutex Windows nomme lorsque ce nom est disponible.
    // ------------------------------------------------------------------------
    explicit SingleInstance(const wchar_t* mutex_name);

    // ------------------------------------------------------------------------
    // Libere le handle du mutex si cette instance l'a acquis.
    //
    // Effet de bord :
    // - permet a un prochain processus de devenir l'instance principale.
    // ------------------------------------------------------------------------
    ~SingleInstance();

    // Interdit la copie afin de conserver un proprietaire unique du handle.
    SingleInstance(const SingleInstance&) = delete;

    // Interdit l'affectation par copie du handle de mutex.
    SingleInstance& operator=(const SingleInstance&) = delete;

    // ------------------------------------------------------------------------
    // Retourne le resultat de la tentative de reservation.
    //
    // Retour :
    // - etat acquis, deja actif ou erreur systeme.
    // ------------------------------------------------------------------------
    SingleInstanceStatus status() const;

private:
    // Handle conserve uniquement par l'instance qui a reserve le nom.
    HANDLE mutex_ = nullptr;

    // Resultat stable de la tentative effectuee par le constructeur.
    SingleInstanceStatus status_ = SingleInstanceStatus::Error;
};

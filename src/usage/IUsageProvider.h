// ============================================================================
// Codex Glass - Interface de fournisseur d'usage
// ----------------------------------------------------------------------------
// Ce fichier definit le contrat minimal permettant au widget de recuperer un
// releve d'usage sans connaitre la source concrete des donnees.
// ============================================================================

#pragma once

#include "UsageSnapshot.h"

// ----------------------------------------------------------------------------
// Contrat commun a toutes les sources de donnees d'usage Codex.
// ----------------------------------------------------------------------------
class IUsageProvider {
public:
    // ------------------------------------------------------------------------
    // Detruit proprement un fournisseur d'usage via son interface.
    // ------------------------------------------------------------------------
    virtual ~IUsageProvider() = default;

    // ------------------------------------------------------------------------
    // Retourne l'identifiant stable du fournisseur.
    // ------------------------------------------------------------------------
    virtual UsageProviderId ProviderId() const = 0;

    // ------------------------------------------------------------------------
    // Recupere le releve d'usage courant.
    //
    // Retour :
    // - snapshot d'usage utilisable par le rendu.
    // ------------------------------------------------------------------------
    virtual UsageSnapshot FetchUsage() = 0;

    // ------------------------------------------------------------------------
    // Annule les operations bloquantes actives du fournisseur.
    // ------------------------------------------------------------------------
    virtual void Cancel() = 0;
};

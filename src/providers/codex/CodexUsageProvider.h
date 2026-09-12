// ============================================================================
// Codex Glass - Provider d'usage Codex
// ----------------------------------------------------------------------------
// Ce fichier declare l'orchestration Codex entre lecture d'authentification,
// transport HTTP et parsing. Ces trois responsabilites restent separees.
// ============================================================================

#pragma once

#include "CodexUsageClient.h"
#include "../../usage/IUsageProvider.h"

// ----------------------------------------------------------------------------
// Fournit les donnees distantes du compte Codex actif.
// ----------------------------------------------------------------------------
class CodexUsageProvider final : public IUsageProvider {
public:
    // ------------------------------------------------------------------------
    // Retourne l'identifiant du provider Codex.
    // ------------------------------------------------------------------------
    UsageProviderId ProviderId() const override;

    // ------------------------------------------------------------------------
    // Recupere un snapshot distant complet.
    // ------------------------------------------------------------------------
    UsageSnapshot FetchUsage() override;

    // ------------------------------------------------------------------------
    // Annule l'appel HTTP actif.
    // ------------------------------------------------------------------------
    void Cancel() override;

private:
    // Client HTTP possede et annulable pendant toute la duree du provider.
    CodexUsageClient client_{};
};

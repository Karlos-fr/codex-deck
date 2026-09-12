// ============================================================================
// Codex Glass - Fabrique de fournisseurs d'usage
// ----------------------------------------------------------------------------
// Ce fichier instancie le provider actif. Codex est le seul provider enregistre
// actuellement, sans exposer son type au worker generique.
// ============================================================================

#include "UsageProviderFactory.h"

#include "../providers/codex/CodexUsageProvider.h"

// ----------------------------------------------------------------------------
// Cree le fournisseur d'usage actif.
//
// Retour :
// - fournisseur Codex partage.
// ----------------------------------------------------------------------------
std::shared_ptr<IUsageProvider> CreateUsageProvider() {
    return std::make_shared<CodexUsageProvider>();
}

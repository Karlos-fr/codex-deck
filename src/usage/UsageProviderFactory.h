// ============================================================================
// Codex Glass - Fabrique de fournisseurs d'usage
// ----------------------------------------------------------------------------
// Ce fichier declare la creation du provider configure. Il evite au worker de
// dependre d'une implementation LLM concrete.
// ============================================================================

#pragma once

#include "IUsageProvider.h"

#include <memory>

// ----------------------------------------------------------------------------
// Cree le fournisseur d'usage actif.
//
// Retour :
// - fournisseur partage pret a etre utilise par un worker.
// ----------------------------------------------------------------------------
std::shared_ptr<IUsageProvider> CreateUsageProvider();

// ============================================================================
// Codex Deck - Chemins applicatifs locaux
// ----------------------------------------------------------------------------
// Ce module centralise les chemins de stockage propres a Codex Deck et isole
// l'appel Shell Windows du reste de l'application.
// ============================================================================

#pragma once

#include "StorageError.h"

#include <expected>
#include <filesystem>

// ----------------------------------------------------------------------------
// Construit le chemin de base depuis une racine LocalAppData injectee.
//
// Parametres :
// - local_app_data_root : racine LocalAppData.
//
// Retour :
// - chemin complet de la base SQLite.
// ----------------------------------------------------------------------------
std::filesystem::path CodexDeckDatabasePath(const std::filesystem::path& local_app_data_root);

// ----------------------------------------------------------------------------
// Retourne le chemin de base Codex Deck dans LocalAppData.
//
// Retour :
// - chemin complet de la base SQLite, apres creation du dossier parent.
// ----------------------------------------------------------------------------
std::expected<std::filesystem::path, StorageError> CodexDeckDatabasePath();

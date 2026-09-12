// ============================================================================
// Codex Deck - Normalisation de chemins Windows
// ----------------------------------------------------------------------------
// Ce module produit une cle de comparaison insensible a la casse sans modifier
// la casse affichee stockee separement.
// ============================================================================

#pragma once

#include <filesystem>
#include <string>

// ----------------------------------------------------------------------------
// Normalise un chemin Windows pour comparaison.
//
// Parametres :
// - path : chemin a normaliser.
//
// Retour :
// - chemin canonique faible, separe par antislashs et en minuscules invariant.
// ----------------------------------------------------------------------------
std::wstring NormalizeWindowsPath(const std::filesystem::path& path);

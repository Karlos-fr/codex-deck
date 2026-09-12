// ============================================================================
// Codex Deck - Matcher fuzzy
// ----------------------------------------------------------------------------
// Ce module score des chaines deja normalisees en minuscules pour la recherche
// et la Command Palette, sans dependance externe.
// ============================================================================

#pragma once

#include <optional>
#include <string_view>
#include <vector>

// ----------------------------------------------------------------------------
// Resultat de correspondance fuzzy.
// ----------------------------------------------------------------------------
struct FuzzyScore {
    // Score total, plus grand signifie meilleur.
    int score = 0;

    // Positions de correspondance dans le candidat.
    std::vector<std::size_t> positions;
};

// ----------------------------------------------------------------------------
// Score une requete fuzzy contre un candidat.
//
// Parametres :
// - query : requete deja normalisee.
// - candidate : candidat deja normalise.
//
// Retour :
// - score si tous les caracteres non espaces correspondent dans l'ordre.
// ----------------------------------------------------------------------------
std::optional<FuzzyScore> FuzzyMatch(std::wstring_view query, std::wstring_view candidate);

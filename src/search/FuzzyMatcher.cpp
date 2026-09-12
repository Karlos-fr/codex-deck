// ============================================================================
// Codex Deck - Implementation du matcher fuzzy
// ----------------------------------------------------------------------------
// Ce fichier applique un algorithme deterministe simple : caracteres dans
// l'ordre, bonus de prefixe/mot/contiguite et penalite des trous longs.
// ============================================================================

#include "FuzzyMatcher.h"

#include <string>
#include <vector>
#include <cwctype>

namespace {

// Bonus pour une correspondance contigue.
constexpr int kContiguousBonus = 18;

// Bonus pour une correspondance en prefixe de mot.
constexpr int kWordPrefixBonus = 30;

// Bonus pour une correspondance en debut de candidat.
constexpr int kStartBonus = 20;

// Score de base par caractere trouve.
constexpr int kCharacterScore = 10;

// Penalite par caractere saute.
constexpr int kGapPenalty = 2;

// Penalite pour un terme de requete absent.
constexpr int kMissingTermPenalty = 35;

// ----------------------------------------------------------------------------
// Indique si une position est un debut de mot.
//
// Parametres :
// - text : texte candidat.
// - index : position testee.
//
// Retour :
// - true si la position commence un mot logique.
// ----------------------------------------------------------------------------
bool IsWordStart(std::wstring_view text, std::size_t index) {
    if (index == 0) {
        return true;
    }
    const wchar_t previous = text[index - 1];
    return previous == L' ' || previous == L'/' || previous == L'\\' || previous == L'-' || previous == L'_';
}

// ----------------------------------------------------------------------------
// Indique si un caractere de requete doit etre ignore.
//
// Parametres :
// - character : caractere source.
//
// Retour :
// - true pour les espaces separant les termes.
// ----------------------------------------------------------------------------
bool IsQuerySeparator(wchar_t character) {
    return std::iswspace(character) != 0;
}

// ----------------------------------------------------------------------------
// Convertit un caractere en minuscule.
//
// Parametres :
// - character : caractere source.
//
// Retour :
// - caractere normalise.
// ----------------------------------------------------------------------------
wchar_t Lower(wchar_t character) {
    return static_cast<wchar_t>(std::towlower(character));
}

// ----------------------------------------------------------------------------
// Decoupe une requete en termes non vides.
//
// Parametres :
// - query : requete source.
//
// Retour :
// - termes compacts.
// ----------------------------------------------------------------------------
std::vector<std::wstring> QueryTerms(std::wstring_view query) {
    std::vector<std::wstring> terms;
    std::wstring current;
    for (const wchar_t character : query) {
        if (IsQuerySeparator(character)) {
            if (!current.empty()) {
                terms.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back(Lower(character));
        }
    }
    if (!current.empty()) {
        terms.push_back(std::move(current));
    }
    return terms;
}

// ----------------------------------------------------------------------------
// Score un terme compact contre un candidat.
//
// Parametres :
// - term : terme sans espace.
// - candidate : candidat source.
//
// Retour :
// - score du terme si tous ses caracteres correspondent.
// ----------------------------------------------------------------------------
std::optional<FuzzyScore> MatchTerm(std::wstring_view term, std::wstring_view candidate) {
    FuzzyScore result{};
    std::size_t search_from = 0;
    std::optional<std::size_t> previous_position;

    for (const wchar_t query_character : term) {
        std::optional<std::size_t> found;
        for (std::size_t index = search_from; index < candidate.size(); ++index) {
            if (Lower(candidate[index]) == query_character) {
                found = index;
                break;
            }
        }
        if (!found) {
            return std::nullopt;
        }

        int score = kCharacterScore;
        if (*found == 0) {
            score += kStartBonus;
        }
        if (IsWordStart(candidate, *found)) {
            score += kWordPrefixBonus;
        }
        if (previous_position) {
            const std::size_t gap = *found - *previous_position - 1;
            if (gap == 0) {
                score += kContiguousBonus;
            } else {
                score -= static_cast<int>(gap) * kGapPenalty;
            }
        } else {
            score -= static_cast<int>(*found) * kGapPenalty;
        }

        result.score += score;
        result.positions.push_back(*found);
        previous_position = *found;
        search_from = *found + 1;
    }
    return result;
}

}  // namespace

// ----------------------------------------------------------------------------
// Score une requete fuzzy contre un candidat.
// ----------------------------------------------------------------------------
std::optional<FuzzyScore> FuzzyMatch(std::wstring_view query, std::wstring_view candidate) {
    FuzzyScore result{};
    const std::vector<std::wstring> terms = QueryTerms(query);
    if (terms.empty()) {
        return std::nullopt;
    }

    for (const std::wstring& term : terms) {
        if (const auto match = MatchTerm(term, candidate)) {
            result.score += match->score;
            result.positions.insert(result.positions.end(), match->positions.begin(), match->positions.end());
        } else {
            result.score -= kMissingTermPenalty;
        }
    }

    return result.positions.empty() ? std::nullopt : std::optional<FuzzyScore>{result};
}

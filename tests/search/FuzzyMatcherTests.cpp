// ============================================================================
// Codex Deck - Tests du matcher fuzzy
// ----------------------------------------------------------------------------
// Ce fichier valide le scoring deterministe utilise par recherche et palette.
// ============================================================================

#include "search/FuzzyMatcher.h"

// ----------------------------------------------------------------------------
// Verifie les bonus de mots et de sequences contigues.
//
// Retour :
// - zero si le classement attendu est respecte.
// ----------------------------------------------------------------------------
int main() {
    const auto spotify = FuzzyMatch(L"spa aud", L"spotifyamp / audio parity");
    const auto remi = FuzzyMatch(L"spa aud", L"openremi / audio notes");
    const auto miss = FuzzyMatch(L"spa aud", L"codex deck");

    if (!spotify || !remi) {
        return 1;
    }
    if (miss) {
        return 2;
    }
    if (spotify->score <= remi->score) {
        return 3;
    }
    if (spotify->positions.empty()) {
        return 4;
    }
    return 0;
}

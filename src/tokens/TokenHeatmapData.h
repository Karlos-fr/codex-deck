// ============================================================================
// Codex Glass - Donnees calculees de la heatmap de tokens
// ----------------------------------------------------------------------------
// Ce fichier declare la projection des agregats quotidiens dans une grille de
// semaines. Il ne depend ni de Direct2D ni du stockage SQLite.
// ============================================================================

#pragma once

#include "TokenUsageTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Nombre fixe de lignes, du lundi au dimanche.
constexpr std::size_t kTokenHeatmapRowCount = 7;

// Nombre de niveaux non nuls derives des bornes visibles.
constexpr std::uint8_t kTokenHeatmapActiveLevelCount = 4;

// ----------------------------------------------------------------------------
// Decrit une cellule quotidienne projetee dans la grille.
// ----------------------------------------------------------------------------
struct TokenHeatmapCell {
    // Cle civile locale au format YYYY-MM-DD.
    std::string date_key;

    // Debut de tranche pour une cellule en granularite cinq minutes.
    std::chrono::system_clock::time_point bucket_start{};

    // Total de tokens du jour, sous-ensembles exclus.
    std::uint64_t total_tokens = 0;

    // Indique que le jour appartient a la couverture disponible.
    bool present = false;

    // Colonne de semaine depuis la gauche.
    std::size_t column = 0;

    // Ligne du jour, lundi valant zero et dimanche six.
    std::size_t row = 0;

    // Niveau visuel, zero puis un a quatre pour les valeurs non nulles.
    std::uint8_t level = 0;
};

// ----------------------------------------------------------------------------
// Construit une grille terminee par la semaine du dernier jour disponible.
//
// Parametres :
// - daily : serie quotidienne ordonnee, jours a zero inclus.
// - week_count : nombre de colonnes de semaines a produire.
//
// Retour :
// - cellules ordonnees par colonne puis par ligne.
// ----------------------------------------------------------------------------
std::vector<TokenHeatmapCell> BuildTokenHeatmapCells(
    const std::vector<TokenDailyUsage>& daily,
    std::size_t week_count
);

// ----------------------------------------------------------------------------
// Projette les dernieres tranches de cinq minutes dans une grille lineaire.
//
// Parametres :
// - buckets : serie chronologique locale de cinq minutes.
// - cell_count : nombre de cellules disponibles dans le layout.
//
// Retour :
// - cellules chronologiques, completees a gauche si la serie est courte.
// ----------------------------------------------------------------------------
std::vector<TokenHeatmapCell> BuildTokenFiveMinuteHeatmapCells(
    const std::vector<TokenFiveMinuteUsage>& buckets,
    std::size_t cell_count
);

// ----------------------------------------------------------------------------
// Calcule un niveau lineaire entre les bornes non nulles visibles.
//
// Parametres :
// - value : total de la cellule.
// - minimum : minimum non nul de la grille visible.
// - maximum : maximum non nul de la grille visible.
//
// Retour :
// - zero pour une valeur nulle, sinon un niveau compris entre un et quatre.
// ----------------------------------------------------------------------------
std::uint8_t CalculateTokenHeatmapLevel(
    std::uint64_t value,
    std::uint64_t minimum,
    std::uint64_t maximum
);

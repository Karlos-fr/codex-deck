// ============================================================================
// Codex Glass - Synthese KPI des tokens
// ----------------------------------------------------------------------------
// Ce fichier declare les agregats temporels du Bilan. Il transforme uniquement
// le snapshot local et ne connait ni Direct2D, ni les interactions du widget.
// ============================================================================

#pragma once

#include "TokenUsageTypes.h"
#include "../settings/AppSettings.h"

#include <cstdint>
#include <vector>

// ----------------------------------------------------------------------------
// Regroupe les compteurs cumules sur une periode du Bilan.
// ----------------------------------------------------------------------------
struct TokenKpiAggregate {
    // Compteurs de tokens additionnes sans recompter leurs sous-ensembles.
    TokenUsageCounts counts{};

    // Nombre total de requetes ou tours sur la periode.
    std::uint64_t request_count = 0;
};

// ----------------------------------------------------------------------------
// Regroupe la periode courante, sa precedente et les micro-series graphiques.
// ----------------------------------------------------------------------------
struct TokenKpiSummary {
    // Agregat de la plage selectionnee la plus recente.
    TokenKpiAggregate current{};

    // Agregat de la plage de meme duree qui precede immediatement la courante.
    TokenKpiAggregate previous{};

    // Indique que tous les buckets de comparaison precedents sont disponibles.
    bool previous_available = false;

    // Totaux de tokens chronologiques utilises par la sparkline principale.
    std::vector<std::uint64_t> token_series;

    // Totaux de requetes chronologiques utilises par la seconde sparkline.
    std::vector<std::uint64_t> request_series;
};

// ----------------------------------------------------------------------------
// Calcule les indicateurs du Bilan pour une plage prise en charge.
//
// Parametres :
// - snapshot : agregats locaux produits par le scanner des sessions.
// - range : duree du Bilan, de une heure a trente jours.
//
// Retour :
// - synthese courante et comparaison, vide si la source ne contient rien.
// ----------------------------------------------------------------------------
TokenKpiSummary BuildTokenKpiSummary(
    const TokenUsageSnapshot& snapshot,
    GraphRange range
);

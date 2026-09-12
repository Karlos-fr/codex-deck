// ============================================================================
// Codex Glass - Rendu des informations provider
// ----------------------------------------------------------------------------
// Ce fichier declare le formatage, le layout et le dessin du forfait, des
// credits et des limites supplementaires. Il reste independant du transport.
// ============================================================================

#pragma once

#include "WidgetRenderUsage.h"
#include "../settings/AppSettings.h"

#include <d2d1.h>

#include <cstddef>
#include <string>
#include <vector>

// Valeur compacte d'une limite additionnelle destinee aux modes orientes.
struct WidgetCompactRateLimitSummary {
    // Libelle court de la limite.
    std::wstring label{};

    // Pourcentage restant deja formate.
    std::wstring percent{};

    // Progression restante normalisee.
    float remaining = 0.0F;

    // Indique si la fenetre utilise la couleur des quotas courts.
    bool short_window = false;
};

// ----------------------------------------------------------------------------
// Compte les fenetres supplementaires visibles d'un snapshot.
//
// Parametres :
// - snapshot : releve contenant les limites additionnelles.
// - visibility : choix utilisateur propres aux lignes Spark.
//
// Retour :
// - nombre de fenetres disponibles qui doivent etre dessinees.
// ----------------------------------------------------------------------------
std::size_t CountVisibleAdditionalRateLimitRows(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
);

// ----------------------------------------------------------------------------
// Compte toutes les lignes de quota visibles pour un mode d'affichage.
//
// Parametres :
// - snapshot : releve contenant les limites additionnelles.
// - visibility : choix utilisateur des quatre lignes principales.
// - include_additional : autorise les lignes provider additionnelles.
//
// Retour :
// - nombre total de lignes principales et additionnelles visibles.
// ----------------------------------------------------------------------------
std::size_t CountVisibleQuotaRows(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility,
    bool include_additional
);

// ----------------------------------------------------------------------------
// Construit les limites additionnelles visibles pour un rendu condense.
//
// Parametres :
// - snapshot : releve contenant les limites additionnelles.
// - visibility : choix utilisateur propres aux lignes Spark.
//
// Retour :
// - valeurs compactes ordonnees comme dans le rendu complet.
// ----------------------------------------------------------------------------
std::vector<WidgetCompactRateLimitSummary> BuildCompactAdditionalRateLimits(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
);

// ----------------------------------------------------------------------------
// Formate la ligne concise de forfait et de credits.
// ----------------------------------------------------------------------------
std::wstring FormatProviderSummary(const UsageSnapshot& snapshot);

// ----------------------------------------------------------------------------
// Dessine la ligne concise de forfait et de credits.
// ----------------------------------------------------------------------------
void DrawProviderSummary(
    const WidgetRenderUsageContext& context,
    const UsageSnapshot& snapshot,
    const D2D1_RECT_F& bounds
);

// ----------------------------------------------------------------------------
// Dessine toutes les fenetres supplementaires et retourne leur bas.
//
// Parametres :
// - context : ressources de dessin partagees.
// - snapshot : releve contenant les limites additionnelles.
// - visibility : choix utilisateur propres aux lignes Spark.
// - first_top : position verticale de la premiere ligne potentielle.
// - left : bord gauche commun des lignes.
// - right : bord droit commun des lignes.
// - short_window_brush : brosse des fenetres courtes.
// - long_window_brush : brosse des fenetres longues.
//
// Retour :
// - bas de la derniere ligne visible ou position initiale si aucune ligne.
// ----------------------------------------------------------------------------
float DrawAdditionalRateLimits(
    const WidgetRenderUsageContext& context,
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility,
    float first_top,
    float left,
    float right,
    ID2D1Brush* short_window_brush,
    ID2D1Brush* long_window_brush
);

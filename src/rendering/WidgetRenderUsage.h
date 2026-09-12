// ============================================================================
// Codex Glass - Rendu des usages
// ----------------------------------------------------------------------------
// Ce fichier declare le dessin des barres, lignes d'usage et chiffres roulants.
// Il depend des ressources de rendu fournies par WidgetRendering.
// ============================================================================

#pragma once

#include "../animation/WidgetRollingNumberAnimation.h"
#include "../usage/UsageSnapshot.h"

#include <d2d1.h>
#include <dwrite.h>

#include <string>

// ----------------------------------------------------------------------------
// Regroupe les ressources Direct2D/DirectWrite necessaires au rendu des usages.
// ----------------------------------------------------------------------------
struct WidgetRenderUsageContext {
    // Cible Direct2D courante.
    ID2D1RenderTarget* render_target = nullptr;

    // Factory DirectWrite utilisee pour mesurer les chiffres roulants.
    IDWriteFactory* dwrite_factory = nullptr;

    // Format DirectWrite des libelles principaux.
    IDWriteTextFormat* body_text_format = nullptr;

    // Format DirectWrite des textes secondaires.
    IDWriteTextFormat* caption_text_format = nullptr;

    // Format DirectWrite du resume provider aligne a droite.
    IDWriteTextFormat* provider_summary_text_format = nullptr;

    // Format DirectWrite des valeurs de quotas alignees a droite.
    IDWriteTextFormat* usage_value_text_format = nullptr;

    // Format DirectWrite des libelles du mode minimal.
    IDWriteTextFormat* minimal_label_text_format = nullptr;

    // Format DirectWrite des valeurs du mode minimal.
    IDWriteTextFormat* minimal_value_text_format = nullptr;

    // Brosse des textes principaux.
    ID2D1Brush* body_text_brush = nullptr;

    // Brosse des textes secondaires.
    ID2D1Brush* muted_text_brush = nullptr;

    // Brosse de fond des barres de progression.
    ID2D1Brush* progress_background_brush = nullptr;

    // Brosse partagee avec le contour principal du widget.
    ID2D1Brush* border_brush = nullptr;

    // Brosse de fond translucide des cartes KPI du mode minimal.
    ID2D1Brush* minimal_card_background_brush = nullptr;

    // Brosse de l'ombre douce des cartes KPI du mode minimal.
    ID2D1Brush* minimal_card_shadow_brush = nullptr;

    // Brosse de valeur 5 h.
    ID2D1Brush* five_hour_value_brush = nullptr;

    // Brosse de valeur hebdomadaire.
    ID2D1Brush* weekly_value_brush = nullptr;
};

// ----------------------------------------------------------------------------
// Anime le libelle de rafraichissement avec un, deux puis trois points.
//
// Parametres :
// - snapshot : releve d'usage courant.
// - dot_count : nombre de points a afficher pendant un rafraichissement.
//
// Retour :
// - libelle d'etat pret a dessiner.
// ----------------------------------------------------------------------------
std::wstring FormatAnimatedFreshnessLabel(const UsageSnapshot& snapshot, std::size_t dot_count);

// ----------------------------------------------------------------------------
// Dessine une barre de progression arrondie.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - bounds : rectangle complet de la barre en DIPs.
// - value : valeur normalisee entre 0 et 1.
// - fill_brush : brosse utilisee pour la partie remplie.
// ----------------------------------------------------------------------------
void DrawProgressBar(
    const WidgetRenderUsageContext& context,
    const D2D1_RECT_F& bounds,
    float value,
    ID2D1Brush* fill_brush
);

// ----------------------------------------------------------------------------
// Dessine une ligne d'usage avec libelle, pourcentage, reset et barre.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - label : libelle principal de la ligne.
// - percent_text : texte du pourcentage affiche.
// - percent_animation : etat d'animation des chiffres.
// - value_available : indique si la valeur doit utiliser le style principal.
// - reset_text : texte de reset affiche sous la barre.
// - usage : valeur normalisee entre 0 et 1.
// - top : position verticale de debut en DIPs.
// - left : position horizontale de debut en DIPs.
// - right : position horizontale de fin en DIPs.
// - progress_brush : brosse utilisee pour la barre remplie.
// ----------------------------------------------------------------------------
void DrawUsageRow(
    const WidgetRenderUsageContext& context,
    const wchar_t* label,
    const wchar_t* percent_text,
    const WidgetRollingNumberFrame& percent_animation,
    bool value_available,
    const wchar_t* reset_text,
    float usage,
    float top,
    float left,
    float right,
    ID2D1Brush* progress_brush
);

// ----------------------------------------------------------------------------
// Dessine une carte unique dans la grille du mode minimal.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - bounds : rectangle de destination de la carte.
// - label : libelle court du quota.
// - percent : pourcentage restant deja formate.
// - animation : animation optionnelle des chiffres.
// - available : indique si la valeur est exploitable.
// - usage : progression restante normalisee.
// - accent_brush : couleur de valeur et de progression.
// - draw_progress : autorise le dessin de la barre de progression.
// ----------------------------------------------------------------------------
void DrawMinimalUsageIndicator(
    const WidgetRenderUsageContext& context,
    const D2D1_RECT_F& bounds,
    const std::wstring& label,
    const std::wstring& percent,
    const WidgetRollingNumberFrame& animation,
    bool available,
    float usage,
    ID2D1Brush* accent_brush,
    bool draw_progress = true
);

// ----------------------------------------------------------------------------
// Dessine un indicateur unique dans une geometrie condensee.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - bounds : rectangle horizontal ou vertical de destination.
// - label : libelle court du quota.
// - percent : pourcentage restant deja formate.
// - animation : animation optionnelle des chiffres.
// - available : indique si la valeur est exploitable.
// - usage : progression restante normalisee.
// - accent_brush : couleur de valeur et de progression.
// ----------------------------------------------------------------------------
void DrawCondensedUsageIndicator(
    const WidgetRenderUsageContext& context,
    const D2D1_RECT_F& bounds,
    const std::wstring& label,
    const std::wstring& percent,
    const WidgetRollingNumberFrame& animation,
    bool available,
    float usage,
    ID2D1Brush* accent_brush
);

// ----------------------------------------------------------------------------
// Dessine un quota condense sous forme de texte, sans carte ni progression.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - bounds : rectangle de destination.
// - label : libelle court du quota.
// - percent : pourcentage restant deja formate.
// - animation : animation optionnelle des chiffres.
// - available : indique si la valeur est exploitable.
// - accent_brush : couleur de la valeur disponible.
// ----------------------------------------------------------------------------
void DrawCondensedUsageValue(
    const WidgetRenderUsageContext& context,
    const D2D1_RECT_F& bounds,
    const std::wstring& label,
    const std::wstring& percent,
    const WidgetRollingNumberFrame& animation,
    bool available,
    ID2D1Brush* accent_brush
);

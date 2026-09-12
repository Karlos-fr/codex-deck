// ============================================================================
// Codex Glass - Contexte de rendu des graphes
// ----------------------------------------------------------------------------
// Ce fichier declare uniquement les ressources Direct2D et DirectWrite
// partagees par les vues Quotas, Tokens, Activite et Bilan.
// ============================================================================

#pragma once

#include <d2d1.h>
#include <dwrite.h>

// ----------------------------------------------------------------------------
// Regroupe les ressources Direct2D/DirectWrite necessaires au rendu du graphe.
// ----------------------------------------------------------------------------
struct WidgetRenderGraphContext {
    // Factory Direct2D utilisee pour les geometries ponctuelles des infobulles.
    ID2D1Factory* d2d_factory = nullptr;

    // Factory DirectWrite utilisee pour ajuster les infobulles a leur texte.
    IDWriteFactory* dwrite_factory = nullptr;

    // Cible Direct2D courante.
    ID2D1RenderTarget* render_target = nullptr;

    // Format DirectWrite partage avec les titres de quotas.
    IDWriteTextFormat* title_text_format = nullptr;

    // Format DirectWrite des textes secondaires.
    IDWriteTextFormat* caption_text_format = nullptr;

    // Brosse des textes secondaires et marqueurs discrets.
    ID2D1SolidColorBrush* muted_text_brush = nullptr;

    // Brosse du contour du graphe.
    ID2D1SolidColorBrush* border_brush = nullptr;

    // Brosse de courbe historique.
    ID2D1SolidColorBrush* history_curve_brush = nullptr;

    // Brosse de fond utilisee pour les infobulles.
    ID2D1SolidColorBrush* panel_background_brush = nullptr;

    // Brosse des textes principaux des onglets et infobulles.
    ID2D1SolidColorBrush* body_text_brush = nullptr;

    // Indique si les controles d'en-tete doivent etre dessines.
    bool show_header_controls = true;
};

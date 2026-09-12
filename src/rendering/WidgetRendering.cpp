// ============================================================================
// Codex Glass - Implementation du rendu Direct2D
// ----------------------------------------------------------------------------
// Ce fichier orchestre le rendu du widget et delegue les details de dessin aux
// modules specialises de `src/rendering`.
// ============================================================================

#include "WidgetRendering.h"

#include "../shell/WidgetScreenshotClipboard.h"
#include "../activity/WidgetActivityText.h"

#include "WidgetRenderConstants.h"
#include "WidgetRenderActivityVein.h"
#include "WidgetRenderColorPanel.h"
#include "WidgetRenderGlassEffect.h"
#include "WidgetGlassElementLenses.h"
#include "WidgetRenderGraph.h"
#include "WidgetRenderGraphControls.h"
#include "WidgetRenderKpiSummary.h"
#include "WidgetRenderMotionPulse.h"
#include "WidgetRenderMotionWave.h"
#include "WidgetGraphLayout.h"
#include "WidgetRenderQuotaGraph.h"
#include "WidgetRenderTokenGraph.h"
#include "WidgetRenderTokenHeatmap.h"
#include "WidgetRenderPalette.h"
#include "WidgetRenderProviderInfo.h"
#include "WidgetRenderResources.h"
#include "WidgetRenderTypes.h"
#include "WidgetRenderUsage.h"
#include "WidgetTrayHideButton.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../usage/UsageFormatting.h"
#include "../usage/UsageHistoryStore.h"
#include "../tokens/TokenKpiSummary.h"
#include "../color/WidgetColorPanel.h"
#include "../glass/WidgetGlassEffectFrame.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <optional>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

// Marge uniforme autour du contenu exporte d'un onglet graphique en DIPs.
constexpr float kGraphScreenshotMargin = 14.0F;

// Facteur uniforme des composants des modes horizontal et vertical.
constexpr float kCondensedDisplayScale = 0.75F;

// ----------------------------------------------------------------------------
// Indique si le graphe compact doit etre dessine.
//
// Parametres :
// - settings : reglages d'affichage courants.
//
// Retour :
// - true si le mode complet et le graphe sont actifs.
// - false sinon.
// ----------------------------------------------------------------------------
bool ShouldRenderGraph(const AppSettings& settings) {
    return settings.display_mode == WidgetDisplayMode::Complete && settings.show_graph;
}

// ----------------------------------------------------------------------------
// Indique si le mode minimal est actif.
//
// Parametres :
// - settings : reglages d'affichage courants.
//
// Retour :
// - true si le widget doit afficher uniquement la synthese minimale.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsMinimalDisplayMode(const AppSettings& settings) {
    return settings.display_mode == WidgetDisplayMode::Minimal;
}

// ----------------------------------------------------------------------------
// Indique si un mode oriente et condense est actif.
//
// Parametres :
// - settings : reglages d'affichage courants.
//
// Retour :
// - true pour les modes horizontal ou vertical.
// - false pour les modes historiques.
// ----------------------------------------------------------------------------
bool IsCondensedDisplayMode(const AppSettings& settings) {
    return settings.display_mode == WidgetDisplayMode::Horizontal
        || settings.display_mode == WidgetDisplayMode::Vertical;
}

// Geometrie complete partagee par le rendu et les lentilles des modes orientes.
struct WidgetCondensedLayout {
    D2D1_RECT_F title_rect{};
    std::vector<D2D1_RECT_F> quota_rects{};
};

// ----------------------------------------------------------------------------
// Construit une ligne ou une colonne de cartes identiques au mode minimal.
//
// Parametres :
// - mode : orientation demandee.
// - content_left : bord gauche interieur.
// - content_right : bord droit interieur.
// - content_top : bord haut interieur commun au mode Minimal.
// - quota_count : nombre d'indicateurs a repartir.
//
// Retour :
// - rectangles prets a dessiner sans chevauchement.
// ----------------------------------------------------------------------------
WidgetCondensedLayout BuildCondensedLayout(
    WidgetDisplayMode mode,
    float content_left,
    float content_right,
    float content_top,
    std::size_t quota_count
) {
    WidgetCondensedLayout layout{};
    layout.quota_rects.reserve(quota_count);
    constexpr float card_size = 96.0F * kCondensedDisplayScale;
    constexpr float card_gap = 12.0F * kCondensedDisplayScale;
    constexpr float title_extent = 24.0F * kCondensedDisplayScale;
    layout.title_rect = D2D1::RectF(
        content_left,
        content_top,
        content_right,
        content_top + title_extent
    );
    if (mode == WidgetDisplayMode::Horizontal) {
        const float cards_left = content_left;
        const float cards_top = content_top + title_extent;
        for (std::size_t index = 0; index < quota_count; ++index) {
            const float left = cards_left + static_cast<float>(index) * (card_size + card_gap);
            layout.quota_rects.push_back(D2D1::RectF(
                left,
                cards_top,
                left + card_size,
                cards_top + card_size
            ));
        }
        return layout;
    }

    for (std::size_t index = 0; index < quota_count; ++index) {
        const float top = content_top + title_extent
            + static_cast<float>(index) * (card_size + card_gap);
        layout.quota_rects.push_back(D2D1::RectF(
            content_left,
            top,
            content_left + card_size,
            top + card_size
        ));
    }
    return layout;
}

// ----------------------------------------------------------------------------
// Repartit les quotas visibles dans une grille minimale de deux colonnes.
//
// Parametres :
// - size : dimensions Direct2D de la fenetre.
// - left : bord gauche interieur.
// - right : bord droit interieur.
// - top : position de la premiere rangee.
// - quota_count : nombre de cartes a disposer.
//
// Retour :
// - rectangles ordonnes, la derniere carte impaire occupant toute la largeur.
// ----------------------------------------------------------------------------
std::vector<D2D1_RECT_F> BuildMinimalQuotaRects(
    D2D1_SIZE_F size,
    float left,
    float right,
    float top,
    std::size_t quota_count
) {
    std::vector<D2D1_RECT_F> rects{};
    if (quota_count == 0) {
        return rects;
    }

    // Espacement historique entre les deux cartes de chaque rangee.
    constexpr float column_gap = 20.0F;

    // Espacement vertical discret entre deux rangees.
    constexpr float row_gap = 12.0F;

    const std::size_t row_count = (quota_count + 1U) / 2U;
    const float available_height = size.height - 12.0F - top
        - row_gap * static_cast<float>(row_count - 1U);
    const float row_height = available_height / static_cast<float>(row_count);
    rects.reserve(quota_count);
    std::size_t quota_index = 0;
    for (std::size_t row = 0; row < row_count; ++row) {
        const float row_top = top + static_cast<float>(row) * (row_height + row_gap);
        const std::size_t remaining = quota_count - quota_index;
        const std::size_t column_count = std::min<std::size_t>(2U, remaining);
        if (column_count == 1U) {
            rects.push_back(D2D1::RectF(left, row_top, right, row_top + row_height));
            ++quota_index;
            continue;
        }

        const float middle = left + ((right - left) * 0.5F);
        rects.push_back(D2D1::RectF(
            left,
            row_top,
            middle - (column_gap * 0.5F),
            row_top + row_height
        ));
        rects.push_back(D2D1::RectF(
            middle + (column_gap * 0.5F),
            row_top,
            right,
            row_top + row_height
        ));
        quota_index += 2U;
    }
    return rects;
}

// ----------------------------------------------------------------------------
// Ajoute les quatre composants visuels d'une ligne de quota.
//
// Parametres :
// - lenses : collection dynamique de la frame.
// - top : position verticale de la ligne.
// - left : bord gauche du contenu.
// - right : bord droit du contenu.
// - profile : profil Glass courant.
// ----------------------------------------------------------------------------
void AppendUsageRowGlassLenses(
    std::vector<WidgetGlassEffectLens>& lenses,
    float top,
    float left,
    float right,
    const GlassEffectRenderProfile& profile
) {
    AppendWidgetGlassElementLens(lenses, D2D1::RectF(left, top, right - 64.0F, top + 20.0F), profile, 4.0F);
    AppendWidgetGlassElementLens(lenses, D2D1::RectF(right - 54.0F, top, right, top + 20.0F), profile, 4.0F);
    AppendWidgetGlassElementLens(
        lenses,
        D2D1::RectF(left, top + 24.0F, right, top + 24.0F + kProgressBarHeight),
        profile,
        kProgressCornerRadius
    );
    AppendWidgetGlassElementLens(lenses, D2D1::RectF(left, top + 36.0F, right, top + 54.0F), profile, 4.0F);
}

// ----------------------------------------------------------------------------
// Ajoute les composants de controle et de donnees du graphe courant.
//
// Parametres :
// - lenses : collection dynamique de la frame.
// - layout : geometrie du graphe deja calculee pour le dessin.
// - interaction : etat courant des tooltips du graphe.
// - page : vue active determinant les composants a refracter.
// - profile : profil Glass courant.
// ----------------------------------------------------------------------------
void AppendGraphGlassLenses(
    std::vector<WidgetGlassEffectLens>& lenses,
    const WidgetGraphLayout& layout,
    const WidgetGraphInteraction& interaction,
    WidgetGraphPage page,
    const GlassEffectRenderProfile& profile,
    bool show_header_controls
) {
    AppendWidgetGlassElementLens(lenses, layout.title_rect, profile, 4.0F);
    if (show_header_controls) {
        AppendWidgetGlassElementLens(lenses, layout.help_button_rect, profile, 6.0F);
        AppendWidgetGlassElementLens(lenses, layout.capture_button_rect, profile, 5.0F);
        AppendWidgetGlassElementLens(lenses, layout.range_selector_rect, profile, 5.0F);
    }
    if (page == WidgetGraphPage::Summary) {
        for (const D2D1_RECT_F& card : layout.summary_card_rects) {
            AppendWidgetGlassElementLens(lenses, card, profile, 6.0F);
        }
    } else if (page != WidgetGraphPage::Activity) {
        AppendWidgetGlassElementLens(lenses, layout.y_axis_rect, profile, 4.0F);
        AppendWidgetGlassElementLens(lenses, layout.plot_rect, profile, 6.0F);
        AppendWidgetGlassElementLens(lenses, layout.x_axis_rect, profile, 4.0F);
    } else {
        AppendWidgetGlassElementLens(lenses, layout.heatmap_months_rect, profile, 4.0F);
        AppendWidgetGlassElementLens(lenses, layout.heatmap_grid_rect, profile, 6.0F);
        AppendWidgetGlassElementLens(lenses, layout.heatmap_legend_rect, profile, 4.0F);
    }
    AppendWidgetGlassElementLens(lenses, layout.quotas_tab_rect, profile, 5.0F);
    AppendWidgetGlassElementLens(lenses, layout.tokens_tab_rect, profile, 5.0F);
    AppendWidgetGlassElementLens(lenses, layout.activity_tab_rect, profile, 5.0F);
    AppendWidgetGlassElementLens(lenses, layout.summary_tab_rect, profile, 5.0F);
    AppendWidgetGlassElementLens(lenses, layout.freshness_status_rect, profile, 4.0F);
    if (show_header_controls && interaction.hovered_graph_info) {
        AppendWidgetGlassElementLens(lenses, layout.hint_tooltip_rect, profile, 5.0F);
    }
    if (show_header_controls && interaction.hovered_graph_capture_button
        && !interaction.pressed_graph_capture_button) {
        AppendWidgetGlassElementLens(lenses, layout.capture_hint_rect, profile, 5.0F);
    }
    if (interaction.hovered_token_bar.has_value()
        || interaction.hovered_heatmap_cell.has_value()
        || interaction.hovered_quota_plot_point.has_value()) {
        AppendWidgetGlassElementLens(lenses, layout.data_tooltip_rect, profile, 5.0F);
    }
}

} // namespace

// ----------------------------------------------------------------------------
// Implementation interne conservant les ressources graphiques natives.
// ----------------------------------------------------------------------------
struct WidgetRenderer::Impl : public WidgetRenderResources {

    // Cache du bitmap Direct2D cree depuis la derniere frame GlassEffect.
    WidgetRenderGlassEffectCache glass_effect_cache;

    // Cible de dessin temporaire utilisee pendant la composition globale Wave.
    ID2D1RenderTarget* active_render_target = nullptr;

    // Cible bitmap compatible reutilisee entre les images de Wave.
    ComPtr<ID2D1BitmapRenderTarget> wave_scene_render_target;

    // ------------------------------------------------------------------------
    // Retourne la cible de scene courante ou la cible HWND par defaut.
    // ------------------------------------------------------------------------
    ID2D1RenderTarget* DrawingTarget() const {
        return active_render_target != nullptr ? active_render_target : render_target.Get();
    }

    // ------------------------------------------------------------------------
    // Libere les ressources Direct2D dependant de la fenetre.
    // ------------------------------------------------------------------------
    void DiscardDeviceResources() {
        wave_scene_render_target.Reset();
        WidgetRenderResources::DiscardDeviceResources();
        glass_effect_cache.Discard();
    }

    // ------------------------------------------------------------------------
    // Demarre le dessin de scene sur une cible compatible lorsque Wave agit.
    //
    // Parametres :
    // - size : taille logique de la scene.
    // - motion : etat Motion de la frame courante.
    //
    // Retour :
    // - true si la scene devra etre recomposee par Wave.
    // ------------------------------------------------------------------------
    bool BeginMotionWaveScene(
        D2D1_SIZE_F size,
        const WidgetVibrationGlassEffectState& motion
    ) {
        const D2D1_SIZE_U pixel_size = render_target->GetPixelSize();
        if (motion.wave_refraction > 0.0F) {
            if (wave_scene_render_target != nullptr
                && (wave_scene_render_target->GetPixelSize().width != pixel_size.width
                    || wave_scene_render_target->GetPixelSize().height != pixel_size.height)) {
                wave_scene_render_target.Reset();
            }
            if (wave_scene_render_target == nullptr) {
                render_target->CreateCompatibleRenderTarget(
                    &size,
                    &pixel_size,
                    nullptr,
                    D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,
                    wave_scene_render_target.GetAddressOf()
                );
            }
        }

        const bool composition_active = motion.wave_refraction > 0.0F
            && wave_scene_render_target != nullptr;
        active_render_target = composition_active
            ? static_cast<ID2D1RenderTarget*>(wave_scene_render_target.Get())
            : static_cast<ID2D1RenderTarget*>(render_target.Get());
        DrawingTarget()->BeginDraw();
        return composition_active;
    }

    // ------------------------------------------------------------------------
    // Termine la scene et applique Wave une seule fois sur son bitmap complet.
    //
    // Parametres :
    // - size : taille logique de la scene.
    // - palette : couleurs utilisees pour effacer la cible finale.
    // - motion : etat Wave de la frame courante.
    // - composition_active : resultat retourne par BeginMotionWaveScene.
    // ------------------------------------------------------------------------
    void EndMotionWaveScene(
        D2D1_SIZE_F size,
        const Palette& palette,
        const WidgetVibrationGlassEffectState& motion,
        bool composition_active
    ) {
        HRESULT result = DrawingTarget()->EndDraw();
        active_render_target = nullptr;
        if (FAILED(result)) {
            if (result == D2DERR_RECREATE_TARGET) {
                DiscardDeviceResources();
            }
            return;
        }
        if (!composition_active) {
            return;
        }

        ComPtr<ID2D1Bitmap> scene_bitmap;
        if (FAILED(wave_scene_render_target->GetBitmap(scene_bitmap.GetAddressOf()))) {
            return;
        }
        render_target->BeginDraw();
        render_target->Clear(palette.window_background);
        DrawWidgetMotionWaveComposition(
            render_target.Get(),
            scene_bitmap.Get(),
            size,
            motion
        );
        result = render_target->EndDraw();
        if (result == D2DERR_RECREATE_TARGET) {
            DiscardDeviceResources();
        }
    }

    // ------------------------------------------------------------------------
    // Dessine un texte DirectWrite dans le rectangle donne.
    //
    // Parametres :
    // - text : texte Unicode a dessiner.
    // - layout_rect : rectangle de destination en DIPs.
    // - format : format DirectWrite a utiliser.
    // - brush : brosse Direct2D a utiliser.
    // ------------------------------------------------------------------------
    void DrawTextLine(
        const wchar_t* text,
        const D2D1_RECT_F& layout_rect,
        IDWriteTextFormat* format,
        ID2D1Brush* brush
    ) {
        DrawingTarget()->DrawTextW(
            text,
            static_cast<UINT32>(wcslen(text)),
            format,
            layout_rect,
            brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP
        );
    }

    // ------------------------------------------------------------------------
    // Dessine un texte aligne a gauche sans modifier durablement le format DirectWrite.
    //
    // Parametres :
    // - text : texte Unicode a dessiner.
    // - layout_rect : rectangle de destination en DIPs.
    // - format : format DirectWrite a utiliser.
    // - brush : brosse Direct2D a utiliser.
    // ------------------------------------------------------------------------
    void DrawLeadingTextLine(
        const wchar_t* text,
        const D2D1_RECT_F& layout_rect,
        IDWriteTextFormat* format,
        ID2D1Brush* brush
    ) {
        const DWRITE_TEXT_ALIGNMENT previous_alignment = format->GetTextAlignment();
        const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph = format->GetParagraphAlignment();
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        DrawTextLine(text, layout_rect, format, brush);
        format->SetTextAlignment(previous_alignment);
        format->SetParagraphAlignment(previous_paragraph);
    }

    // ------------------------------------------------------------------------
    // Mesure la largeur exacte d'un texte DirectWrite en DIPs.
    //
    // Parametres :
    // - text : texte Unicode a mesurer.
    // - format : format DirectWrite utilise pour la mesure.
    //
    // Retour :
    // - largeur du texte, ou zero si DirectWrite refuse le layout.
    // ------------------------------------------------------------------------
    float MeasureTextWidth(const std::wstring& text, IDWriteTextFormat* format) const {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        const HRESULT result = dwrite_factory->CreateTextLayout(
            text.c_str(),
            static_cast<UINT32>(text.size()),
            format,
            1000.0F,
            100.0F,
            layout.GetAddressOf()
        );
        if (FAILED(result)) {
            return 0.0F;
        }

        DWRITE_TEXT_METRICS metrics{};
        if (FAILED(layout->GetMetrics(&metrics))) {
            return 0.0F;
        }
        return metrics.widthIncludingTrailingWhitespace;
    }

    // ------------------------------------------------------------------------
    // Mesure un texte avec le format partage par les titres de quotas.
    // ------------------------------------------------------------------------
    float MeasureGraphTitleTextWidth(const std::wstring& text) const {
        return MeasureTextWidth(text, body_text_format.Get());
    }

    // ------------------------------------------------------------------------
    // Mesure un texte avec le format partage par les hints compacts.
    //
    // Parametres :
    // - text : texte localise a mesurer.
    //
    // Retour :
    // - largeur DirectWrite en DIPs, ou zero si la mesure echoue.
    // ------------------------------------------------------------------------
    float MeasureCaptionTextWidth(const std::wstring& text) const {
        return MeasureTextWidth(text, caption_text_format.Get());
    }

    // ------------------------------------------------------------------------
    // Dessine le statut de mise a jour dans une zone fixe alignee a droite.
    //
    // Parametres :
    // - snapshot : releve dont la fraicheur doit etre affichee.
    // - bounds : ligne complete reservee au statut.
    // - refreshing_label_dot_count : nombre courant de points animes.
    // ------------------------------------------------------------------------
    void DrawFreshnessStatus(
        const UsageSnapshot& snapshot,
        const D2D1_RECT_F& bounds,
        std::size_t refreshing_label_dot_count
    ) {
        if (snapshot.freshness == UsageFreshness::Refreshing) {
            const std::wstring maximum_refresh_text = T(IDS_STATUS_REFRESHING) + L"...";
            const float measured_refresh_width = MeasureTextWidth(
                maximum_refresh_text,
                refreshing_status_text_format.Get()
            );
            const float refresh_width = measured_refresh_width > 0.0F
                ? std::min(measured_refresh_width, kFreshnessStatusWidth)
                : kFreshnessStatusWidth;
            const D2D1_RECT_F refreshing_status_rect = D2D1::RectF(
                std::max(bounds.left, bounds.right - refresh_width),
                bounds.top,
                bounds.right,
                bounds.bottom
            );
            const std::wstring freshness_text = FormatAnimatedFreshnessLabel(
                snapshot,
                refreshing_label_dot_count
            );
            DrawTextLine(
                freshness_text.c_str(),
                refreshing_status_rect,
                refreshing_status_text_format.Get(),
                muted_text_brush.Get()
            );
            return;
        }

        const D2D1_RECT_F status_rect = D2D1::RectF(
            std::max(bounds.left, bounds.right - kFreshnessStatusWidth),
            bounds.top,
            bounds.right,
            bounds.bottom
        );
        const std::wstring last_update = FormatLastUpdateTime(snapshot.sampled_at);
        DrawTextLine(
            last_update.c_str(),
            status_rect,
            status_text_format.Get(),
            muted_text_brush.Get()
        );
    }

    // ------------------------------------------------------------------------
    // Regroupe les ressources necessaires au rendu des usages.
    //
    // Retour :
    // - contexte de rendu des barres, lignes et chiffres.
    // ------------------------------------------------------------------------
    WidgetRenderUsageContext BuildUsageContext() const {
        return WidgetRenderUsageContext{
            DrawingTarget(),
            dwrite_factory.Get(),
            body_text_format.Get(),
            caption_text_format.Get(),
            provider_summary_text_format.Get(),
            usage_value_text_format.Get(),
            minimal_label_text_format.Get(),
            minimal_value_text_format.Get(),
            body_text_brush.Get(),
            muted_text_brush.Get(),
            progress_background_brush.Get(),
            border_brush.Get(),
            minimal_card_background_brush.Get(),
            minimal_card_shadow_brush.Get(),
            five_hour_remaining_accent_brush.Get(),
            weekly_remaining_accent_brush.Get(),
        };
    }

    // ------------------------------------------------------------------------
    // Regroupe les ressources necessaires au rendu du graphe.
    //
    // Retour :
    // - contexte de rendu du graphe historique.
    // ------------------------------------------------------------------------
    WidgetRenderGraphContext BuildGraphContext(bool show_header_controls = true) const {
        return WidgetRenderGraphContext{
            d2d_factory.Get(),
            dwrite_factory.Get(),
            DrawingTarget(),
            body_text_format.Get(),
            caption_text_format.Get(),
            muted_text_brush.Get(),
            border_brush.Get(),
            history_curve_brush.Get(),
            panel_background_brush.Get(),
            body_text_brush.Get(),
            show_header_controls,
        };
    }

    // ------------------------------------------------------------------------
    // Regroupe les ressources necessaires au rendu du panneau couleurs.
    //
    // Retour :
    // - contexte de rendu du panneau couleurs.
    // ------------------------------------------------------------------------
    WidgetRenderColorPanelContext BuildColorPanelContext() {
        return WidgetRenderColorPanelContext{
            DrawingTarget(),
            &glass_effect_cache,
            panel_background_brush.Get(),
            border_brush.Get(),
            body_text_brush.Get(),
            muted_text_brush.Get(),
            color_panel_label_text_format.Get(),
            color_panel_button_text_format.Get(),
            body_text_format.Get(),
        };
    }

    // ------------------------------------------------------------------------
    // Rend l'onglet graphique actif dans une cible bitmap independante.
    //
    // Parametres :
    // - hwnd : fenetre qui fournit les ressources et le DPI courants.
    // - settings : page, plage et palette actuellement selectionnees.
    // - snapshot : quotas courants necessaires au layout.
    // - token_snapshot : donnees locales des vues de consommation.
    // - history_store : historique de la vue Quotas.
    // - graph_range : plage active de la vue Quotas.
    // - quota_graph_animation : serie visible de la vue Quotas.
    //
    // Retour :
    // - true si le rendu hors ecran a ete copie dans le presse-papiers.
    //
    // Effets de bord :
    // - aucun rendu n'est presente dans la fenetre principale.
    // ------------------------------------------------------------------------
    bool CopyActiveGraphTabToClipboard(
        HWND hwnd,
        const AppSettings& settings,
        const UsageSnapshot& snapshot,
        const TokenUsageSnapshot& token_snapshot,
        const UsageHistoryStore& history_store,
        GraphRange graph_range,
        const WidgetQuotaGraphAnimationFrame& quota_graph_animation
    ) {
        if (!CreateDeviceResources(hwnd, settings) || !ShouldRenderGraph(settings)) {
            return false;
        }

        const D2D1_SIZE_F widget_size = render_target->GetSize();
        const std::size_t token_bar_count = settings.graph_page == WidgetGraphPage::Activity
                && settings.activity_graph_range == GraphRange::Minutes5
            ? token_snapshot.five_minute.size()
            : (settings.graph_page == WidgetGraphPage::Tokens
            ? std::min(
                settings.token_graph_range == GraphRange::Hours1
                    ? token_snapshot.five_minute.size()
                    : token_snapshot.hourly.size(),
                TokenGraphBucketCount(settings.token_graph_range)
            )
            : 0U);
        const std::wstring graph_title = settings.graph_page == WidgetGraphPage::Tokens
            ? T(IDS_GRAPH_TOKENS_TITLE)
            : (settings.graph_page == WidgetGraphPage::Activity
                ? T(IDS_GRAPH_ACTIVITY_TITLE)
                : (settings.graph_page == WidgetGraphPage::Summary
                    ? T(IDS_GRAPH_SUMMARY_TITLE)
                    : T(IDS_GRAPH_TAB_QUOTAS)));
        const WidgetGraphLayout layout = BuildWidgetGraphLayout(
            widget_size,
            settings,
            snapshot,
            token_bar_count,
            MeasureGraphTitleTextWidth(graph_title),
            MeasureCaptionTextWidth(T(IDS_GRAPH_CAPTURE_HINT))
        );
        const float content_bottom = settings.graph_page == WidgetGraphPage::Summary
            ? layout.summary_grid_rect.bottom
            : (settings.graph_page == WidgetGraphPage::Activity
                ? layout.heatmap_legend_rect.bottom
                : layout.x_axis_rect.bottom);
        const D2D1_SIZE_F output_size = D2D1::SizeF(
            std::max(1.0F, layout.section_rect.right - layout.section_rect.left)
                + (kGraphScreenshotMargin * 2.0F),
            std::max(1.0F, content_bottom - layout.section_rect.top)
                + (kGraphScreenshotMargin * 2.0F)
        );

        ComPtr<ID2D1BitmapRenderTarget> screenshot_target;
        const HRESULT target_result = render_target->CreateCompatibleRenderTarget(
            &output_size,
            nullptr,
            nullptr,
            D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_GDI_COMPATIBLE,
            screenshot_target.GetAddressOf()
        );
        if (FAILED(target_result) || screenshot_target == nullptr) {
            return false;
        }

        const Palette palette = ActivePalette(settings);
        const WidgetGraphInteraction clean_interaction{};
        active_render_target = screenshot_target.Get();
        screenshot_target->BeginDraw();
        screenshot_target->Clear(palette.panel_background);
        screenshot_target->SetTransform(D2D1::Matrix3x2F::Translation(
            kGraphScreenshotMargin - layout.section_rect.left,
            kGraphScreenshotMargin - layout.section_rect.top
        ));
        const WidgetRenderGraphContext graph_context = BuildGraphContext(false);
        if (settings.graph_page == WidgetGraphPage::Tokens) {
            DrawTokenUsageGraph(
                graph_context,
                layout,
                token_snapshot,
                clean_interaction,
                settings.token_graph_range
            );
        } else if (settings.graph_page == WidgetGraphPage::Activity) {
            DrawTokenUsageHeatmap(
                graph_context,
                layout,
                token_snapshot,
                clean_interaction,
                settings.activity_graph_range
            );
        } else if (settings.graph_page == WidgetGraphPage::Summary) {
            DrawWidgetKpiSummary(
                graph_context,
                layout,
                token_snapshot,
                clean_interaction,
                settings.summary_graph_range
            );
        } else {
            DrawQuotaGraph(
                graph_context,
                layout,
                history_store,
                snapshot,
                clean_interaction,
                graph_range,
                quota_graph_animation
            );
        }
        screenshot_target->SetTransform(D2D1::Matrix3x2F::Identity());
        const HRESULT draw_result = screenshot_target->EndDraw();
        active_render_target = nullptr;
        if (FAILED(draw_result)) {
            return false;
        }
        return CopyD2DRenderTargetScreenshotToClipboard(hwnd, screenshot_target.Get());
    }

    // ------------------------------------------------------------------------
    // Dessine l'interface complete du widget avec Direct2D et DirectWrite.
    //
    // Parametres :
    // - hwnd : handle de la fenetre a redessiner.
    // - settings : reglages visuels et d'affichage courants.
    // - snapshot : releve d'usage courant.
    // - token_snapshot : agregats locaux affiches dans les vues Tokens.
    // - graph_interaction : etat interactif courant des graphes.
    // - tray_button_interaction : etat du bouton de masquage dans le tray.
    // - history_store : historique local utilise pour le graphe.
    // - graph_range : plage temporelle du graphe.
    // - activity_frame : etats stabilises de l'activite Codex.
    // - activity_interaction : survol global du filament.
    // ------------------------------------------------------------------------
    void Render(
        HWND hwnd,
        const AppSettings& settings,
        const UsageSnapshot& snapshot,
        const TokenUsageSnapshot& token_snapshot,
        const WidgetGraphInteraction& graph_interaction,
        const WidgetTrayHideButtonInteraction& tray_button_interaction,
        const UsageHistoryStore& history_store,
        GraphRange graph_range,
        const WidgetActivityFrame& activity_frame,
        const WidgetActivityVeinInteraction& activity_interaction,
        const WidgetQuotaGraphAnimationFrame& quota_graph_animation,
        const WidgetGlassEffectFrame* glass_effect_frame,
        const WidgetVibrationGlassEffectState& vibration_glass_effect,
        const WidgetRollingNumberFrame& five_hour_rolling,
        const WidgetRollingNumberFrame& weekly_rolling,
        std::size_t refreshing_label_dot_count,
        bool show_graph_header_controls
    ) {
        if (!CreateDeviceResources(hwnd, settings)) {
            return;
        }

        const D2D1_SIZE_F size = render_target->GetSize();
        const D2D1_RECT_F panel_rect = D2D1::RectF(
            kPanelMargin,
            kPanelMargin,
            std::max(kPanelMargin, size.width - kPanelMargin),
            std::max(kPanelMargin, size.height - kPanelMargin)
        );

        const bool minimal_mode = IsMinimalDisplayMode(settings);
        const bool condensed_mode = IsCondensedDisplayMode(settings);
        const GlassEffectRenderProfile glass_effect_profile = ApplyAnimationsToGlassEffectProfile(
            ApplyVibrationToGlassEffectProfile(
                ApplyUserOpacityToGlassEffectProfile(
                    GlassEffectProfileForAppearance(settings.glass_effect.appearance),
                    settings.background_opacity
                ),
                vibration_glass_effect
            ),
            settings.glass_effect
        );
        const WidgetGlassEffectAnimationSettings base_glass_effect_animation =
            GlassEffectAnimationSettingsForSettings(
                settings.glass_effect,
                GlassEffectAnimationTimeSeconds()
            );
        const float panel_padding = minimal_mode
            ? kMinimalPanelPadding
            : (condensed_mode ? kMinimalPanelPadding * kCondensedDisplayScale : kPanelPadding);
        const float content_left = panel_rect.left + panel_padding;
        const float content_right = panel_rect.right - panel_padding;
        const float content_top = panel_rect.top + (minimal_mode
            ? 14.0F
            : (condensed_mode ? 14.0F * kCondensedDisplayScale : 12.0F));
        const float first_usage_top = content_top + 42.0F;
        const WidgetTrayHideButtonLayout tray_button_layout = BuildWidgetTrayHideButtonLayout(
            size,
            settings.display_mode,
            MeasureCaptionTextWidth(T(IDS_WIDGET_HIDE_TO_TRAY_HINT))
        );
        const std::wstring activity_label = BuildWidgetActivityLabel(activity_frame);
        const std::wstring activity_hint = BuildWidgetActivityHint(activity_frame);
        const WidgetActivityVeinLayout activity_vein_layout = BuildWidgetActivityVeinLayout(
            size,
            settings.display_mode,
            MeasureCaptionTextWidth(activity_hint)
        );
        const std::size_t condensed_quota_count = condensed_mode
            ? CountVisibleQuotaRows(snapshot, settings.quota_visibility, true)
            : 0U;
        const std::size_t minimal_quota_count = minimal_mode
            ? CountVisibleQuotaRows(snapshot, settings.quota_visibility, true)
            : 0U;
        const std::vector<D2D1_RECT_F> minimal_quota_rects = minimal_mode
            ? BuildMinimalQuotaRects(
                size,
                content_left,
                content_right,
                first_usage_top,
                minimal_quota_count
            )
            : std::vector<D2D1_RECT_F>{};
        const std::optional<WidgetCondensedLayout> condensed_layout = condensed_mode
            ? std::optional<WidgetCondensedLayout>(BuildCondensedLayout(
                settings.display_mode,
                content_left,
                content_right,
                content_top,
                condensed_quota_count
            ))
            : std::nullopt;
        const float header_content_right = std::max(
            content_left,
            tray_button_layout.button_rect.left - 6.0F
        );
        const bool usage_data_available = snapshot.sampled_at.time_since_epoch().count() != 0;
        const std::wstring graph_title = settings.graph_page == WidgetGraphPage::Tokens
            ? T(IDS_GRAPH_TOKENS_TITLE)
            : (settings.graph_page == WidgetGraphPage::Activity
                ? T(IDS_GRAPH_ACTIVITY_TITLE)
                : (settings.graph_page == WidgetGraphPage::Summary
                    ? T(IDS_GRAPH_SUMMARY_TITLE)
                    : T(IDS_GRAPH_TAB_QUOTAS)));
        std::optional<WidgetGraphLayout> graph_layout;
        if (ShouldRenderGraph(settings)) {
            const std::size_t token_bar_count = settings.graph_page == WidgetGraphPage::Activity
                    && settings.activity_graph_range == GraphRange::Minutes5
                ? token_snapshot.five_minute.size()
                : (settings.graph_page == WidgetGraphPage::Tokens
                ? std::min(
                    settings.token_graph_range == GraphRange::Hours1
                        ? token_snapshot.five_minute.size()
                        : token_snapshot.hourly.size(),
                    TokenGraphBucketCount(settings.token_graph_range)
                )
                : 0U);
            graph_layout = BuildWidgetGraphLayout(
                size,
                settings,
                snapshot,
                token_bar_count,
                MeasureGraphTitleTextWidth(graph_title),
                MeasureCaptionTextWidth(T(IDS_GRAPH_CAPTURE_HINT))
            );
        }
        std::vector<WidgetGlassEffectLens> glass_lenses;
        if (condensed_layout.has_value()) {
            AppendWidgetGlassElementLens(
                glass_lenses,
                condensed_layout->title_rect,
                glass_effect_profile,
                4.0F * kCondensedDisplayScale
            );
            for (const D2D1_RECT_F& quota_rect : condensed_layout->quota_rects) {
                AppendWidgetGlassElementLens(
                    glass_lenses,
                    quota_rect,
                    glass_effect_profile,
                    8.0F * kCondensedDisplayScale
                );
            }
        } else if (minimal_mode) {
            AppendWidgetGlassElementLens(
                glass_lenses,
                D2D1::RectF(content_left, content_top, content_right, content_top + 24.0F),
                glass_effect_profile,
                5.0F
            );
            for (const D2D1_RECT_F& quota_rect : minimal_quota_rects) {
                AppendWidgetGlassElementLens(
                    glass_lenses,
                    quota_rect,
                    glass_effect_profile,
                    6.0F
                );
            }
        } else {
            const std::wstring title = T(IDS_APP_TITLE);
            const float measured_title_width = MeasureTextWidth(title, title_text_format.Get());
            const float title_width = std::min(
                std::max(0.0F, content_right - content_left),
                measured_title_width > 0.0F ? measured_title_width : content_right - content_left
            );
            const D2D1_RECT_F title_rect = D2D1::RectF(
                content_left,
                content_top,
                content_left + title_width,
                content_top + 28.0F
            );
            AppendWidgetGlassElementLens(glass_lenses, title_rect, glass_effect_profile, 5.0F);
            AppendWidgetGlassElementLens(
                glass_lenses,
                D2D1::RectF(
                    std::min(header_content_right, title_rect.right + 10.0F),
                    content_top,
                    header_content_right,
                    content_top + 28.0F
                ),
                glass_effect_profile,
                5.0F
            );
            float quota_lens_top = first_usage_top;
            if (settings.quota_visibility.five_hour) {
                AppendUsageRowGlassLenses(
                    glass_lenses,
                    quota_lens_top,
                    content_left,
                    content_right,
                    glass_effect_profile
                );
                quota_lens_top += kRowSpacing;
            }
            if (settings.quota_visibility.weekly) {
                AppendUsageRowGlassLenses(
                    glass_lenses,
                    quota_lens_top,
                    content_left,
                    content_right,
                    glass_effect_profile
                );
                quota_lens_top += kRowSpacing;
            }
            if (settings.display_mode == WidgetDisplayMode::Complete) {
                const std::size_t additional_row_count = CountVisibleAdditionalRateLimitRows(
                    snapshot,
                    settings.quota_visibility
                );
                for (std::size_t index = 0; index < additional_row_count; ++index) {
                    AppendUsageRowGlassLenses(
                        glass_lenses,
                        quota_lens_top + (static_cast<float>(index) * kRowSpacing),
                        content_left,
                        content_right,
                        glass_effect_profile
                    );
                }
            }
            if (graph_layout.has_value()) {
                AppendGraphGlassLenses(
                    glass_lenses,
                    *graph_layout,
                    graph_interaction,
                    settings.graph_page,
                    glass_effect_profile,
                    show_graph_header_controls
                );
            } else {
                AppendWidgetGlassElementLens(
                    glass_lenses,
                    D2D1::RectF(content_left, size.height - 24.0F, content_right, size.height - 10.0F),
                    glass_effect_profile,
                    4.0F
                );
            }
        }
        AppendWidgetGlassElementLens(
            glass_lenses,
            tray_button_layout.button_rect,
            glass_effect_profile,
            5.0F
        );
        if (tray_button_interaction.hovered && !tray_button_interaction.pressed) {
            AppendWidgetGlassElementLens(
                glass_lenses,
                tray_button_layout.hint_rect,
                glass_effect_profile,
                5.0F
            );
        }
        if (activity_interaction.hovered && activity_vein_layout.available) {
            AppendWidgetGlassElementLens(
                glass_lenses,
                activity_vein_layout.hint_rect,
                glass_effect_profile,
                5.0F
            );
        }
        const Palette palette = ActivePalette(settings);
        const bool wave_composition_active = BeginMotionWaveScene(size, vibration_glass_effect);
        const WidgetGlassEffectAnimationSettings glass_effect_animation = wave_composition_active
            ? base_glass_effect_animation
            : ApplyMotionWaveToGlassEffectAnimation(
                base_glass_effect_animation,
                vibration_glass_effect
            );
        const WidgetRenderUsageContext usage_context = BuildUsageContext();
        DrawingTarget()->Clear(palette.window_background);
        if (glass_effect_frame != nullptr) {
            glass_effect_cache.DrawBackground(
                DrawingTarget(),
                panel_background_brush.Get(),
                *glass_effect_frame,
                panel_rect,
                size,
                palette,
                glass_lenses,
                glass_effect_profile,
                glass_effect_animation
            );
        } else {
            DrawingTarget()->FillRectangle(panel_rect, panel_background_brush.Get());
        }
        DrawingTarget()->DrawRectangle(panel_rect, border_brush.Get(), 1.0F);
        DrawWidgetMotionPulse(
            DrawingTarget(),
            panel_rect,
            palette.active_control,
            vibration_glass_effect
        );

        const auto finish_scene = [&]() {
            EndMotionWaveScene(
                size,
                palette,
                vibration_glass_effect,
                wave_composition_active
            );
        };

        if (condensed_layout.has_value()) {
            D2D1_MATRIX_3X2_F title_previous_transform{};
            DrawingTarget()->GetTransform(&title_previous_transform);
            const D2D1_RECT_F logical_title_rect = D2D1::RectF(
                condensed_layout->title_rect.left,
                condensed_layout->title_rect.top,
                condensed_layout->title_rect.left
                    + ((condensed_layout->title_rect.right - condensed_layout->title_rect.left)
                        / kCondensedDisplayScale),
                condensed_layout->title_rect.top + 24.0F
            );
            DrawingTarget()->SetTransform(
                D2D1::Matrix3x2F::Scale(
                    kCondensedDisplayScale,
                    kCondensedDisplayScale,
                    D2D1::Point2F(
                        condensed_layout->title_rect.left,
                        condensed_layout->title_rect.top
                    )
                ) * title_previous_transform
            );
            DrawLeadingTextLine(
                T(IDS_APP_TITLE).c_str(),
                logical_title_rect,
                body_text_format.Get(),
                title_text_brush.Get()
            );
            DrawingTarget()->SetTransform(title_previous_transform);

            const std::wstring five_hour_percent = usage_data_available
                ? FormatOptionalRemainingPercent(snapshot.five_hour_used_percent, snapshot.five_hour_available)
                : std::wstring{};
            const std::wstring weekly_percent = usage_data_available
                ? FormatOptionalRemainingPercent(snapshot.weekly_used_percent, snapshot.weekly_available)
                : std::wstring{};
            std::size_t quota_index = 0;
            const auto draw_quota = [&](
                const std::wstring& label,
                const std::wstring& percent,
                const WidgetRollingNumberFrame& animation,
                bool available,
                float remaining,
                ID2D1Brush* accent_brush
            ) {
                if (quota_index >= condensed_layout->quota_rects.size()) {
                    return;
                }
                const D2D1_RECT_F target_bounds = condensed_layout->quota_rects[quota_index];
                D2D1_MATRIX_3X2_F card_previous_transform{};
                DrawingTarget()->GetTransform(&card_previous_transform);
                DrawingTarget()->SetTransform(
                    D2D1::Matrix3x2F::Scale(
                        kCondensedDisplayScale,
                        kCondensedDisplayScale,
                        D2D1::Point2F(target_bounds.left, target_bounds.top)
                    ) * card_previous_transform
                );
                DrawMinimalUsageIndicator(
                    usage_context,
                    D2D1::RectF(
                        target_bounds.left,
                        target_bounds.top,
                        target_bounds.left + 96.0F,
                        target_bounds.top + 96.0F
                    ),
                    label,
                    percent,
                    animation,
                    available,
                    remaining,
                    accent_brush,
                    false
                );
                DrawCompactTokenActivityStrip(
                    BuildGraphContext(false),
                    token_snapshot,
                    D2D1::RectF(
                        target_bounds.left + 5.0F,
                        target_bounds.top + 75.0F,
                        target_bounds.left + 91.0F,
                        target_bounds.top + 85.0F
                    )
                );
                DrawingTarget()->SetTransform(card_previous_transform);
                ++quota_index;
            };
            if (settings.quota_visibility.five_hour) {
                draw_quota(
                    T(IDS_MENU_QUOTA_5H),
                    five_hour_percent,
                    five_hour_rolling,
                    usage_data_available && snapshot.five_hour_available,
                    usage_data_available && snapshot.five_hour_available
                        ? NormalizeRemainingPercent(snapshot.five_hour_used_percent)
                        : 0.0F,
                    five_hour_remaining_accent_brush.Get()
                );
            }
            if (settings.quota_visibility.weekly) {
                draw_quota(
                    T(IDS_MENU_QUOTA_WEEKLY),
                    weekly_percent,
                    weekly_rolling,
                    usage_data_available && snapshot.weekly_available,
                    usage_data_available && snapshot.weekly_available
                        ? NormalizeRemainingPercent(snapshot.weekly_used_percent)
                        : 0.0F,
                    weekly_remaining_accent_brush.Get()
                );
            }
            for (const WidgetCompactRateLimitSummary& additional :
                BuildCompactAdditionalRateLimits(snapshot, settings.quota_visibility)) {
                draw_quota(
                    additional.label,
                    additional.percent,
                    WidgetRollingNumberFrame{
                        additional.percent,
                        additional.percent,
                        1.0,
                        false
                    },
                    true,
                    additional.remaining,
                    additional.short_window
                        ? five_hour_remaining_accent_brush.Get()
                        : weekly_remaining_accent_brush.Get()
                );
            }
            DrawWidgetTrayHideButton(
                {
                    DrawingTarget(),
                    panel_background_brush.Get(),
                    border_brush.Get(),
                    muted_text_brush.Get(),
                    caption_text_format.Get(),
                },
                tray_button_layout,
                tray_button_interaction
            );
            finish_scene();
            return;
        }

        if (minimal_mode) {
            const D2D1_RECT_F title_rect = D2D1::RectF(content_left, content_top, content_right, content_top + 24.0F);
            DrawTextLine(T(IDS_APP_TITLE).c_str(), title_rect, minimal_title_text_format.Get(), title_text_brush.Get());
        } else {
            const std::wstring title = T(IDS_APP_TITLE);
            const float measured_title_width = MeasureTextWidth(title, title_text_format.Get());
            const float available_header_width = std::max(0.0F, content_right - content_left);
            const float title_width = std::min(
                available_header_width,
                measured_title_width > 0.0F ? measured_title_width : available_header_width
            );
            const D2D1_RECT_F title_rect = D2D1::RectF(
                content_left,
                content_top,
                content_left + title_width,
                content_top + 28.0F
            );
            const D2D1_RECT_F provider_rect = D2D1::RectF(
                std::min(header_content_right, title_rect.right + 10.0F),
                content_top,
                header_content_right,
                content_top + 28.0F
            );
            const float measured_provider_width = MeasureTextWidth(
                FormatProviderSummary(snapshot),
                provider_summary_text_format.Get()
            );
            const float provider_text_left = std::max(
                provider_rect.left,
                provider_rect.right - measured_provider_width
            );
            DrawTextLine(title.c_str(), title_rect, title_text_format.Get(), title_text_brush.Get());
            DrawProviderSummary(usage_context, snapshot, provider_rect);
            const float measured_activity_width = MeasureCaptionTextWidth(activity_label);
            const D2D1_RECT_F activity_label_rect = D2D1::RectF(
                title_rect.right + 10.0F,
                content_top,
                std::max(title_rect.right + 10.0F, provider_text_left - 8.0F),
                content_top + 28.0F
            );
            if (measured_activity_width > 0.0F
                && activity_label_rect.right - activity_label_rect.left
                    >= measured_activity_width) {
                DrawTextLine(
                    activity_label.c_str(),
                    activity_label_rect,
                    caption_text_format.Get(),
                    muted_text_brush.Get()
                );
            }
            DrawWidgetActivityVein(
                {
                    d2d_factory.Get(),
                    DrawingTarget(),
                    palette,
                },
                activity_vein_layout.vein_rect,
                activity_frame,
                WidgetActivityVeinOrientation::Horizontal,
                true
            );
        }

        const std::wstring five_hour_percent = usage_data_available
            ? FormatOptionalRemainingPercent(snapshot.five_hour_used_percent, snapshot.five_hour_available)
            : std::wstring{};

        const std::wstring weekly_percent = usage_data_available
            ? FormatOptionalRemainingPercent(snapshot.weekly_used_percent, snapshot.weekly_available)
            : std::wstring{};

        const std::wstring five_hour_reset = FormatQuotaResetMetadata(
            snapshot.five_hour_reset_at,
            usage_data_available && snapshot.five_hour_available
        );
        const std::wstring weekly_reset = FormatQuotaResetMetadata(
            snapshot.weekly_reset_at,
            usage_data_available && snapshot.weekly_available
        );

        if (minimal_mode) {
            std::size_t quota_index = 0;
            const auto draw_quota = [&](
                const std::wstring& label,
                const std::wstring& percent,
                const WidgetRollingNumberFrame& animation,
                bool available,
                float remaining,
                ID2D1Brush* accent_brush
            ) {
                if (quota_index >= minimal_quota_rects.size()) {
                    return;
                }
                DrawMinimalUsageIndicator(
                    usage_context,
                    minimal_quota_rects[quota_index],
                    label,
                    percent,
                    animation,
                    available,
                    remaining,
                    accent_brush
                );
                ++quota_index;
            };
            if (settings.quota_visibility.five_hour) {
                draw_quota(
                    T(IDS_MENU_QUOTA_5H),
                    five_hour_percent,
                    five_hour_rolling,
                    usage_data_available && snapshot.five_hour_available,
                    usage_data_available && snapshot.five_hour_available
                        ? NormalizeRemainingPercent(snapshot.five_hour_used_percent)
                        : 0.0F,
                    five_hour_remaining_accent_brush.Get()
                );
            }
            if (settings.quota_visibility.weekly) {
                draw_quota(
                    T(IDS_MENU_QUOTA_WEEKLY),
                    weekly_percent,
                    weekly_rolling,
                    usage_data_available && snapshot.weekly_available,
                    usage_data_available && snapshot.weekly_available
                        ? NormalizeRemainingPercent(snapshot.weekly_used_percent)
                        : 0.0F,
                    weekly_remaining_accent_brush.Get()
                );
            }
            for (const WidgetCompactRateLimitSummary& additional :
                BuildCompactAdditionalRateLimits(snapshot, settings.quota_visibility)) {
                draw_quota(
                    additional.label,
                    additional.percent,
                    WidgetRollingNumberFrame{
                        additional.percent,
                        additional.percent,
                        1.0,
                        false
                    },
                    true,
                    additional.remaining,
                    additional.short_window
                        ? five_hour_remaining_accent_brush.Get()
                        : weekly_remaining_accent_brush.Get()
                );
            }

            DrawWidgetTrayHideButton(
                {
                    DrawingTarget(),
                    panel_background_brush.Get(),
                    border_brush.Get(),
                    muted_text_brush.Get(),
                    caption_text_format.Get(),
                },
                tray_button_layout,
                tray_button_interaction
            );

            finish_scene();
            return;
        }

        float quota_row_top = first_usage_top;
        if (settings.quota_visibility.five_hour) {
            DrawUsageRow(
                usage_context,
                T(IDS_USAGE_5H).c_str(),
                five_hour_percent.c_str(),
                five_hour_rolling,
                usage_data_available && snapshot.five_hour_available,
                five_hour_reset.c_str(),
                usage_data_available && snapshot.five_hour_available
                    ? NormalizeRemainingPercent(snapshot.five_hour_used_percent)
                    : 0.0F,
                quota_row_top,
                content_left,
                content_right,
                five_hour_remaining_accent_brush.Get()
            );
            quota_row_top += kRowSpacing;
        }
        if (settings.quota_visibility.weekly) {
            DrawUsageRow(
                usage_context,
                T(IDS_USAGE_WEEK).c_str(),
                weekly_percent.c_str(),
                weekly_rolling,
                usage_data_available && snapshot.weekly_available,
                weekly_reset.c_str(),
                usage_data_available && snapshot.weekly_available
                    ? NormalizeRemainingPercent(snapshot.weekly_used_percent)
                    : 0.0F,
                quota_row_top,
                content_left,
                content_right,
                weekly_remaining_accent_brush.Get()
            );
            quota_row_top += kRowSpacing;
        }

        if (settings.display_mode == WidgetDisplayMode::Complete
            && CountVisibleAdditionalRateLimitRows(snapshot, settings.quota_visibility) > 0) {
            DrawAdditionalRateLimits(
                usage_context,
                snapshot,
                settings.quota_visibility,
                quota_row_top,
                content_left,
                content_right,
                five_hour_remaining_accent_brush.Get(),
                weekly_remaining_accent_brush.Get()
            );
        }

        if (ShouldRenderGraph(settings)) {
            const GraphRange active_graph_range = settings.graph_page == WidgetGraphPage::Tokens
                ? settings.token_graph_range
                : (settings.graph_page == WidgetGraphPage::Activity
                    ? settings.activity_graph_range
                    : (settings.graph_page == WidgetGraphPage::Summary
                        ? settings.summary_graph_range
                        : graph_range));
            const WidgetGraphLayout& active_graph_layout = *graph_layout;
            if (settings.graph_page == WidgetGraphPage::Tokens) {
                DrawTokenUsageGraph(
                    BuildGraphContext(show_graph_header_controls), active_graph_layout, token_snapshot,
                    graph_interaction, settings.token_graph_range
                );
            } else if (settings.graph_page == WidgetGraphPage::Activity) {
                DrawTokenUsageHeatmap(
                    BuildGraphContext(show_graph_header_controls),
                    active_graph_layout,
                    token_snapshot,
                    graph_interaction,
                    settings.activity_graph_range
                );
            } else if (settings.graph_page == WidgetGraphPage::Summary) {
                DrawWidgetKpiSummary(
                    BuildGraphContext(show_graph_header_controls),
                    active_graph_layout,
                    token_snapshot,
                    graph_interaction,
                    settings.summary_graph_range
                );
            } else {
                DrawQuotaGraph(
                    BuildGraphContext(show_graph_header_controls), active_graph_layout, history_store,
                    snapshot, graph_interaction, graph_range, quota_graph_animation
                );
            }
            DrawWidgetGraphSegmentedControl(
                BuildGraphContext(show_graph_header_controls),
                active_graph_layout,
                settings.graph_page,
                graph_interaction
            );
            if (show_graph_header_controls) {
                DrawWidgetGraphCaptureButton(
                    BuildGraphContext(),
                    active_graph_layout,
                    graph_interaction
                );
            }
            if (show_graph_header_controls) {
                DrawWidgetGraphRangeSelector(
                    BuildGraphContext(),
                    active_graph_layout,
                    active_graph_range,
                    settings.graph_page,
                    graph_interaction
                );
            }
            DrawFreshnessStatus(snapshot, active_graph_layout.freshness_status_rect, refreshing_label_dot_count);
        } else {
            const D2D1_RECT_F freshness_status_rect = D2D1::RectF(
                content_left,
                size.height - 24.0F,
                content_right,
                size.height - 10.0F
            );
            DrawFreshnessStatus(snapshot, freshness_status_rect, refreshing_label_dot_count);
        }

        if (snapshot.freshness == UsageFreshness::Error && snapshot.error_message) {
            const D2D1_RECT_F error_rect = D2D1::RectF(
                content_left,
                size.height - 28.0F,
                std::max(content_left, content_right - kFreshnessStatusWidth - 8.0F),
                size.height - 10.0F
            );
            // Largeur sous laquelle un message tronque nuirait au statut.
            constexpr float kMinimumErrorLabelWidth = 80.0F;
            if (error_rect.right - error_rect.left >= kMinimumErrorLabelWidth) {
                const DWRITE_WORD_WRAPPING previous_wrapping = caption_text_format->GetWordWrapping();
                caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                DrawTextLine(
                    snapshot.error_message->c_str(),
                    error_rect,
                    caption_text_format.Get(),
                    muted_text_brush.Get()
                );
                caption_text_format->SetWordWrapping(previous_wrapping);
            }
        }

        DrawWidgetTrayHideButton(
            {
                DrawingTarget(),
                panel_background_brush.Get(),
                border_brush.Get(),
                muted_text_brush.Get(),
                caption_text_format.Get(),
            },
            tray_button_layout,
            tray_button_interaction
        );

        DrawWidgetActivityVeinHint(
            {
                DrawingTarget(),
                panel_background_brush.Get(),
                border_brush.Get(),
                muted_text_brush.Get(),
                caption_text_format.Get(),
            },
            activity_vein_layout,
            activity_interaction,
            activity_hint
        );

        finish_scene();
    }

    // ------------------------------------------------------------------------
    // Dessine uniquement la palette flottante de couleurs.
    //
    // Parametres :
    // - hwnd : handle de la fenetre outil.
    // - settings : reglages visuels et couleurs courants.
    // - interaction : etat souris de la palette.
    // - glass_effect_frame : capture du bureau situee derriere la palette.
    // - vibration_glass_effect : deformation de vibration courante.
    // ------------------------------------------------------------------------
    void RenderColorPanelWindow(
        HWND hwnd,
        const AppSettings& settings,
        const WidgetColorPanelInteraction& interaction,
        const WidgetGlassEffectFrame* glass_effect_frame,
        const WidgetVibrationGlassEffectState& vibration_glass_effect
    ) {
        if (!CreateDeviceResources(hwnd, settings)) {
            return;
        }

        const D2D1_SIZE_F size = render_target->GetSize();
        const Palette palette = ActivePalette(settings);
        const GlassEffectRenderProfile glass_effect_profile = ApplyAnimationsToGlassEffectProfile(
            ApplyVibrationToGlassEffectProfile(
                ApplyUserOpacityToGlassEffectProfile(
                    GlassEffectProfileForAppearance(settings.glass_effect.appearance),
                    settings.background_opacity
                ),
                vibration_glass_effect
            ),
            settings.glass_effect
        );
        const WidgetGlassEffectAnimationSettings base_glass_effect_animation =
            GlassEffectAnimationSettingsForSettings(
                settings.glass_effect,
                GlassEffectAnimationTimeSeconds()
            );
        const bool wave_composition_active = BeginMotionWaveScene(size, vibration_glass_effect);
        const WidgetGlassEffectAnimationSettings glass_effect_animation = wave_composition_active
            ? base_glass_effect_animation
            : ApplyMotionWaveToGlassEffectAnimation(
                base_glass_effect_animation,
                vibration_glass_effect
            );
        const WidgetColorPanelLayout color_layout = BuildWidgetColorPanelLayout(size);
        std::vector<WidgetGlassEffectLens> color_lenses;
        AppendWidgetGlassElementLens(
            color_lenses,
            D2D1::RectF(
                color_layout.panel_rect.left + 14.0F,
                color_layout.panel_rect.top + 6.0F,
                color_layout.close_button_rect.left - 8.0F,
                color_layout.panel_rect.top + 30.0F
            ),
            glass_effect_profile,
            5.0F
        );
        for (std::size_t index = 0; index < color_layout.color_swatch_rects.size(); ++index) {
            AppendWidgetGlassElementLens(
                color_lenses,
                color_layout.color_swatch_rects[index],
                glass_effect_profile,
                kColorPanelSwatchRadius
            );
            AppendWidgetGlassElementLens(
                color_lenses,
                color_layout.color_label_rects[index],
                glass_effect_profile,
                4.0F
            );
        }
        AppendWidgetGlassElementLens(
            color_lenses,
            color_layout.random_button_rect,
            glass_effect_profile,
            5.0F
        );
        AppendWidgetGlassElementLens(
            color_lenses,
            color_layout.close_button_rect,
            glass_effect_profile,
            5.0F
        );
        DrawingTarget()->Clear(palette.window_background);
        DrawColorPanel(
            BuildColorPanelContext(),
            size,
            settings,
            interaction,
            glass_effect_frame,
            color_lenses,
            glass_effect_profile,
            glass_effect_animation,
            palette
        );
        const D2D1_RECT_F color_panel_rect = D2D1::RectF(
            0.5F,
            0.5F,
            std::max(0.5F, size.width - 0.5F),
            std::max(0.5F, size.height - 0.5F)
        );
        DrawingTarget()->DrawRectangle(
            color_panel_rect,
            border_brush.Get(),
            1.0F
        );
        DrawWidgetMotionPulse(
            DrawingTarget(),
            color_panel_rect,
            palette.active_control,
            vibration_glass_effect
        );
        EndMotionWaveScene(
            size,
            palette,
            vibration_glass_effect,
            wave_composition_active
        );
    }
};

// ----------------------------------------------------------------------------
// Cree le renderer et son etat interne.
// ----------------------------------------------------------------------------
WidgetRenderer::WidgetRenderer()
    : impl_(std::make_unique<Impl>()) {
}

// ----------------------------------------------------------------------------
// Libere les ressources graphiques encore conservees.
// ----------------------------------------------------------------------------
WidgetRenderer::~WidgetRenderer() = default;

// ----------------------------------------------------------------------------
// Initialise les factories Direct2D et DirectWrite independantes de la fenetre.
//
// Retour :
// - true si les factories sont disponibles.
// - false sinon.
// ----------------------------------------------------------------------------
bool WidgetRenderer::Initialize() {
    return impl_->Initialize();
}

// ----------------------------------------------------------------------------
// Force la recreation des ressources graphiques au prochain rendu.
// ----------------------------------------------------------------------------
void WidgetRenderer::DiscardDeviceResources() {
    impl_->DiscardDeviceResources();
}

// ----------------------------------------------------------------------------
// Redimensionne le render target Direct2D lorsque la fenetre change.
//
// Parametres :
// - hwnd : handle de la fenetre redimensionnee.
// ----------------------------------------------------------------------------
void WidgetRenderer::Resize(HWND hwnd) {
    impl_->Resize(hwnd);
}

// ----------------------------------------------------------------------------
// Force la recreation des ressources graphiques et invalide la fenetre.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// ----------------------------------------------------------------------------
void WidgetRenderer::RefreshVisualResources(HWND hwnd) {
    DiscardDeviceResources();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Mesure un texte avec le format partage par les titres de quotas.
//
// Parametres :
// - text : texte localise a mesurer.
//
// Retour :
// - largeur DirectWrite en DIPs, ou zero si la mesure echoue.
// ----------------------------------------------------------------------------
float WidgetRenderer::MeasureGraphTitleTextWidth(const std::wstring& text) const {
    return impl_->MeasureGraphTitleTextWidth(text);
}

// ----------------------------------------------------------------------------
// Mesure un texte avec le format partage par les hints compacts.
//
// Parametres :
// - text : texte localise a mesurer.
//
// Retour :
// - largeur DirectWrite en DIPs, ou zero si la mesure echoue.
// ----------------------------------------------------------------------------
float WidgetRenderer::MeasureCaptionTextWidth(const std::wstring& text) const {
    return impl_->MeasureCaptionTextWidth(text);
}

// ----------------------------------------------------------------------------
// Rend l'onglet graphique actif hors ecran et le copie dans le presse-papiers.
//
// Parametres :
// - hwnd : fenetre qui fournit les ressources, le DPI et le presse-papiers.
// - settings : page, plage et palette actuellement selectionnees.
// - snapshot : quotas courants necessaires aux vues et au layout.
// - token_snapshot : donnees locales des vues Tokens, Activite et Bilan.
// - history_store : historique utilise par la vue Quotas.
// - graph_range : plage active de la vue Quotas.
// - quota_graph_animation : serie actuellement visible dans la vue Quotas.
//
// Retour :
// - true si l'image sans controles ni effets Glass a ete copiee.
// ----------------------------------------------------------------------------
bool WidgetRenderer::CopyActiveGraphTabToClipboard(
    HWND hwnd,
    const AppSettings& settings,
    const UsageSnapshot& snapshot,
    const TokenUsageSnapshot& token_snapshot,
    const UsageHistoryStore& history_store,
    GraphRange graph_range,
    const WidgetQuotaGraphAnimationFrame& quota_graph_animation
) {
    return impl_->CopyActiveGraphTabToClipboard(
        hwnd,
        settings,
        snapshot,
        token_snapshot,
        history_store,
        graph_range,
        quota_graph_animation
    );
}

// ----------------------------------------------------------------------------
// Dessine l'interface du widget.
//
// Parametres :
// - hwnd : handle de la fenetre a redessiner.
// - settings : reglages visuels et d'affichage courants.
// - snapshot : releve d'usage courant.
// - history_store : historique local utilise pour le graphe.
// - graph_range : plage temporelle du graphe.
// - activity_frame : etats stabilises de l'activite Codex.
// - activity_interaction : survol global du filament.
// ----------------------------------------------------------------------------
void WidgetRenderer::Render(
    HWND hwnd,
    const AppSettings& settings,
    const UsageSnapshot& snapshot,
    const TokenUsageSnapshot& token_snapshot,
    const WidgetGraphInteraction& graph_interaction,
    const WidgetTrayHideButtonInteraction& tray_button_interaction,
    const UsageHistoryStore& history_store,
    GraphRange graph_range,
    const WidgetActivityFrame& activity_frame,
    const WidgetActivityVeinInteraction& activity_interaction,
    const WidgetQuotaGraphAnimationFrame& quota_graph_animation,
    const WidgetGlassEffectFrame* glass_effect_frame,
    const WidgetVibrationGlassEffectState& vibration_glass_effect,
    const WidgetRollingNumberFrame& five_hour_rolling,
    const WidgetRollingNumberFrame& weekly_rolling,
    std::size_t refreshing_label_dot_count,
    bool show_graph_header_controls
) {
    impl_->Render(
        hwnd,
        settings,
        snapshot,
        token_snapshot,
        graph_interaction,
        tray_button_interaction,
        history_store,
        graph_range,
        activity_frame,
        activity_interaction,
        quota_graph_animation,
        glass_effect_frame,
        vibration_glass_effect,
        five_hour_rolling,
        weekly_rolling,
        refreshing_label_dot_count,
        show_graph_header_controls
    );
}

// ----------------------------------------------------------------------------
// Dessine uniquement la palette flottante de personnalisation des couleurs.
//
// Parametres :
// - hwnd : handle de la fenetre outil.
// - settings : reglages visuels et couleurs courants.
// - interaction : etat souris de la palette.
// - glass_effect_frame : capture du bureau situee derriere la palette.
// - vibration_glass_effect : deformation de vibration courante.
// ----------------------------------------------------------------------------
void WidgetRenderer::RenderColorPanelWindow(
    HWND hwnd,
    const AppSettings& settings,
    const WidgetColorPanelInteraction& interaction,
    const WidgetGlassEffectFrame* glass_effect_frame,
    const WidgetVibrationGlassEffectState& vibration_glass_effect
) {
    impl_->RenderColorPanelWindow(hwnd, settings, interaction, glass_effect_frame, vibration_glass_effect);
}

// ============================================================================
// Codex Glass - Implementation du rendu des usages
// ----------------------------------------------------------------------------
// Ce fichier dessine les barres de quota, le resume minimal et l'animation des
// chiffres, sans connaitre le graphe ni le panneau couleurs.
// ============================================================================

#include "WidgetRenderUsage.h"

#include "WidgetRenderConstants.h"
#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"
#include "../usage/UsageFormatting.h"

#include <algorithm>
#include <cwchar>
#include <wrl/client.h>
#include <windows.h>

namespace {

// Opacite appliquee a une valeur de quota indisponible.
constexpr float kUnavailableValueOpacity = 0.62F;

// ----------------------------------------------------------------------------
// Dessine un texte DirectWrite dans le rectangle donne.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - text : texte Unicode a dessiner.
// - layout_rect : rectangle de destination en DIPs.
// - format : format DirectWrite a utiliser.
// - brush : brosse Direct2D a utiliser.
// ----------------------------------------------------------------------------
void DrawTextLine(
    const WidgetRenderUsageContext& context,
    const wchar_t* text,
    const D2D1_RECT_F& layout_rect,
    IDWriteTextFormat* format,
    ID2D1Brush* brush
) {
    context.render_target->DrawTextW(
        text,
        static_cast<UINT32>(wcslen(text)),
        format,
        layout_rect,
        brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
}

// ----------------------------------------------------------------------------
// Indique si un caractere doit recevoir l'animation de roulement.
//
// Parametres :
// - previous_char : ancien caractere.
// - current_char : nouveau caractere.
//
// Retour :
// - true si les deux caracteres sont des chiffres differents.
// ----------------------------------------------------------------------------
bool ShouldRollCharacter(wchar_t previous_char, wchar_t current_char) {
    return previous_char != current_char
        && previous_char >= L'0'
        && previous_char <= L'9'
        && current_char >= L'0'
        && current_char <= L'9';
}

// ----------------------------------------------------------------------------
// Dessine une valeur avec roulement vertical des chiffres modifies.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - frame : etat d'animation courant.
// - layout_rect : rectangle de destination.
// - format : format DirectWrite a utiliser.
// - brush : brosse du texte.
// ----------------------------------------------------------------------------
void DrawRollingNumber(
    const WidgetRenderUsageContext& context,
    const WidgetRollingNumberFrame& frame,
    const D2D1_RECT_F& layout_rect,
    IDWriteTextFormat* format,
    ID2D1Brush* brush
) {
    if (!frame.active || frame.previous_text.size() != frame.current_text.size()) {
        DrawTextLine(context, frame.current_text.c_str(), layout_rect, format, brush);
        return;
    }

    const size_t character_count = frame.current_text.size();
    if (character_count == 0) {
        return;
    }

    const float text_height = layout_rect.bottom - layout_rect.top;
    const float offset = text_height * static_cast<float>(std::clamp(frame.progress, 0.0, 1.0));
    Microsoft::WRL::ComPtr<IDWriteTextLayout> text_layout;
    const HRESULT layout_result = context.dwrite_factory->CreateTextLayout(
        frame.current_text.c_str(),
        static_cast<UINT32>(character_count),
        format,
        std::max(1.0F, layout_rect.right - layout_rect.left),
        std::max(1.0F, layout_rect.bottom - layout_rect.top),
        text_layout.GetAddressOf()
    );
    if (FAILED(layout_result)) {
        DrawTextLine(context, frame.current_text.c_str(), layout_rect, format, brush);
        return;
    }

    context.render_target->PushAxisAlignedClip(layout_rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (size_t index = 0; index < character_count; ++index) {
        FLOAT character_x = 0.0F;
        FLOAT character_y = 0.0F;
        DWRITE_HIT_TEST_METRICS metrics{};
        if (FAILED(text_layout->HitTestTextPosition(
                static_cast<UINT32>(index),
                FALSE,
                &character_x,
                &character_y,
                &metrics
            ))) {
            continue;
        }

        const float cell_left = layout_rect.left + metrics.left;
        const float cell_right = cell_left + std::max(1.0F, metrics.width);
        const wchar_t previous_char = frame.previous_text[index];
        const wchar_t current_char = frame.current_text[index];
        const bool roll_character = ShouldRollCharacter(previous_char, current_char);

        wchar_t text[2]{current_char, L'\0'};
        D2D1_RECT_F current_rect = D2D1::RectF(cell_left, layout_rect.top, cell_right, layout_rect.bottom);
        if (roll_character) {
            current_rect.top -= text_height - offset;
            current_rect.bottom -= text_height - offset;
        }
        DrawTextLine(context, text, current_rect, format, brush);

        if (roll_character) {
            text[0] = previous_char;
            const D2D1_RECT_F previous_rect = D2D1::RectF(
                cell_left,
                layout_rect.top + offset,
                cell_right,
                layout_rect.bottom + offset
            );
            DrawTextLine(context, text, previous_rect, format, brush);
        }
    }

    context.render_target->PopAxisAlignedClip();
}

// ----------------------------------------------------------------------------
// Dessine une cellule minimale de pourcentage.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - label : libelle court de la cellule.
// - percent_text : pourcentage a afficher.
// - percent_animation : etat d'animation des chiffres.
// - value_available : indique si la valeur est exploitable.
// - usage : progression restante normalisee.
// - bounds : rectangle de destination en DIPs.
// - accent_brush : couleur d'accent de la valeur et de la progression.
// - draw_progress : autorise le dessin de la barre de progression.
// ----------------------------------------------------------------------------
void DrawMinimalPercentCell(
    const WidgetRenderUsageContext& context,
    const wchar_t* label,
    const wchar_t* percent_text,
    const WidgetRollingNumberFrame& percent_animation,
    bool value_available,
    float usage,
    const D2D1_RECT_F& bounds,
    ID2D1Brush* accent_brush,
    bool draw_progress
) {
    // Rayon discret des cartes du mode minimal en DIPs.
    constexpr float card_radius = 6.0F;

    // Marge horizontale interne des cartes du mode minimal en DIPs.
    constexpr float horizontal_inset = 5.0F;

    // Epaisseur de la mini-barre de progression en DIPs.
    constexpr float mini_bar_height = 3.0F;

    const D2D1_ROUNDED_RECT card{bounds, card_radius, card_radius};
    const D2D1_ROUNDED_RECT outer_shadow{
        D2D1::RectF(bounds.left - 2.0F, bounds.top, bounds.right + 2.0F, bounds.bottom + 3.0F),
        card_radius + 2.0F,
        card_radius + 2.0F
    };
    const D2D1_ROUNDED_RECT inner_shadow{
        D2D1::RectF(bounds.left - 1.0F, bounds.top + 1.0F, bounds.right + 1.0F, bounds.bottom + 2.0F),
        card_radius + 1.0F,
        card_radius + 1.0F
    };
    const float shadow_opacity = context.minimal_card_shadow_brush->GetOpacity();
    context.minimal_card_shadow_brush->SetOpacity(0.68F);
    context.render_target->FillRoundedRectangle(outer_shadow, context.minimal_card_shadow_brush);
    context.minimal_card_shadow_brush->SetOpacity(1.0F);
    context.render_target->FillRoundedRectangle(inner_shadow, context.minimal_card_shadow_brush);
    context.minimal_card_shadow_brush->SetOpacity(shadow_opacity);
    context.render_target->FillRoundedRectangle(card, context.minimal_card_background_brush);
    context.render_target->DrawRoundedRectangle(card, context.border_brush, 1.0F);

    const D2D1_RECT_F label_rect = D2D1::RectF(
        bounds.left + horizontal_inset,
        bounds.top + 12.0F,
        bounds.right - horizontal_inset,
        bounds.top + 30.0F
    );
    const D2D1_RECT_F value_rect = D2D1::RectF(
        bounds.left + horizontal_inset,
        bounds.top + 29.0F,
        bounds.right - horizontal_inset,
        bounds.bottom - 27.0F
    );
    const D2D1_RECT_F progress_rect = D2D1::RectF(
        bounds.left + horizontal_inset,
        bounds.bottom - 17.0F,
        bounds.right - horizontal_inset,
        bounds.bottom - 17.0F + mini_bar_height
    );

    DrawTextLine(context, label, label_rect, context.minimal_label_text_format, context.muted_text_brush);
    ID2D1Brush* value_brush = value_available ? accent_brush : context.muted_text_brush;
    const float value_opacity = value_brush->GetOpacity();
    if (!value_available) {
        value_brush->SetOpacity(kUnavailableValueOpacity);
    }
    DrawRollingNumber(
        context,
        value_available && percent_animation.active
            ? percent_animation
            : WidgetRollingNumberFrame{percent_text, percent_text, 1.0, false},
        value_rect,
        context.minimal_value_text_format,
        value_brush
    );
    value_brush->SetOpacity(value_opacity);
    if (draw_progress) {
        DrawProgressBar(context, progress_rect, usage, accent_brush);
    }
}

// ----------------------------------------------------------------------------
// Dessine une progression verticale remplie depuis le bas.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - bounds : rail vertical complet en DIPs.
// - value : valeur normalisee entre zero et un.
// - fill_brush : brosse utilisee pour la partie remplie.
// ----------------------------------------------------------------------------
void DrawVerticalProgressBar(
    const WidgetRenderUsageContext& context,
    const D2D1_RECT_F& bounds,
    float value,
    ID2D1Brush* fill_brush
) {
    const float clamped_value = std::clamp(value, 0.0F, 1.0F);
    const float height = bounds.bottom - bounds.top;
    const float radius = (bounds.right - bounds.left) * 0.5F;
    context.render_target->FillRoundedRectangle(
        D2D1::RoundedRect(bounds, radius, radius),
        context.progress_background_brush
    );
    if (clamped_value <= 0.0F) {
        return;
    }

    D2D1_RECT_F filled_bounds = bounds;
    filled_bounds.top = bounds.bottom - height * clamped_value;
    context.render_target->FillRoundedRectangle(
        D2D1::RoundedRect(filled_bounds, radius, radius),
        fill_brush
    );
}

// ----------------------------------------------------------------------------
// Dessine une capsule de quota qui adapte son contenu a son orientation.
//
// Parametres :
// - context : ressources de rendu necessaires.
// - label : libelle court de la capsule.
// - percent_text : pourcentage a afficher.
// - percent_animation : etat d'animation des chiffres.
// - value_available : indique si la valeur est exploitable.
// - usage : progression restante normalisee.
// - bounds : rectangle de destination en DIPs.
// - accent_brush : couleur d'accent de la valeur et de la progression.
// ----------------------------------------------------------------------------
void DrawCondensedPercentCell(
    const WidgetRenderUsageContext& context,
    const wchar_t* label,
    const wchar_t* percent_text,
    const WidgetRollingNumberFrame& percent_animation,
    bool value_available,
    float usage,
    const D2D1_RECT_F& bounds,
    ID2D1Brush* accent_brush
) {
    // Rayon doux commun aux capsules compactes.
    constexpr float card_radius = 8.0F;

    // Marge interne preservant la legerete des capsules.
    constexpr float inset = 10.0F;

    // Epaisseur de la progression compacte.
    constexpr float progress_height = 3.0F;

    const float height = bounds.bottom - bounds.top;
    const bool horizontal_capsule = height <= 60.0F;
    const D2D1_ROUNDED_RECT card{bounds, card_radius, card_radius};
    const D2D1_ROUNDED_RECT shadow{
        D2D1::RectF(bounds.left, bounds.top + 2.0F, bounds.right, bounds.bottom + 3.0F),
        card_radius,
        card_radius
    };
    context.render_target->FillRoundedRectangle(shadow, context.minimal_card_shadow_brush);
    context.render_target->FillRoundedRectangle(card, context.minimal_card_background_brush);
    context.render_target->DrawRoundedRectangle(card, context.border_brush, 1.0F);

    D2D1_RECT_F label_rect{};
    D2D1_RECT_F value_rect{};
    if (horizontal_capsule) {
        const float middle = bounds.left + ((bounds.right - bounds.left) * 0.5F);
        label_rect = D2D1::RectF(
            bounds.left + 6.0F,
            bounds.top + 6.0F,
            middle + 4.0F,
            bounds.bottom - 12.0F
        );
        value_rect = D2D1::RectF(
            middle + 4.0F,
            bounds.top + 5.0F,
            bounds.right - 6.0F,
            bounds.bottom - 12.0F
        );
    } else {
        label_rect = D2D1::RectF(
            bounds.left + 24.0F,
            bounds.top + 11.0F,
            bounds.right - inset,
            bounds.top + 31.0F
        );
        value_rect = D2D1::RectF(
            bounds.left + 24.0F,
            bounds.top + 33.0F,
            bounds.right - inset,
            bounds.bottom - 12.0F
        );
    }

    DrawTextLine(context, label, label_rect, context.minimal_label_text_format, context.muted_text_brush);
    ID2D1Brush* value_brush = value_available ? accent_brush : context.muted_text_brush;
    const float value_opacity = value_brush->GetOpacity();
    if (!value_available) {
        value_brush->SetOpacity(kUnavailableValueOpacity);
    }
    DrawRollingNumber(
        context,
        value_available && percent_animation.active
            ? percent_animation
            : WidgetRollingNumberFrame{percent_text, percent_text, 1.0, false},
        value_rect,
        context.minimal_value_text_format,
        value_brush
    );
    value_brush->SetOpacity(value_opacity);
    if (horizontal_capsule) {
        const D2D1_RECT_F progress_rect = D2D1::RectF(
            bounds.left + inset,
            bounds.bottom - 12.0F,
            bounds.right - inset,
            bounds.bottom - 12.0F + progress_height
        );
        DrawProgressBar(context, progress_rect, usage, accent_brush);
    } else {
        const D2D1_RECT_F progress_rect = D2D1::RectF(
            bounds.left + 10.0F,
            bounds.top + 11.0F,
            bounds.left + 14.0F,
            bounds.bottom - 11.0F
        );
        DrawVerticalProgressBar(context, progress_rect, usage, accent_brush);
    }
}

}

// ----------------------------------------------------------------------------
// Anime le libelle de rafraichissement avec un, deux puis trois points.
//
// Parametres :
// - snapshot : releve d'usage courant.
//
// Retour :
// - libelle d'etat pret a dessiner.
// ----------------------------------------------------------------------------
std::wstring FormatAnimatedFreshnessLabel(const UsageSnapshot& snapshot, std::size_t dot_count) {
    std::wstring label = FormatFreshnessLabel(snapshot);
    if (snapshot.freshness != UsageFreshness::Refreshing) {
        return label;
    }

    label.append(std::clamp<std::size_t>(dot_count, 1U, 3U), L'.');
    return label;
}

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
) {
    const float clamped_value = std::clamp(value, 0.0F, 1.0F);
    const float width = bounds.right - bounds.left;
    D2D1_RECT_F filled_bounds = bounds;
    filled_bounds.right = bounds.left + (width * clamped_value);

    const D2D1_ROUNDED_RECT background{
        bounds,
        kProgressCornerRadius,
        kProgressCornerRadius,
    };
    context.render_target->FillRoundedRectangle(background, context.progress_background_brush);

    if (filled_bounds.right > filled_bounds.left) {
        const D2D1_ROUNDED_RECT foreground{
            filled_bounds,
            kProgressCornerRadius,
            kProgressCornerRadius,
        };
        context.render_target->FillRoundedRectangle(foreground, fill_brush);
    }
}

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
) {
    const D2D1_RECT_F label_rect = D2D1::RectF(left, top, right - 64.0F, top + 20.0F);
    const D2D1_RECT_F percent_rect = D2D1::RectF(right - 54.0F, top, right, top + 20.0F);
    const D2D1_RECT_F progress_rect = D2D1::RectF(left, top + 24.0F, right, top + 24.0F + kProgressBarHeight);
    const D2D1_RECT_F reset_rect = D2D1::RectF(left, top + 36.0F, right, top + 54.0F);

    DrawTextLine(context, label, label_rect, context.body_text_format, context.body_text_brush);
    ID2D1Brush* value_brush = value_available ? context.body_text_brush : context.muted_text_brush;
    if (!value_available) {
        value_brush->SetOpacity(kUnavailableValueOpacity);
    }
    DrawRollingNumber(
        context,
        value_available && percent_animation.active
            ? percent_animation
            : WidgetRollingNumberFrame{percent_text, percent_text, 1.0, false},
        percent_rect,
        context.usage_value_text_format,
        value_brush
    );
    if (!value_available) {
        value_brush->SetOpacity(1.0F);
    }
    DrawProgressBar(context, progress_rect, usage, progress_brush);
    DrawTextLine(context, reset_text, reset_rect, context.caption_text_format, context.muted_text_brush);
}

// ----------------------------------------------------------------------------
// Dessine une carte unique dans la grille du mode minimal.
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
    bool draw_progress
) {
    DrawMinimalPercentCell(
        context,
        label.c_str(),
        percent.c_str(),
        animation,
        available,
        usage,
        bounds,
        accent_brush,
        draw_progress
    );
}

// ----------------------------------------------------------------------------
// Dessine un indicateur unique dans une geometrie condensee.
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
) {
    DrawCondensedPercentCell(
        context,
        label.c_str(),
        percent.c_str(),
        animation,
        available,
        usage,
        bounds,
        accent_brush
    );
}

// ----------------------------------------------------------------------------
// Dessine un quota condense sous forme de texte, sans carte ni progression.
// ----------------------------------------------------------------------------
void DrawCondensedUsageValue(
    const WidgetRenderUsageContext& context,
    const D2D1_RECT_F& bounds,
    const std::wstring& label,
    const std::wstring& percent,
    const WidgetRollingNumberFrame& animation,
    bool available,
    ID2D1Brush* accent_brush
) {
    const DWRITE_TEXT_ALIGNMENT previous_label_alignment =
        context.minimal_label_text_format->GetTextAlignment();
    const DWRITE_TEXT_ALIGNMENT previous_value_alignment =
        context.minimal_value_text_format->GetTextAlignment();
    context.minimal_label_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    context.minimal_value_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);

    const float split = bounds.left + ((bounds.right - bounds.left) * 0.64F);
    DrawTextLine(
        context,
        label.c_str(),
        D2D1::RectF(bounds.left, bounds.top, split, bounds.bottom),
        context.minimal_label_text_format,
        context.muted_text_brush
    );
    ID2D1Brush* value_brush = available ? accent_brush : context.muted_text_brush;
    const float previous_opacity = value_brush->GetOpacity();
    if (!available) {
        value_brush->SetOpacity(kUnavailableValueOpacity);
    }
    DrawRollingNumber(
        context,
        available && animation.active
            ? animation
            : WidgetRollingNumberFrame{percent, percent, 1.0, false},
        D2D1::RectF(split, bounds.top, bounds.right, bounds.bottom),
        context.minimal_value_text_format,
        value_brush
    );
    value_brush->SetOpacity(previous_opacity);
    context.minimal_label_text_format->SetTextAlignment(previous_label_alignment);
    context.minimal_value_text_format->SetTextAlignment(previous_value_alignment);
}

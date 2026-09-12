// ============================================================================
// Codex Glass - Implementation du rendu de la veine d'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier compose un filament organique, un halo discret et au plus trois
// impulsions visibles sans lire ni afficher d'identifiant de session.
// ============================================================================

#include "WidgetRenderActivityVein.h"

#include "WidgetRenderConstants.h"
#include "../settings/AppSettings.h"

#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

using Microsoft::WRL::ComPtr;

namespace {

// Nombre de segments utilises pour lisser le filament organique.
constexpr std::size_t kVeinSegmentCount = 48U;

// Nombre maximal d'impulsions dessinees individuellement par defaut.
constexpr std::size_t kDefaultMaximumDistinctPulseCount = 10U;

// Constante circulaire utilisee pour les ondulations et respirations.
constexpr float kPi = 3.14159265358979323846F;

// Largeur maximale du halo diffus du filament.
constexpr float kOuterHaloWidth = 7.0F;

// Largeur du halo proche du coeur du filament.
constexpr float kInnerHaloWidth = 3.2F;

// Largeur du coeur net du filament.
constexpr float kCoreWidth = 1.05F;

// Decalage transversal du reflet secondaire.
constexpr float kSecondaryReflectionOffset = 0.65F;

// Vitesse calme d'une impulsion correspondant a un outil.
constexpr float kToolTravelSpeed = 0.19F;

// Vitesse de respiration de l'etat de reflexion.
constexpr float kThinkingBreathSpeed = 1.35F;

// Vitesse de derive tres lente d'une impulsion de reflexion.
constexpr float kThinkingTravelSpeed = 0.045F;

// Vitesse du reflet qui matérialise la circulation globale dans la veine.
constexpr float kVeinFlowSpeed = 0.105F;

// Hauteur du hint global de la veine.
constexpr float kActivityHintHeight = 24.0F;

// Espace vertical entre le filament et son hint.
constexpr float kActivityHintGap = 5.0F;

// Marge minimale du hint par rapport aux bords de la fenetre.
constexpr float kActivityHintEdgeMargin = 6.0F;

// Rayon commun au fond et au contour du hint.
constexpr float kActivityHintCornerRadius = 4.0F;

// Marge de confort du hit-test autour du filament fin.
constexpr float kActivityVeinHitPadding = 4.0F;

// ----------------------------------------------------------------------------
// Decrit l'apparence instantanee d'une impulsion.
// ----------------------------------------------------------------------------
struct PulseVisual {
    float position = 0.0F;
    float intensity = 0.0F;
    float scale = 1.0F;
    bool alert = false;
};

// ----------------------------------------------------------------------------
// Interpole deux couleurs sans introduire de teinte independante du theme.
//
// Parametres :
// - first : couleur de depart.
// - second : couleur d'arrivee.
// - amount : poids normalise de la seconde couleur.
//
// Retour :
// - couleur interpolee avec une opacite unitaire.
// ----------------------------------------------------------------------------
D2D1_COLOR_F BlendVeinColor(
    const D2D1_COLOR_F& first,
    const D2D1_COLOR_F& second,
    float amount
) {
    const float safe_amount = std::clamp(amount, 0.0F, 1.0F);
    const float inverse = 1.0F - safe_amount;
    return D2D1::ColorF(
        (first.r * inverse) + (second.r * safe_amount),
        (first.g * inverse) + (second.g * safe_amount),
        (first.b * inverse) + (second.b * safe_amount),
        1.0F
    );
}

// ----------------------------------------------------------------------------
// Calcule l'ondulation stable du filament a une position normalisee.
//
// Parametres :
// - progress : position entre le debut et la fin de la veine.
//
// Retour :
// - decalage transversal normalise comportant trois renflements doux.
// ----------------------------------------------------------------------------
float OrganicVeinOffset(
    float progress,
    float animation_phase,
    float animation_strength
) {
    const float stable_shape = (std::sin((progress * 2.0F * kPi) + 0.35F) * 0.46F)
        + (std::sin((progress * 5.0F * kPi) - 0.70F) * 0.20F)
        + (std::sin((progress * 9.0F * kPi) + 0.20F) * 0.08F);
    const float living_shape = (std::sin(
        (progress * 4.0F * kPi) - (animation_phase * 1.35F)
    ) * 0.52F) + (std::sin(
        (progress * 7.0F * kPi) + (animation_phase * 0.82F)
    ) * 0.24F);
    return stable_shape + (living_shape * std::clamp(animation_strength, 0.0F, 1.0F));
}

// ----------------------------------------------------------------------------
// Retourne un point du filament dans son orientation courante.
//
// Parametres :
// - bounds : zone de dessin de la veine.
// - orientation : axe principal du filament.
// - progress : position normalisee le long de cet axe.
// - reflection_offset : decalage transversal supplementaire du reflet.
// - animation_phase : temps monotone animant doucement la courbe.
// - animation_strength : poids normalise de la deformation vivante.
//
// Retour :
// - point Direct2D appartenant a la courbe organique.
// ----------------------------------------------------------------------------
D2D1_POINT_2F VeinPointAt(
    const D2D1_RECT_F& bounds,
    WidgetActivityVeinOrientation orientation,
    float progress,
    float reflection_offset,
    float animation_phase,
    float animation_strength
) {
    const float safe_progress = std::clamp(progress, 0.0F, 1.0F);
    const float width = std::max(0.0F, bounds.right - bounds.left);
    const float height = std::max(0.0F, bounds.bottom - bounds.top);
    const float cross_extent = orientation == WidgetActivityVeinOrientation::Horizontal
        ? height
        : width;
    const float amplitude = std::min(2.4F, cross_extent * 0.22F);
    const float organic_offset = OrganicVeinOffset(
        safe_progress,
        animation_phase,
        animation_strength
    ) * amplitude
        + reflection_offset;
    if (orientation == WidgetActivityVeinOrientation::Horizontal) {
        return D2D1::Point2F(
            bounds.left + (width * safe_progress),
            ((bounds.top + bounds.bottom) * 0.5F) + organic_offset
        );
    }
    return D2D1::Point2F(
        ((bounds.left + bounds.right) * 0.5F) + organic_offset,
        bounds.top + (height * safe_progress)
    );
}

// ----------------------------------------------------------------------------
// Construit la geometrie ouverte d'un filament ou de son reflet.
//
// Parametres :
// - factory : factory Direct2D proprietaire de la geometrie.
// - bounds : zone de dessin de la veine.
// - orientation : axe principal du filament.
// - reflection_offset : decalage transversal du chemin demande.
// - animation_phase : temps monotone de la deformation vivante.
// - animation_strength : poids normalise de cette deformation.
//
// Retour :
// - geometrie prete a dessiner, ou nullptr si Direct2D refuse sa creation.
// ----------------------------------------------------------------------------
ComPtr<ID2D1PathGeometry> BuildVeinGeometry(
    ID2D1Factory* factory,
    const D2D1_RECT_F& bounds,
    WidgetActivityVeinOrientation orientation,
    float reflection_offset,
    float animation_phase,
    float animation_strength
) {
    ComPtr<ID2D1PathGeometry> geometry;
    if (factory == nullptr
        || FAILED(factory->CreatePathGeometry(geometry.GetAddressOf()))) {
        return nullptr;
    }

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(geometry->Open(sink.GetAddressOf()))) {
        return nullptr;
    }
    sink->BeginFigure(
        VeinPointAt(
            bounds,
            orientation,
            0.0F,
            reflection_offset,
            animation_phase,
            animation_strength
        ),
        D2D1_FIGURE_BEGIN_HOLLOW
    );
    for (std::size_t index = 1; index <= kVeinSegmentCount; ++index) {
        const float progress = static_cast<float>(index)
            / static_cast<float>(kVeinSegmentCount);
        sink->AddLine(VeinPointAt(
            bounds,
            orientation,
            progress,
            reflection_offset,
            animation_phase,
            animation_strength
        ));
    }
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    if (FAILED(sink->Close())) {
        return nullptr;
    }
    return geometry;
}

// ----------------------------------------------------------------------------
// Calcule l'apparence d'une impulsion a partir de son etat stabilise.
//
// Parametres :
// - pulse : impulsion fournie par le controleur.
// - shared_phase : phase monotone commune a toutes les sessions.
// - animation_enabled : autorise ou fige les variations temporelles.
//
// Retour :
// - position, intensite, taille et famille de couleur de l'impulsion.
// ----------------------------------------------------------------------------
PulseVisual CalculatePulseVisual(
    const WidgetActivityPulseFrame& pulse,
    float shared_phase,
    bool animation_enabled
) {
    const float phase = animation_enabled ? shared_phase : 0.0F;
    PulseVisual visual{};
    visual.position = pulse.phase_offset;
    const float entrance = std::clamp(pulse.transition_progress, 0.0F, 1.0F);
    switch (pulse.state) {
    case CodexActivityState::Thinking: {
        visual.position = animation_enabled
            ? std::fmod(pulse.phase_offset + (phase * kThinkingTravelSpeed), 1.0F)
            : pulse.phase_offset;
        const float breath = animation_enabled
            ? 0.5F + (0.5F * std::sin(
                (phase * kThinkingBreathSpeed) + (pulse.phase_offset * 2.0F * kPi)
            ))
            : 0.55F;
        visual.intensity = entrance * (0.34F + (breath * 0.22F));
        visual.scale = 0.82F + (breath * 0.20F);
        break;
    }
    case CodexActivityState::ToolRunning:
        visual.position = animation_enabled
            ? std::fmod(pulse.phase_offset + (phase * kToolTravelSpeed), 1.0F)
            : pulse.phase_offset;
        visual.intensity = entrance * 0.92F;
        visual.scale = 1.0F;
        break;
    case CodexActivityState::WaitingForUser:
        visual.intensity = entrance * 0.68F;
        visual.scale = 0.94F;
        break;
    case CodexActivityState::Completed: {
        const float fade = 1.0F - std::clamp(pulse.terminal_progress, 0.0F, 1.0F);
        visual.intensity = std::max(0.0F, fade * fade);
        visual.scale = 1.08F + (0.30F * (1.0F - fade));
        break;
    }
    case CodexActivityState::Error: {
        const float fade = 1.0F - std::clamp(pulse.terminal_progress, 0.0F, 1.0F);
        const float calm_variation = animation_enabled
            ? 0.92F + (0.08F * std::sin(phase * 3.2F))
            : 1.0F;
        visual.intensity = std::max(0.0F, fade * calm_variation);
        visual.scale = 1.06F;
        visual.alert = true;
        break;
    }
    case CodexActivityState::Aborted:
        visual.intensity = 0.38F
            * (1.0F - std::clamp(pulse.terminal_progress, 0.0F, 1.0F));
        visual.scale = 0.82F;
        break;
    case CodexActivityState::Unavailable:
    case CodexActivityState::Idle:
        visual.intensity = 0.0F;
        break;
    }
    return visual;
}

// ----------------------------------------------------------------------------
// Cree le degrade longitudinal du filament a partir des couleurs configurables.
//
// Parametres :
// - context : cible Direct2D et palette utilisateur.
// - bounds : zone qui fixe les extremites du degrade.
// - orientation : axe principal du filament.
//
// Retour :
// - brosse lineaire prete a dessiner, ou nullptr en cas d'echec Direct2D.
// ----------------------------------------------------------------------------
ComPtr<ID2D1LinearGradientBrush> CreateVeinGradientBrush(
    const WidgetRenderActivityVeinContext& context,
    const D2D1_RECT_F& bounds,
    WidgetActivityVeinOrientation orientation
) {
    const D2D1_GRADIENT_STOP stops[] = {
        D2D1::GradientStop(0.0F, context.palette.active_control),
        D2D1::GradientStop(0.52F, context.palette.remaining_accent),
        D2D1::GradientStop(1.0F, context.palette.history_curve),
    };
    ComPtr<ID2D1GradientStopCollection> stop_collection;
    if (FAILED(context.render_target->CreateGradientStopCollection(
        stops,
        ARRAYSIZE(stops),
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        stop_collection.GetAddressOf()
    ))) {
        return nullptr;
    }

    const D2D1_POINT_2F start = orientation == WidgetActivityVeinOrientation::Horizontal
        ? D2D1::Point2F(bounds.left, (bounds.top + bounds.bottom) * 0.5F)
        : D2D1::Point2F((bounds.left + bounds.right) * 0.5F, bounds.top);
    const D2D1_POINT_2F end = orientation == WidgetActivityVeinOrientation::Horizontal
        ? D2D1::Point2F(bounds.right, (bounds.top + bounds.bottom) * 0.5F)
        : D2D1::Point2F((bounds.left + bounds.right) * 0.5F, bounds.bottom);
    ComPtr<ID2D1LinearGradientBrush> brush;
    if (FAILED(context.render_target->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(start, end),
        stop_collection.Get(),
        brush.GetAddressOf()
    ))) {
        return nullptr;
    }
    return brush;
}

// ----------------------------------------------------------------------------
// Dessine le halo elliptique d'une impulsion sur la courbe.
//
// Parametres :
// - render_target : cible Direct2D courante.
// - center : centre de l'impulsion.
// - orientation : axe principal de l'impulsion.
// - color : couleur derivee de la palette.
// - intensity : intensite normalisee de la session.
// - scale : facteur de taille propre a l'etat.
// ----------------------------------------------------------------------------
void DrawVeinPulse(
    ID2D1RenderTarget* render_target,
    D2D1_POINT_2F center,
    WidgetActivityVeinOrientation orientation,
    const D2D1_COLOR_F& color,
    float intensity,
    float scale
) {
    if (render_target == nullptr || intensity <= 0.001F) {
        return;
    }
    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(render_target->CreateSolidColorBrush(color, brush.GetAddressOf()))) {
        return;
    }
    const float along_radius = 6.6F * scale;
    const float cross_radius = 2.1F * scale;
    const float radius_x = orientation == WidgetActivityVeinOrientation::Horizontal
        ? along_radius
        : cross_radius;
    const float radius_y = orientation == WidgetActivityVeinOrientation::Horizontal
        ? cross_radius
        : along_radius;
    for (int layer = 0; layer < 4; ++layer) {
        const float ratio = static_cast<float>(layer) / 3.0F;
        const float expansion = 2.6F - (ratio * 1.7F);
        brush->SetOpacity(intensity * (0.025F + (0.075F * ratio * ratio)));
        render_target->FillEllipse(
            D2D1::Ellipse(center, radius_x * expansion, radius_y * expansion),
            brush.Get()
        );
    }
    brush->SetOpacity(intensity * 0.72F);
    render_target->FillEllipse(
        D2D1::Ellipse(center, radius_x * 0.72F, radius_y * 0.52F),
        brush.Get()
    );
}

} // namespace

// ----------------------------------------------------------------------------
// Calcule le placement courant de la veine et de son hint.
// ----------------------------------------------------------------------------
WidgetActivityVeinLayout BuildWidgetActivityVeinLayout(
    D2D1_SIZE_F size,
    WidgetDisplayMode display_mode,
    float hint_text_width
) {
    WidgetActivityVeinLayout layout{};
    if (display_mode != WidgetDisplayMode::Compact
        && display_mode != WidgetDisplayMode::Complete) {
        return layout;
    }

    const float content_left = kPanelMargin + kPanelPadding;
    const float content_right = std::max(content_left, size.width - kPanelPadding);
    const float content_top = kPanelMargin + 12.0F;
    layout.vein_rect = D2D1::RectF(
        content_left,
        content_top + 29.0F,
        content_right,
        content_top + 39.0F
    );
    const float maximum_hint_width = std::max(
        0.0F,
        size.width - (kActivityHintEdgeMargin * 2.0F)
    );
    const float hint_width = std::min(
        maximum_hint_width,
        std::max(0.0F, hint_text_width + (kWidgetHintHorizontalPadding * 2.0F))
    );
    const float preferred_left = layout.vein_rect.left;
    const float maximum_left = std::max(
        kActivityHintEdgeMargin,
        size.width - kActivityHintEdgeMargin - hint_width
    );
    const float hint_left = std::clamp(
        preferred_left,
        kActivityHintEdgeMargin,
        maximum_left
    );
    layout.hint_rect = D2D1::RectF(
        hint_left,
        layout.vein_rect.bottom + kActivityHintGap,
        hint_left + hint_width,
        layout.vein_rect.bottom + kActivityHintGap + kActivityHintHeight
    );
    layout.available = true;
    return layout;
}

// ----------------------------------------------------------------------------
// Indique si un point appartient a la zone globale de la veine.
// ----------------------------------------------------------------------------
bool HitTestWidgetActivityVein(
    const WidgetActivityVeinLayout& layout,
    D2D1_POINT_2F point
) {
    return layout.available
        && point.x >= layout.vein_rect.left
        && point.x <= layout.vein_rect.right
        && point.y >= layout.vein_rect.top - kActivityVeinHitPadding
        && point.y <= layout.vein_rect.bottom + kActivityVeinHitPadding;
}

// ----------------------------------------------------------------------------
// Dessine le filament lumineux et les impulsions des sessions actives.
// ----------------------------------------------------------------------------
void DrawWidgetActivityVein(
    const WidgetRenderActivityVeinContext& context,
    const D2D1_RECT_F& bounds,
    const WidgetActivityFrame& frame,
    WidgetActivityVeinOrientation orientation,
    bool animation_enabled
) {
    if (context.d2d_factory == nullptr || context.render_target == nullptr) {
        return;
    }
    const float width = bounds.right - bounds.left;
    const float height = bounds.bottom - bounds.top;
    const float primary_extent = orientation == WidgetActivityVeinOrientation::Horizontal
        ? width
        : height;
    const float cross_extent = orientation == WidgetActivityVeinOrientation::Horizontal
        ? height
        : width;
    if (primary_extent < 16.0F || cross_extent < 3.0F) {
        return;
    }

    const float living_strength = animation_enabled && frame.animation_active ? 1.0F : 0.0F;
    const float living_phase = animation_enabled ? frame.animation_phase_seconds : 0.0F;
    const ComPtr<ID2D1PathGeometry> geometry = BuildVeinGeometry(
        context.d2d_factory,
        bounds,
        orientation,
        0.0F,
        living_phase,
        living_strength
    );
    const ComPtr<ID2D1PathGeometry> reflection_geometry = BuildVeinGeometry(
        context.d2d_factory,
        bounds,
        orientation,
        -kSecondaryReflectionOffset,
        living_phase,
        living_strength
    );
    if (geometry == nullptr || reflection_geometry == nullptr) {
        return;
    }

    const D2D1_COLOR_F vein_color = BlendVeinColor(
        context.palette.active_control,
        context.palette.remaining_accent,
        0.36F
    );
    ComPtr<ID2D1LinearGradientBrush> vein_brush = CreateVeinGradientBrush(
        context,
        bounds,
        orientation
    );
    if (vein_brush == nullptr) {
        return;
    }

    const std::size_t overflow_count = frame.pulses.size() > kDefaultMaximumDistinctPulseCount
        ? frame.pulses.size() - kDefaultMaximumDistinctPulseCount
        : 0U;
    const float density_boost = std::min(0.16F, static_cast<float>(overflow_count) * 0.035F);
    const float idle_opacity = frame.monitor_available ? 0.105F : 0.065F;
    const float activity_opacity = frame.pulses.empty() ? idle_opacity : 0.15F + density_boost;

    vein_brush->SetOpacity(activity_opacity * 0.20F);
    context.render_target->DrawGeometry(geometry.Get(), vein_brush.Get(), kOuterHaloWidth);
    vein_brush->SetOpacity(activity_opacity * 0.42F);
    context.render_target->DrawGeometry(geometry.Get(), vein_brush.Get(), kInnerHaloWidth);
    vein_brush->SetOpacity(activity_opacity + 0.06F);
    context.render_target->DrawGeometry(geometry.Get(), vein_brush.Get(), kCoreWidth);
    vein_brush->SetOpacity(activity_opacity * 0.32F);
    context.render_target->DrawGeometry(reflection_geometry.Get(), vein_brush.Get(), 0.65F);

    if (frame.animation_active) {
        const float flow_position = animation_enabled
            ? std::fmod(living_phase * kVeinFlowSpeed, 1.0F)
            : 0.5F;
        DrawVeinPulse(
            context.render_target,
            VeinPointAt(
                bounds,
                orientation,
                flow_position,
                0.0F,
                living_phase,
                living_strength
            ),
            orientation,
            vein_color,
            0.32F,
            1.38F
        );
    }

    const std::size_t visible_count = std::min(
        frame.pulses.size(),
        kDefaultMaximumDistinctPulseCount
    );
    for (std::size_t index = 0; index < visible_count; ++index) {
        const WidgetActivityPulseFrame& pulse = frame.pulses[index];
        const PulseVisual visual = CalculatePulseVisual(
            pulse,
            frame.animation_phase_seconds,
            animation_enabled
        );
        const D2D1_COLOR_F pulse_color = visual.alert
            ? BlendVeinColor(
                context.palette.progress_background,
                context.palette.active_control,
                0.16F
            )
            : vein_color;
        DrawVeinPulse(
            context.render_target,
            VeinPointAt(
                bounds,
                orientation,
                visual.position,
                0.0F,
                living_phase,
                living_strength
            ),
            orientation,
            pulse_color,
            visual.intensity,
            visual.scale
        );
    }
}

// ----------------------------------------------------------------------------
// Dessine le hint global de la veine lorsqu'elle est survolee.
// ----------------------------------------------------------------------------
void DrawWidgetActivityVeinHint(
    const WidgetRenderActivityVeinHintContext& context,
    const WidgetActivityVeinLayout& layout,
    const WidgetActivityVeinInteraction& interaction,
    const std::wstring& text
) {
    if (!layout.available
        || !interaction.hovered
        || text.empty()
        || context.render_target == nullptr
        || context.panel_background_brush == nullptr
        || context.border_brush == nullptr
        || context.muted_text_brush == nullptr
        || context.caption_text_format == nullptr) {
        return;
    }

    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(
        layout.hint_rect,
        kActivityHintCornerRadius,
        kActivityHintCornerRadius
    );
    context.render_target->FillRoundedRectangle(rounded, context.panel_background_brush);
    context.render_target->DrawRoundedRectangle(rounded, context.border_brush, 1.0F);

    const DWRITE_TEXT_ALIGNMENT previous_alignment =
        context.caption_text_format->GetTextAlignment();
    const DWRITE_PARAGRAPH_ALIGNMENT previous_paragraph =
        context.caption_text_format->GetParagraphAlignment();
    const DWRITE_WORD_WRAPPING previous_wrapping =
        context.caption_text_format->GetWordWrapping();
    context.caption_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    context.caption_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    context.caption_text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    context.render_target->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        context.caption_text_format,
        layout.hint_rect,
        context.muted_text_brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP
    );
    context.caption_text_format->SetTextAlignment(previous_alignment);
    context.caption_text_format->SetParagraphAlignment(previous_paragraph);
    context.caption_text_format->SetWordWrapping(previous_wrapping);
}

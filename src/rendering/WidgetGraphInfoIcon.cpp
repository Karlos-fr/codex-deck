// ============================================================================
// Codex Glass - Icone d'information des vues graphiques
// ----------------------------------------------------------------------------
// Ce fichier dessine le cercle et un i vectoriel dont les espacements ne
// dependent pas de la police employee ailleurs dans l'interface.
// ============================================================================

#include "WidgetGraphInfoIcon.h"

namespace {

// Epaisseur du contour circulaire en DIPs.
constexpr float kInfoCircleStrokeWidth = 1.0F;

// Rayon du point du i en DIPs.
constexpr float kInfoDotRadius = 0.65F;

// Decalage vertical du centre du point par rapport au centre du cercle.
constexpr float kInfoDotCenterOffsetY = -2.25F;

// Demi-largeur de la barre du i en DIPs.
constexpr float kInfoStemHalfWidth = 0.55F;

// Position du haut de la barre par rapport au centre du cercle.
constexpr float kInfoStemTopOffsetY = -0.45F;

// Position du bas de la barre par rapport au centre du cercle.
constexpr float kInfoStemBottomOffsetY = 2.55F;

} // namespace

// ----------------------------------------------------------------------------
// Dessine un i vectoriel centre dans sa bulle circulaire.
//
// Parametres :
// - render_target : cible Direct2D recevant le pictogramme.
// - brush : brosse appliquee au cercle, au point et a la barre.
// - bounds : zone partagee avec le hit-test du bouton d'information.
// ----------------------------------------------------------------------------
void DrawWidgetGraphInfoIcon(
    ID2D1RenderTarget* render_target,
    ID2D1Brush* brush,
    const D2D1_RECT_F& bounds
) {
    if (render_target == nullptr || brush == nullptr) {
        return;
    }

    const D2D1_POINT_2F center = D2D1::Point2F(
        (bounds.left + bounds.right) * 0.5F,
        (bounds.top + bounds.bottom) * 0.5F
    );
    render_target->DrawEllipse(
        D2D1::Ellipse(center, kWidgetGraphInfoRadius, kWidgetGraphInfoRadius),
        brush,
        kInfoCircleStrokeWidth
    );
    render_target->FillEllipse(
        D2D1::Ellipse(
            D2D1::Point2F(center.x, center.y + kInfoDotCenterOffsetY),
            kInfoDotRadius,
            kInfoDotRadius
        ),
        brush
    );
    render_target->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(
                center.x - kInfoStemHalfWidth,
                center.y + kInfoStemTopOffsetY,
                center.x + kInfoStemHalfWidth,
                center.y + kInfoStemBottomOffsetY
            ),
            kInfoStemHalfWidth,
            kInfoStemHalfWidth
        ),
        brush
    );
}

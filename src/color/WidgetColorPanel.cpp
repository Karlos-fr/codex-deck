// ============================================================================
// Codex Glass - Implementation du panneau integre de couleurs
// ----------------------------------------------------------------------------
// Ce fichier calcule la grille du panneau lateral et traduit les clics souris
// en actions independantes de l'application.
// ============================================================================

#include "WidgetColorPanel.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>

namespace {

// Marge interieure du panneau de couleurs.
constexpr float kColorPanelPadding = 14.0F;

// Espace entre le haut du panneau et la grille.
constexpr float kColorPanelTopSpacing = 38.0F;

// Largeur d'une colonne de personnalisation.
constexpr float kColorPanelColumnWidth = 122.0F;

// Hauteur d'une ligne de personnalisation.
constexpr float kColorPanelRowHeight = 34.0F;

// Taille d'un carre de couleur.
constexpr float kColorPanelSwatchSize = 18.0F;

// Marge cliquable autour d'un carre de couleur.
constexpr float kColorPanelSwatchHitPadding = 5.0F;

// Hauteur d'un bouton du panneau.
constexpr float kColorPanelButtonHeight = 22.0F;

// Largeur du bouton aleatoire.
constexpr float kColorPanelRandomButtonWidth = 72.0F;

// Taille du bouton de fermeture.
constexpr float kColorPanelCloseButtonSize = 18.0F;

// Espacement entre le carre couleur et son libelle.
constexpr float kColorPanelSwatchLabelGap = 7.0F;

// Nombre de colonnes de couleurs dans le panneau.
constexpr size_t kColorPanelColumnCount = 2;

// Nombre de champs couleur affiches.
constexpr size_t kColorPanelFieldCount = 8;

// ----------------------------------------------------------------------------
// Indique si un point client appartient a un rectangle Direct2D.
//
// Parametres :
// - rect : rectangle a tester.
// - point : point client Win32.
//
// Retour :
// - true si le point est dans le rectangle.
// ----------------------------------------------------------------------------
bool RectContainsPoint(const D2D1_RECT_F& rect, POINT point) {
    const float x = static_cast<float>(point.x);
    const float y = static_cast<float>(point.y);
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

}  // namespace

// ----------------------------------------------------------------------------
// Indique si deux resultats de hit-test representent le meme element.
// ----------------------------------------------------------------------------
bool IsSameWidgetColorPanelHit(const WidgetColorPanelHitTestResult& first, const WidgetColorPanelHitTestResult& second) {
    if (first.action != second.action) {
        return false;
    }

    if (first.action == WidgetColorPanelActionType::None) {
        return true;
    }

    if (first.action == WidgetColorPanelActionType::PickColor) {
        return first.field == second.field;
    }

    return true;
}

// ----------------------------------------------------------------------------
// Retourne le champ couleur associe a un index de panneau.
// ----------------------------------------------------------------------------
WidgetColorField WidgetColorFieldFromPanelIndex(size_t index) {
    switch (index) {
    case 0:
        return WidgetColorField::Background;
    case 1:
        return WidgetColorField::Border;
    case 2:
        return WidgetColorField::Text;
    case 3:
        return WidgetColorField::SecondaryText;
    case 4:
        return WidgetColorField::RemainingBar;
    case 5:
        return WidgetColorField::ConsumedBar;
    case 6:
        return WidgetColorField::HistoryCurve;
    case 7:
        return WidgetColorField::ActiveControl;
    default:
        return WidgetColorField::Background;
    }
}

// ----------------------------------------------------------------------------
// Retourne le libelle localise d'un champ couleur.
// ----------------------------------------------------------------------------
std::wstring WidgetColorFieldLabel(WidgetColorField field) {
    switch (field) {
    case WidgetColorField::Background:
        return T(IDS_COLOR_BACKGROUND);
    case WidgetColorField::Border:
        return T(IDS_COLOR_BORDER);
    case WidgetColorField::Text:
        return T(IDS_COLOR_TEXT);
    case WidgetColorField::SecondaryText:
        return T(IDS_COLOR_SECONDARY_TEXT);
    case WidgetColorField::RemainingBar:
        return T(IDS_COLOR_REMAINING_BAR);
    case WidgetColorField::ConsumedBar:
        return T(IDS_COLOR_CONSUMED_BAR);
    case WidgetColorField::HistoryCurve:
        return T(IDS_COLOR_HISTORY_CURVE);
    case WidgetColorField::ActiveControl:
        return T(IDS_COLOR_ACTIVE_CONTROL);
    default:
        return T(IDS_COLOR_BACKGROUND);
    }
}

// ----------------------------------------------------------------------------
// Calcule la geometrie du panneau depuis la taille client.
// ----------------------------------------------------------------------------
WidgetColorPanelLayout BuildWidgetColorPanelLayout(D2D1_SIZE_F client_size) {
    WidgetColorPanelLayout layout{};
    layout.panel_rect = D2D1::RectF(
        std::max(0.0F, client_size.width - static_cast<float>(kWidgetColorPanelWidth)),
        0.0F,
        client_size.width,
        client_size.height
    );

    const float grid_left = layout.panel_rect.left + kColorPanelPadding;
    const float grid_top = layout.panel_rect.top + kColorPanelTopSpacing;

    for (size_t index = 0; index < kColorPanelFieldCount; ++index) {
        const size_t column = index % kColorPanelColumnCount;
        const size_t row = index / kColorPanelColumnCount;
        const float item_left = grid_left + (static_cast<float>(column) * kColorPanelColumnWidth);
        const float item_top = grid_top + (static_cast<float>(row) * kColorPanelRowHeight);

        layout.color_swatch_rects[index] = D2D1::RectF(
            item_left,
            item_top,
            item_left + kColorPanelSwatchSize,
            item_top + kColorPanelSwatchSize
        );
        layout.color_label_rects[index] = D2D1::RectF(
            item_left + kColorPanelSwatchSize + kColorPanelSwatchLabelGap,
            item_top - 1.0F,
            item_left + kColorPanelColumnWidth - 8.0F,
            item_top + kColorPanelSwatchSize + 3.0F
        );
    }

    const float button_bottom = layout.panel_rect.bottom - kColorPanelPadding;
    layout.random_button_rect = D2D1::RectF(
        layout.panel_rect.right - kColorPanelPadding - kColorPanelRandomButtonWidth,
        button_bottom - kColorPanelButtonHeight,
        layout.panel_rect.right - kColorPanelPadding,
        button_bottom
    );
    layout.close_button_rect = D2D1::RectF(
        layout.panel_rect.right - kColorPanelPadding - kColorPanelCloseButtonSize,
        layout.panel_rect.top + 9.0F,
        layout.panel_rect.right - kColorPanelPadding,
        layout.panel_rect.top + 9.0F + kColorPanelCloseButtonSize
    );

    return layout;
}

// ----------------------------------------------------------------------------
// Teste un clic souris dans le panneau de couleurs.
// ----------------------------------------------------------------------------
WidgetColorPanelHitTestResult HitTestWidgetColorPanel(const WidgetColorPanelLayout& layout, POINT point) {
    if (!RectContainsPoint(layout.panel_rect, point)) {
        return WidgetColorPanelHitTestResult{};
    }

    if (RectContainsPoint(layout.close_button_rect, point)) {
        return WidgetColorPanelHitTestResult{WidgetColorPanelActionType::Close, WidgetColorField::Background};
    }

    if (RectContainsPoint(layout.random_button_rect, point)) {
        return WidgetColorPanelHitTestResult{WidgetColorPanelActionType::Randomize, WidgetColorField::Background};
    }

    for (size_t index = 0; index < layout.color_swatch_rects.size(); ++index) {
        const D2D1_RECT_F hit_rect = D2D1::RectF(
            layout.color_swatch_rects[index].left - kColorPanelSwatchHitPadding,
            layout.color_swatch_rects[index].top - kColorPanelSwatchHitPadding,
            layout.color_swatch_rects[index].right + kColorPanelSwatchHitPadding,
            layout.color_swatch_rects[index].bottom + kColorPanelSwatchHitPadding
        );
        if (RectContainsPoint(hit_rect, point)) {
            return WidgetColorPanelHitTestResult{
                WidgetColorPanelActionType::PickColor,
                WidgetColorFieldFromPanelIndex(index),
            };
        }
    }

    return WidgetColorPanelHitTestResult{};
}

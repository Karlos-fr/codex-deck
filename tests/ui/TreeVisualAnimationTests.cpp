// ============================================================================
// Codex Deck - Tests des animations visuelles Tree
// ----------------------------------------------------------------------------
// Ce fichier valide les calculs purs utilises par la TreeView pour garder les
// animations testables hors Direct2D.
// ============================================================================

#include "ui/TreeVisualAnimation.h"

#include <cmath>

namespace {

// ----------------------------------------------------------------------------
// Compare deux flottants avec tolerance.
//
// Parametres :
// - left : valeur gauche.
// - right : valeur droite.
//
// Retour :
// - true si les valeurs sont proches.
// ----------------------------------------------------------------------------
bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 0.01F;
}

}  // namespace

// ----------------------------------------------------------------------------
// Execute les validations d'animations visuelles.
//
// Retour :
// - zero si les interpolations et offsets attendus sont produits.
// ----------------------------------------------------------------------------
int main() {
    if (!NearlyEqual(ApproachVisualValue(4.0F, 8.0F, 1.5F), 5.5F)) {
        return 1;
    }
    if (!NearlyEqual(ApproachVisualValue(7.6F, 8.0F, 1.5F), 8.0F)) {
        return 2;
    }
    if (!NearlyEqual(ComputeMarqueeOffset(120.0F, 180.0F, 0.0F), 0.0F)) {
        return 3;
    }
    if (!NearlyEqual(ComputeMarqueeOffset(120.0F, 180.0F, 18.5F), 18.5F)) {
        return 4;
    }
    if (!NearlyEqual(ComputeMarqueeOffset(120.0F, 180.0F, 90.0F), 60.0F)) {
        return 5;
    }
    if (!NearlyEqual(ComputeMarqueeOffset(180.0F, 120.0F, 20.0F), 0.0F)) {
        return 6;
    }
    if (!NearlyEqual(ComputeMarqueeLayoutWidth(120.0F, 180.0F, true), 181.0F)) {
        return 7;
    }
    if (!NearlyEqual(ComputeMarqueeLayoutWidth(120.0F, 180.0F, false), 120.0F)) {
        return 8;
    }
    if (!NearlyEqual(ComputeMarqueeLayoutWidth(120.0F, 90.0F, true), 120.0F)) {
        return 9;
    }
    DampedVisualState opening{};
    opening = AdvanceDampedVisualState(opening, 1.0F, 1.0F / 60.0F, 0.55F, 44.0F);
    if (!(opening.value > 0.0F && opening.value < 0.02F && opening.velocity > 0.0F)) {
        return 10;
    }
    for (int frame = 0; frame < 300; ++frame) {
        opening = AdvanceDampedVisualState(opening, 1.0F, 1.0F / 60.0F, 0.55F, 44.0F);
    }
    if (!NearlyEqual(opening.value, 1.0F) || !NearlyEqual(opening.velocity, 0.0F)) {
        return 11;
    }
    DampedVisualState closing = AdvanceDampedVisualState(opening, 0.0F, 1.0F / 60.0F, 0.55F, 44.0F);
    if (!(closing.value < 1.0F && closing.velocity < 0.0F)) {
        return 12;
    }
    for (int frame = 0; frame < 300; ++frame) {
        closing = AdvanceDampedVisualState(closing, 0.0F, 1.0F / 60.0F, 0.55F, 44.0F);
    }
    if (!NearlyEqual(closing.value, 0.0F) || !NearlyEqual(closing.velocity, 0.0F)) {
        return 13;
    }
    MarqueeAnimationState marquee{};
    marquee.maximum_offset = 120.0F;
    for (int frame = 0; frame < 20; ++frame) {
        marquee = AdvanceMarqueeAnimation(marquee, true, 1.0F / 60.0F, 0.55F, 0.16F, 44.0F);
    }
    const float release_value = marquee.visual.value;
    marquee = AdvanceMarqueeAnimation(marquee, false, 1.0F / 60.0F, 0.55F, 0.16F, 44.0F);
    if (!marquee.coasting || marquee.visual.value < release_value || marquee.visual.velocity <= 0.0F) {
        return 14;
    }
    for (int frame = 0; frame < 180 && marquee.coasting; ++frame) {
        marquee = AdvanceMarqueeAnimation(marquee, false, 1.0F / 60.0F, 0.55F, 0.16F, 44.0F);
    }
    if (marquee.coasting || marquee.visual.value <= release_value || !NearlyEqual(marquee.visual.velocity, 0.0F)) {
        return 15;
    }
    marquee = AdvanceMarqueeAnimation(marquee, false, 1.0F / 60.0F, 0.55F, 0.16F, 44.0F);
    if (marquee.visual.velocity >= 0.0F) {
        return 16;
    }
    MarqueeAnimationState short_title{};
    short_title.maximum_offset = 100.0F;
    MarqueeAnimationState long_title{};
    long_title.maximum_offset = 300.0F;
    for (int frame = 0; frame < 90; ++frame) {
        short_title = AdvanceMarqueeAnimation(short_title, true, 1.0F / 60.0F, 0.55F, 0.16F, 44.0F);
        long_title = AdvanceMarqueeAnimation(long_title, true, 1.0F / 60.0F, 0.55F, 0.16F, 44.0F);
    }
    if (!NearlyEqual(short_title.visual.velocity, long_title.visual.velocity)
        || short_title.visual.velocity > 44.01F) {
        return 17;
    }
    return 0;
}

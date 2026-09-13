// ============================================================================
// Codex Deck - Animations visuelles Tree
// ----------------------------------------------------------------------------
// Ce module expose les petits calculs d'animation de la TreeView sans
// dependance au rendu Direct2D ni aux messages Win32.
// ============================================================================

#pragma once

// Etat position/vitesse d'une animation visuelle amortie.
struct DampedVisualState {
    // Position courante dans l'unite visuelle de l'appelant.
    float value = 0.0F;

    // Vitesse courante par seconde dans la meme unite.
    float velocity = 0.0F;
};

// Etat complet d'un marquee avec freinage avant le retour.
struct MarqueeAnimationState {
    // Mouvement normalise actuellement affiche.
    DampedVisualState visual;

    // Indique que le titre etait actif a la frame precedente.
    bool was_active = false;

    // Indique que le titre termine son freinage vers la gauche.
    bool coasting = false;

    // Position d'arret calculee au moment de la sortie du pointeur.
    float coast_target = 0.0F;

    // Distance maximale de texte actuellement masquee en DIPs.
    float maximum_offset = 0.0F;
};

// ----------------------------------------------------------------------------
// Rapproche une valeur visuelle de sa cible.
//
// Parametres :
// - current : valeur courante.
// - target : valeur cible.
// - step : pas maximal pour cette frame.
//
// Retour :
// - nouvelle valeur bornee entre la valeur courante et la cible.
// ----------------------------------------------------------------------------
float ApproachVisualValue(float current, float target, float step);

// ----------------------------------------------------------------------------
// Calcule l'offset horizontal d'un titre selectionne tronque.
//
// Parametres :
// - available_width : largeur visible du titre.
// - natural_width : largeur naturelle du titre.
// - phase : progression animee exprimee en DIPs.
//
// Retour :
// - offset positif, borne au dernier caractere lisible.
// ----------------------------------------------------------------------------
float ComputeMarqueeOffset(float available_width, float natural_width, float phase);

// ----------------------------------------------------------------------------
// Calcule la largeur de layout requise pour reveler un titre en marquee.
//
// Parametres :
// - available_width : largeur visible du titre.
// - natural_width : largeur naturelle du titre.
// - marquee_active : true lorsque le titre selectionne doit defiler.
//
// Retour :
// - largeur naturelle pendant le marquee, sinon largeur visible.
// ----------------------------------------------------------------------------
float ComputeMarqueeLayoutWidth(float available_width, float natural_width, bool marquee_active);

// ----------------------------------------------------------------------------
// Avance une progression avec acceleration et deceleration amorties.
//
// Parametres :
// - state : position et vitesse courantes.
// - target : position cible dans l'unite de l'appelant.
// - elapsed_seconds : temps ecoule depuis la frame precedente.
// - smooth_time_seconds : duree caracteristique de l'amortissement.
// - maximum_speed : vitesse absolue maximale par seconde.
//
// Retour :
// - nouvel etat borne, stabilise exactement sur la cible en fin de mouvement.
// ----------------------------------------------------------------------------
DampedVisualState AdvanceDampedVisualState(
    DampedVisualState state,
    float target,
    float elapsed_seconds,
    float smooth_time_seconds,
    float maximum_speed
);

// ----------------------------------------------------------------------------
// Avance un marquee et freine son mouvement avant le retour a droite.
//
// Parametres :
// - state : etat complet courant du marquee.
// - active : true pendant la selection ou le survol du titre.
// - elapsed_seconds : temps ecoule depuis la frame precedente.
// - smooth_time_seconds : duree caracteristique des mouvements principaux.
// - coast_projection_seconds : duree de projection utilisee pour le freinage.
// - maximum_speed : vitesse maximale commune a tous les titres en DIPs/s.
//
// Retour :
// - nouvel etat qui accelere, freine puis revient sans rupture de vitesse.
// ----------------------------------------------------------------------------
MarqueeAnimationState AdvanceMarqueeAnimation(
    MarqueeAnimationState state,
    bool active,
    float elapsed_seconds,
    float smooth_time_seconds,
    float coast_projection_seconds,
    float maximum_speed
);

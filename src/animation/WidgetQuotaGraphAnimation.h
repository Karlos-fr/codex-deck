// ============================================================================
// Codex Glass - Animation de mise a jour du graphique Quotas
// ----------------------------------------------------------------------------
// Ce module conserve la serie visible pendant un rafraichissement puis fait
// avancer sa fenetre temporelle vers le nouveau releve. Il ne dessine rien et
// ne depend ni de Win32 ni de SQLite.
// ============================================================================

#pragma once

#include <chrono>
#include <optional>

// ----------------------------------------------------------------------------
// Decrit la transition graphique a appliquer pendant une image de rendu.
// ----------------------------------------------------------------------------
struct WidgetQuotaGraphAnimationFrame {
    // Date maximale visible pendant que le worker construit le nouveau releve.
    std::optional<std::chrono::system_clock::time_point> maximum_sampled_at;

    // Fin de fenetre temporelle interpolee pendant le glissement de la courbe.
    std::optional<std::chrono::system_clock::time_point> range_end;

    // Indique que la translation d'entree doit encore etre animee.
    bool active = false;

};

// ----------------------------------------------------------------------------
// Gere le gel temporaire puis la transition de la courbe Quotas.
// ----------------------------------------------------------------------------
class WidgetQuotaGraphAnimation {
public:
    // Horloge monotone utilisee pour garantir une progression reguliere.
    using Clock = std::chrono::steady_clock;

    // Instant monotone accepte par les operations de timeline.
    using TimePoint = Clock::time_point;

    // ------------------------------------------------------------------------
    // Fige les samples et la fin de la fenetre temporelle courante.
    //
    // Parametres :
    // - maximum_sampled_at : dernier instant autorise dans la serie visible.
    // - range_end : fin de fenetre affichee avant le nouveau releve.
    // ------------------------------------------------------------------------
    void Hold(
        std::chrono::system_clock::time_point maximum_sampled_at,
        std::chrono::system_clock::time_point range_end
    );

    // ------------------------------------------------------------------------
    // Libere la serie et fait avancer la fenetre vers le nouveau releve.
    //
    // Parametres :
    // - range_end : nouvelle fin de fenetre correspondant au releve recu.
    // - now : instant monotone de debut de l'animation.
    // ------------------------------------------------------------------------
    void Start(
        std::chrono::system_clock::time_point range_end,
        TimePoint now
    );

    // ------------------------------------------------------------------------
    // Supprime tout gel ou animation encore actif.
    // ------------------------------------------------------------------------
    void Clear();

    // ------------------------------------------------------------------------
    // Calcule la frame de transition correspondant a un instant.
    //
    // Parametres :
    // - now : instant monotone courant.
    //
    // Retour :
    // - coupure et translation a transmettre au renderer.
    // ------------------------------------------------------------------------
    WidgetQuotaGraphAnimationFrame Frame(TimePoint now) const;

    // ------------------------------------------------------------------------
    // Indique si la translation necessite encore des images intermediaires.
    //
    // Parametres :
    // - now : instant monotone courant.
    //
    // Retour :
    // - true avant la fin de la transition ; false sinon.
    // ------------------------------------------------------------------------
    bool IsActive(TimePoint now) const;

private:
    // Date de coupure appliquee uniquement pendant le rafraichissement.
    std::optional<std::chrono::system_clock::time_point> maximum_sampled_at_;

    // Fin de fenetre conservee avant le rafraichissement.
    std::chrono::system_clock::time_point previous_range_end_{};

    // Fin de fenetre atteinte lorsque la nouvelle mesure est en place.
    std::chrono::system_clock::time_point target_range_end_{};

    // Instant de depart du glissement apres reception du releve.
    TimePoint started_at_{};

    // Indique qu'une translation a effectivement ete demarree.
    bool started_ = false;
};

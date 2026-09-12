// ============================================================================
// Codex Glass - Animation des chiffres de consommation
// ----------------------------------------------------------------------------
// Ce fichier declare l'etat d'une animation de remplacement vertical des
// chiffres quand un indicateur de consommation change.
// ============================================================================

#pragma once

#include <array>
#include <chrono>
#include <string>

// ----------------------------------------------------------------------------
// Etat rendu d'une animation de chiffres.
// ----------------------------------------------------------------------------
struct WidgetRollingNumberFrame {
    // Ancien texte affiche avant l'animation.
    std::wstring previous_text{};

    // Nouveau texte cible.
    std::wstring current_text{};

    // Progression bornee entre 0 et 1.
    double progress = 1.0;

    // Indique si l'animation doit etre dessinee.
    bool active = false;
};

// ----------------------------------------------------------------------------
// Suit une courte animation de remplacement vertical des chiffres.
// ----------------------------------------------------------------------------
class WidgetRollingNumberAnimation {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // ------------------------------------------------------------------------
    // Initialise la valeur sans declencher d'animation.
    //
    // Parametres :
    // - text : texte courant a memoriser.
    // ------------------------------------------------------------------------
    void Reset(const std::wstring& text);

    // ------------------------------------------------------------------------
    // Met a jour la valeur et demarre une animation si elle change.
    //
    // Parametres :
    // - text : nouveau texte affiche.
    // - now : instant de reference.
    // ------------------------------------------------------------------------
    void SetText(const std::wstring& text, TimePoint now);

    // ------------------------------------------------------------------------
    // Indique si une animation est encore active.
    //
    // Parametres :
    // - now : instant courant.
    //
    // Retour :
    // - true si l'animation doit continuer.
    // ------------------------------------------------------------------------
    bool IsActive(TimePoint now) const;

    // ------------------------------------------------------------------------
    // Retourne l'etat de rendu courant.
    //
    // Parametres :
    // - now : instant courant.
    //
    // Retour :
    // - frame a fournir au renderer.
    // ------------------------------------------------------------------------
    WidgetRollingNumberFrame Frame(TimePoint now) const;

private:
    std::array<wchar_t, 32> previous_text_{};
    std::array<wchar_t, 32> current_text_{};
    size_t previous_text_length_ = 0;
    size_t current_text_length_ = 0;
    TimePoint started_at_{};
    bool initialized_ = false;
    bool active_ = false;
};

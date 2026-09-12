// ============================================================================
// Codex Glass - Controleur visuel de l'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier declare la machine d'etats multisession qui stabilise les
// snapshots techniques avant leur consommation par le futur renderer.
// ============================================================================

#pragma once

#include "CodexActivityTypes.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

// ----------------------------------------------------------------------------
// Impulsion visuelle associee a une session sans exposer son identifiant.
// ----------------------------------------------------------------------------
struct WidgetActivityPulseFrame {
    // Etat visuel courant de l'impulsion.
    CodexActivityState state = CodexActivityState::Idle;

    // Decalage stable entre zero et un le long de la veine.
    float phase_offset = 0.0F;

    // Progression douce de l'entree dans l'etat courant.
    float transition_progress = 1.0F;

    // Temps ecoule depuis l'entree dans l'etat courant, en secondes.
    float state_elapsed_seconds = 0.0F;

    // Progression normalisee d'un etat terminal, nulle pour un etat actif.
    float terminal_progress = 0.0F;
};

// ----------------------------------------------------------------------------
// Resume visuel agrege de toutes les sessions suivies.
// ----------------------------------------------------------------------------
struct WidgetActivityFrame {
    // Etat prioritaire utilise par le libelle et le mouvement principal.
    CodexActivityState global_state = CodexActivityState::Idle;

    // Impulsions individuelles, ordonnees de facon stable.
    std::vector<WidgetActivityPulseFrame> pulses;

    // Nombre de sessions actuellement actives.
    std::size_t active_session_count = 0;

    // Nombre total d'outils ouverts, hors demandes utilisateur.
    std::size_t running_tool_count = 0;

    // Nombre de sessions qui attendent explicitement l'utilisateur.
    std::size_t waiting_session_count = 0;

    // Phase monotone commune qui ne saute pas entre deux etats.
    float animation_phase_seconds = 0.0F;

    // Indique si la source locale est accessible.
    bool monitor_available = false;

    // Indique qu'au moins une impulsion necessite encore un rendu anime.
    bool animation_active = false;
};

// ----------------------------------------------------------------------------
// Stabilise les etats techniques et construit les frames de la veine.
// ----------------------------------------------------------------------------
class WidgetActivityController {
public:
    // Horloge monotone utilisee pour les transitions.
    using Clock = std::chrono::steady_clock;

    // Instant monotone transmis aux operations du controleur.
    using TimePoint = Clock::time_point;

    // ------------------------------------------------------------------------
    // Cree une machine d'etats vide avec une phase commune initialisee.
    // ------------------------------------------------------------------------
    WidgetActivityController();

    // ------------------------------------------------------------------------
    // Libere automatiquement l'implementation privee.
    // ------------------------------------------------------------------------
    ~WidgetActivityController();

    WidgetActivityController(const WidgetActivityController&) = delete;
    WidgetActivityController& operator=(const WidgetActivityController&) = delete;

    // ------------------------------------------------------------------------
    // Integre un snapshot technique multisession.
    //
    // Parametres :
    // - snapshot : donnees publiees par CodexActivityMonitor.
    // - now : instant monotone de reception sur le thread UI.
    // ------------------------------------------------------------------------
    void Update(const CodexActivitySnapshot& snapshot, TimePoint now);

    // ------------------------------------------------------------------------
    // Avance les transitions et retourne la frame visuelle courante.
    //
    // Parametres :
    // - now : instant monotone du rendu.
    //
    // Retour :
    // - resume agrege sans identifiant de session.
    // ------------------------------------------------------------------------
    WidgetActivityFrame Frame(TimePoint now);

    // ------------------------------------------------------------------------
    // Efface tous les etats et redemarre la phase monotone.
    // ------------------------------------------------------------------------
    void Reset();

private:
    class Impl;

    // Implementation privee de la machine d'etats.
    std::unique_ptr<Impl> impl_;
};

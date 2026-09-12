// ============================================================================
// Codex Glass - Controleur visuel de l'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier agrege les sessions, applique l'hysteresis et fournit une phase
// continue. Il ne dessine rien et ne lit aucun fichier de session.
// ============================================================================

#include "WidgetActivityController.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

namespace {

// Validation necessaire avant d'afficher un etat d'attente.
constexpr std::chrono::milliseconds kWaitingValidationDelay{250};

// Duree minimale d'un etat actif avant une transition ordinaire.
constexpr std::chrono::milliseconds kMinimumAnimatedStateDuration{300};

// Duree d'affichage de l'eclair de fin.
constexpr std::chrono::milliseconds kCompletedHoldDuration{1800};

// Duree d'affichage de l'alerte d'erreur.
constexpr std::chrono::milliseconds kErrorHoldDuration{2500};

// Duree d'extinction apres une interruption ou une disparition.
constexpr std::chrono::milliseconds kAbortedFadeDuration{450};

// Duree utilisee pour lisser l'entree dans un nouvel etat.
constexpr std::chrono::milliseconds kStateTransitionDuration{300};

// Valeur initiale du hachage FNV-1a utilise pour les phases stables.
constexpr std::uint32_t kFnvOffsetBasis = 2166136261U;

// Facteur du hachage FNV-1a utilise pour les phases stables.
constexpr std::uint32_t kFnvPrime = 16777619U;

// ----------------------------------------------------------------------------
// Indique si un etat represente une session en travail.
// ------------------------------------------------------------------------
bool IsActiveState(CodexActivityState state) {
    return state == CodexActivityState::Thinking
        || state == CodexActivityState::ToolRunning
        || state == CodexActivityState::WaitingForUser;
}

// ----------------------------------------------------------------------------
// Indique si un etat possede une duree terminale propre.
// ------------------------------------------------------------------------
bool IsTerminalState(CodexActivityState state) {
    return state == CodexActivityState::Completed
        || state == CodexActivityState::Aborted
        || state == CodexActivityState::Error;
}

// ----------------------------------------------------------------------------
// Retourne la priorite d'un etat pour le resume global.
// ------------------------------------------------------------------------
int StatePriority(CodexActivityState state) {
    switch (state) {
    case CodexActivityState::Error: return 6;
    case CodexActivityState::ToolRunning: return 5;
    case CodexActivityState::Thinking: return 4;
    case CodexActivityState::WaitingForUser: return 3;
    case CodexActivityState::Completed: return 2;
    case CodexActivityState::Aborted: return 1;
    case CodexActivityState::Idle: return 0;
    case CodexActivityState::Unavailable: return -1;
    }
    return -1;
}

// ----------------------------------------------------------------------------
// Retourne la duree de maintien d'un etat terminal.
// ------------------------------------------------------------------------
std::chrono::milliseconds TerminalHoldDuration(CodexActivityState state) {
    switch (state) {
    case CodexActivityState::Completed: return kCompletedHoldDuration;
    case CodexActivityState::Error: return kErrorHoldDuration;
    case CodexActivityState::Aborted: return kAbortedFadeDuration;
    default: return std::chrono::milliseconds::zero();
    }
}

// ----------------------------------------------------------------------------
// Calcule un decalage stable a partir d'une cle de session non exposee.
// ------------------------------------------------------------------------
float StablePhaseOffset(const std::wstring& session_key) {
    std::uint32_t hash = kFnvOffsetBasis;
    for (const wchar_t character : session_key) {
        hash ^= static_cast<std::uint32_t>(character);
        hash *= kFnvPrime;
    }
    return static_cast<float>(hash % 10000U) / 10000.0F;
}

// ----------------------------------------------------------------------------
// Normalise une progression temporelle entre zero et un.
// ------------------------------------------------------------------------
float TransitionProgress(
    WidgetActivityController::TimePoint entered_at,
    WidgetActivityController::TimePoint now
) {
    if (entered_at.time_since_epoch().count() == 0 || now <= entered_at) {
        return 0.0F;
    }
    const float elapsed = std::chrono::duration<float>(now - entered_at).count();
    const float duration = std::chrono::duration<float>(kStateTransitionDuration).count();
    return std::clamp(elapsed / duration, 0.0F, 1.0F);
}

// ----------------------------------------------------------------------------
// Calcule la progression normalisee d'un etat terminal.
//
// Parametres :
// - state : etat dont la duree doit etre examinee.
// - entered_at : instant d'entree dans cet etat.
// - now : instant courant de la frame.
//
// Retour :
// - progression entre zero et un, ou zero pour un etat non terminal.
// ----------------------------------------------------------------------------
float TerminalProgress(
    CodexActivityState state,
    WidgetActivityController::TimePoint entered_at,
    WidgetActivityController::TimePoint now
) {
    const auto duration = TerminalHoldDuration(state);
    if (duration <= std::chrono::milliseconds::zero() || now <= entered_at) {
        return 0.0F;
    }
    const float elapsed_seconds = std::chrono::duration<float>(now - entered_at).count();
    const float duration_seconds = std::chrono::duration<float>(duration).count();
    return std::clamp(elapsed_seconds / duration_seconds, 0.0F, 1.0F);
}

} // namespace

// ----------------------------------------------------------------------------
// Implementation privee de la machine d'etats multisession.
// ----------------------------------------------------------------------------
class WidgetActivityController::Impl {
public:
    // ------------------------------------------------------------------------
    // Initialise la phase commune a l'instant de creation.
    // ------------------------------------------------------------------------
    Impl() : animation_epoch_(Clock::now()) {}

    // ------------------------------------------------------------------------
    // Integre un snapshot et programme les transitions necessaires.
    // ------------------------------------------------------------------------
    void Update(const CodexActivitySnapshot& snapshot, TimePoint now) {
        monitor_available_ = snapshot.monitor_available;
        ++snapshot_generation_;
        for (const CodexSessionActivitySnapshot& source : snapshot.sessions) {
            auto [iterator, inserted] = sessions_.try_emplace(source.session_key);
            SessionVisualState& target = iterator->second;
            if (inserted) {
                target.session_key = source.session_key;
                target.phase_offset = StablePhaseOffset(source.session_key);
                target.entered_at = now - kMinimumAnimatedStateDuration;
            }
            target.seen_generation = snapshot_generation_;
            target.present_in_source = true;
            target.open_tool_count = source.open_tool_count;
            target.waiting_for_user = source.waiting_for_user;
            if (inserted
                || target.source_state != source.state
                || target.source_event_at != source.last_event_at) {
                target.source_state = source.state;
                target.source_event_at = source.last_event_at;
                RequestState(target, source.state, now);
            }
        }

        for (auto iterator = sessions_.begin(); iterator != sessions_.end();) {
            SessionVisualState& session = iterator->second;
            if (session.seen_generation == snapshot_generation_) {
                ++iterator;
                continue;
            }
            session.present_in_source = false;
            session.open_tool_count = 0;
            session.waiting_for_user = false;
            if (session.current_state == CodexActivityState::Idle) {
                iterator = sessions_.erase(iterator);
                continue;
            }
            if (!IsTerminalState(session.current_state)) {
                ApplyState(session, CodexActivityState::Aborted, now);
            }
            ++iterator;
        }
        Advance(now);
    }

    // ------------------------------------------------------------------------
    // Avance la machine puis construit une frame sans identifiants.
    // ------------------------------------------------------------------------
    WidgetActivityFrame Frame(TimePoint now) {
        Advance(now);
        WidgetActivityFrame frame{};
        frame.monitor_available = monitor_available_;
        frame.animation_phase_seconds = std::chrono::duration<float>(now - animation_epoch_).count();
        frame.global_state = monitor_available_
            ? CodexActivityState::Idle
            : CodexActivityState::Unavailable;

        for (const auto& [key, session] : sessions_) {
            static_cast<void>(key);
            if (session.current_state == CodexActivityState::Idle
                || session.current_state == CodexActivityState::Unavailable) {
                continue;
            }
            frame.pulses.push_back(WidgetActivityPulseFrame{
                session.current_state,
                session.phase_offset,
                TransitionProgress(session.entered_at, now),
                std::max(0.0F, std::chrono::duration<float>(now - session.entered_at).count()),
                TerminalProgress(session.current_state, session.entered_at, now),
            });
            if (IsActiveState(session.current_state)) {
                ++frame.active_session_count;
            }
            if (session.current_state == CodexActivityState::ToolRunning) {
                frame.running_tool_count += std::max<std::size_t>(session.open_tool_count, 1U);
            }
            if (session.current_state == CodexActivityState::WaitingForUser
                || session.waiting_for_user) {
                ++frame.waiting_session_count;
            }
            if (StatePriority(session.current_state) > StatePriority(frame.global_state)) {
                frame.global_state = session.current_state;
            }
        }
        std::sort(frame.pulses.begin(), frame.pulses.end(), [](const auto& left, const auto& right) {
            return left.phase_offset < right.phase_offset;
        });
        frame.animation_active = !frame.pulses.empty();
        return frame;
    }

    // ------------------------------------------------------------------------
    // Efface les sessions et redemarre la phase commune.
    // ------------------------------------------------------------------------
    void Reset() {
        sessions_.clear();
        monitor_available_ = false;
        snapshot_generation_ = 0;
        animation_epoch_ = Clock::now();
    }

private:
    // Etat visuel prive associe a une session technique.
    struct SessionVisualState {
        std::wstring session_key;
        CodexActivityState current_state = CodexActivityState::Idle;
        std::optional<CodexActivityState> pending_state;
        CodexActivityState source_state = CodexActivityState::Unavailable;
        TimePoint entered_at{};
        TimePoint pending_since{};
        TimePoint terminal_until{};
        std::chrono::system_clock::time_point source_event_at{};
        float phase_offset = 0.0F;
        std::size_t open_tool_count = 0;
        std::size_t seen_generation = 0;
        bool waiting_for_user = false;
        bool present_in_source = false;
    };

    // ------------------------------------------------------------------------
    // Applique ou differe une transition selon les regles d'hysteresis.
    // ------------------------------------------------------------------------
    void RequestState(
        SessionVisualState& session,
        CodexActivityState requested,
        TimePoint now
    ) {
        if (requested == session.current_state) {
            session.pending_state.reset();
            return;
        }
        if (requested == CodexActivityState::WaitingForUser) {
            if (session.pending_state != requested) {
                session.pending_state = requested;
                session.pending_since = now;
            }
            return;
        }

        if (IsTerminalState(requested)) {
            ApplyState(session, requested, now);
            return;
        }
        if (requested == CodexActivityState::ToolRunning) {
            ApplyState(session, requested, now);
            return;
        }
        if (session.current_state == CodexActivityState::ToolRunning
            && requested == CodexActivityState::Thinking) {
            if (session.pending_state != requested) {
                session.pending_state = requested;
                session.pending_since = now;
            }
            return;
        }
        if (now - session.entered_at < kMinimumAnimatedStateDuration) {
            if (session.pending_state != requested) {
                session.pending_state = requested;
                session.pending_since = now;
            }
            return;
        }
        ApplyState(session, requested, now);
    }

    // ------------------------------------------------------------------------
    // Remplace immediatement l'etat courant et initialise ses delais.
    // ------------------------------------------------------------------------
    void ApplyState(
        SessionVisualState& session,
        CodexActivityState state,
        TimePoint now
    ) {
        session.current_state = state;
        session.entered_at = now;
        session.pending_state.reset();
        const auto terminal_duration = TerminalHoldDuration(state);
        session.terminal_until = terminal_duration > std::chrono::milliseconds::zero()
            ? now + terminal_duration
            : TimePoint{};
    }

    // ------------------------------------------------------------------------
    // Applique les transitions arrivees a echeance et retire les fins expirees.
    // ------------------------------------------------------------------------
    void Advance(TimePoint now) {
        for (auto iterator = sessions_.begin(); iterator != sessions_.end();) {
            SessionVisualState& session = iterator->second;
            if (IsTerminalState(session.current_state)
                && session.terminal_until.time_since_epoch().count() != 0
                && now >= session.terminal_until) {
                ApplyState(session, CodexActivityState::Idle, now);
                if (!session.present_in_source) {
                    iterator = sessions_.erase(iterator);
                    continue;
                }
            }
            if (session.pending_state.has_value()) {
                const auto requested = *session.pending_state;
                const bool waiting_ready = requested != CodexActivityState::WaitingForUser
                    || now - session.pending_since >= kWaitingValidationDelay;
                const bool tool_exit_ready = session.current_state != CodexActivityState::ToolRunning
                    || requested != CodexActivityState::Thinking
                    || now - session.pending_since >= kMinimumAnimatedStateDuration;
                const bool current_ready = now - session.entered_at >= kMinimumAnimatedStateDuration;
                if (waiting_ready && tool_exit_ready && current_ready) {
                    ApplyState(session, requested, now);
                }
            }
            ++iterator;
        }
    }

    // Etats visuels indexes par cle technique uniquement dans le controleur.
    std::map<std::wstring, SessionVisualState> sessions_;

    // Origine monotone commune a toutes les impulsions.
    TimePoint animation_epoch_{};

    // Generation du dernier snapshot utilisee pour reperer les disparitions.
    std::size_t snapshot_generation_ = 0;

    // Disponibilite courante de la source locale.
    bool monitor_available_ = false;
};

// ----------------------------------------------------------------------------
// Cree une machine d'etats vide.
// ----------------------------------------------------------------------------
WidgetActivityController::WidgetActivityController()
    : impl_(std::make_unique<Impl>()) {}

// ----------------------------------------------------------------------------
// Libere automatiquement l'implementation privee.
// ----------------------------------------------------------------------------
WidgetActivityController::~WidgetActivityController() = default;

// ----------------------------------------------------------------------------
// Integre un snapshot technique multisession.
// ----------------------------------------------------------------------------
void WidgetActivityController::Update(const CodexActivitySnapshot& snapshot, TimePoint now) {
    impl_->Update(snapshot, now);
}

// ----------------------------------------------------------------------------
// Avance les transitions et retourne la frame courante.
// ----------------------------------------------------------------------------
WidgetActivityFrame WidgetActivityController::Frame(TimePoint now) {
    return impl_->Frame(now);
}

// ----------------------------------------------------------------------------
// Efface les etats et redemarre la phase commune.
// ----------------------------------------------------------------------------
void WidgetActivityController::Reset() {
    impl_->Reset();
}

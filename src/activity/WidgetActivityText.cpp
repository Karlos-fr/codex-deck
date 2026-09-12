// ============================================================================
// Codex Glass - Implementation des textes de l'activite Codex
// ----------------------------------------------------------------------------
// Ce fichier localise les etats et compteurs sans acceder aux identifiants,
// chemins, commandes ou contenus conserves par le moniteur technique.
// ============================================================================

#include "WidgetActivityText.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <cwchar>
#include <vector>

namespace {

// Separateur visuel compact utilise entre les fragments du hint.
constexpr wchar_t kActivityHintSeparator[] = L" \u00B7 ";

// ----------------------------------------------------------------------------
// Retourne le texte localise de l'etat global.
//
// Parametres :
// - state : etat prioritaire calcule par le controleur.
//
// Retour :
// - libelle sans compteur ni information de session.
// ----------------------------------------------------------------------------
std::wstring ActivityStateText(CodexActivityState state) {
    switch (state) {
    case CodexActivityState::Thinking: return T(IDS_ACTIVITY_STATE_THINKING);
    case CodexActivityState::ToolRunning: return T(IDS_ACTIVITY_STATE_TOOL);
    case CodexActivityState::WaitingForUser: return T(IDS_ACTIVITY_STATE_WAITING);
    case CodexActivityState::Completed: return T(IDS_ACTIVITY_STATE_COMPLETED);
    case CodexActivityState::Error: return T(IDS_ACTIVITY_STATE_ERROR);
    case CodexActivityState::Aborted:
    case CodexActivityState::Unavailable:
    case CodexActivityState::Idle:
        return T(IDS_ACTIVITY_STATE_IDLE);
    }
    return T(IDS_ACTIVITY_STATE_IDLE);
}

// ----------------------------------------------------------------------------
// Formate un compteur avec une ressource singuliere ou plurielle.
//
// Parametres :
// - count : valeur a inserer.
// - singular_id : ressource utilisee pour la valeur un.
// - plural_id : ressource utilisee pour les autres valeurs.
//
// Retour :
// - texte localise avec compteur decimal.
// ----------------------------------------------------------------------------
std::wstring FormatActivityCount(
    std::size_t count,
    unsigned int singular_id,
    unsigned int plural_id
) {
    const std::wstring format = T(count == 1U ? singular_id : plural_id);
    wchar_t buffer[96]{};
    _snwprintf_s(buffer, _TRUNCATE, format.c_str(), static_cast<unsigned int>(count));
    return buffer;
}

// ----------------------------------------------------------------------------
// Assemble des fragments avec le separateur compact commun.
//
// Parametres :
// - fragments : textes non vides a reunir dans leur ordre d'affichage.
//
// Retour :
// - chaine unique adaptee au hint global.
// ----------------------------------------------------------------------------
std::wstring JoinActivityFragments(const std::vector<std::wstring>& fragments) {
    std::wstring result;
    for (const std::wstring& fragment : fragments) {
        if (fragment.empty()) {
            continue;
        }
        if (!result.empty()) {
            result += kActivityHintSeparator;
        }
        result += fragment;
    }
    return result;
}

} // namespace

// ----------------------------------------------------------------------------
// Construit le libelle permanent compact de l'activite.
// ----------------------------------------------------------------------------
std::wstring BuildWidgetActivityLabel(const WidgetActivityFrame& frame) {
    if (frame.global_state == CodexActivityState::Error
        || frame.global_state == CodexActivityState::Completed
        || frame.global_state == CodexActivityState::WaitingForUser) {
        return ActivityStateText(frame.global_state);
    }
    if (frame.active_session_count > 1U) {
        return FormatActivityCount(
            frame.active_session_count,
            IDS_ACTIVITY_SESSION_ACTIVE,
            IDS_ACTIVITY_SESSIONS_ACTIVE
        );
    }
    if (frame.active_session_count == 1U) {
        return T(IDS_ACTIVITY_WORKING);
    }
    return ActivityStateText(frame.global_state);
}

// ----------------------------------------------------------------------------
// Construit le hint detaille de la veine.
// ----------------------------------------------------------------------------
std::wstring BuildWidgetActivityHint(const WidgetActivityFrame& frame) {
    std::vector<std::wstring> fragments;
    fragments.push_back(ActivityStateText(frame.global_state));
    if (frame.active_session_count > 0U) {
        fragments.push_back(FormatActivityCount(
            frame.active_session_count,
            IDS_ACTIVITY_SESSION_ACTIVE,
            IDS_ACTIVITY_SESSIONS_ACTIVE
        ));
    }
    if (frame.running_tool_count > 0U) {
        fragments.push_back(FormatActivityCount(
            frame.running_tool_count,
            IDS_ACTIVITY_TOOL_RUNNING,
            IDS_ACTIVITY_TOOLS_RUNNING
        ));
    }
    if (frame.waiting_session_count > 0U) {
        fragments.push_back(FormatActivityCount(
            frame.waiting_session_count,
            IDS_ACTIVITY_WAITING_COUNT,
            IDS_ACTIVITY_WAITING_COUNT
        ));
    }
    return JoinActivityFragments(fragments);
}

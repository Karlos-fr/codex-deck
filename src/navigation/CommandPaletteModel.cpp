// ============================================================================
// Codex Deck - Implementation du modele de Command Palette
// ----------------------------------------------------------------------------
// Ce fichier score sessions, projets et actions sur tout le snapshot puis borne
// seulement la liste retournee pour l'affichage.
// ============================================================================

#include "CommandPaletteModel.h"

#include "../search/FuzzyMatcher.h"

#include <algorithm>
#include <array>
#include <unordered_map>

namespace {

// ----------------------------------------------------------------------------
// Convertit une chaine ASCII/UTF-8 simple en wide.
//
// Parametres :
// - text : texte source.
//
// Retour :
// - texte wide.
// ----------------------------------------------------------------------------
std::wstring WidenAscii(std::string_view text) {
    std::wstring wide;
    wide.reserve(text.size());
    for (const char character : text) {
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    }
    return wide;
}

// ----------------------------------------------------------------------------
// Convertit une chaine wide ASCII en minuscules.
//
// Parametres :
// - text : texte source.
//
// Retour :
// - texte normalise pour matching.
// ----------------------------------------------------------------------------
std::wstring LowerAscii(std::wstring text) {
    for (wchar_t& character : text) {
        if (character >= L'A' && character <= L'Z') {
            character = static_cast<wchar_t>(character - L'A' + L'a');
        }
    }
    return text;
}

// ----------------------------------------------------------------------------
// Decrit une action statique de palette.
// ----------------------------------------------------------------------------
struct StaticAction {
    // Identifiant stable.
    const char* id;

    // Titre recherche.
    const wchar_t* title;

    // Commande associee.
    DeckCommandKind command;
};

// Actions globales exposees dans la palette.
constexpr std::array<StaticAction, 6> kActions{{
    {"action:new-session", L"new session", DeckCommandKind::NewSession},
    {"action:new-session-current-project", L"new session current project", DeckCommandKind::NewSessionInCurrentProject},
    {"action:rename-thread", L"rename thread", DeckCommandKind::RenameThread},
    {"action:archive-thread", L"archive thread", DeckCommandKind::ArchiveThread},
    {"action:toggle-favorite", L"toggle favorite", DeckCommandKind::ToggleFavorite},
    {"action:detach-workbench", L"detach workbench", DeckCommandKind::DetachWorkbench},
}};

// ----------------------------------------------------------------------------
// Compare deux entrees scorees.
//
// Parametres :
// - left : entree gauche.
// - right : entree droite.
//
// Retour :
// - true si left doit preceder right.
// ----------------------------------------------------------------------------
bool BetterEntry(const PaletteEntry& left, const PaletteEntry& right) {
    if (left.score != right.score) {
        return left.score > right.score;
    }
    if (left.kind != right.kind) {
        return static_cast<int>(left.kind) < static_cast<int>(right.kind);
    }
    return left.id < right.id;
}

// ----------------------------------------------------------------------------
// Ajoute une entree si elle correspond a la requete.
//
// Parametres :
// - entries : liste de sortie.
// - query : requete fuzzy.
// - entry : entree candidate.
// - haystack : texte score.
// ----------------------------------------------------------------------------
void AddIfMatched(
    std::vector<PaletteEntry>& entries,
    std::wstring_view query,
    PaletteEntry entry,
    std::wstring_view haystack
) {
    if (const auto match = FuzzyMatch(LowerAscii(std::wstring(query)), LowerAscii(std::wstring(haystack)))) {
        entry.score = match->score;
        entries.push_back(std::move(entry));
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// Construit les entrees de palette correspondant a une requete.
// ----------------------------------------------------------------------------
std::vector<PaletteEntry> BuildCommandPaletteEntries(
    const SessionCatalogSnapshot& snapshot,
    std::wstring_view query,
    std::size_t max_results
) {
    std::vector<PaletteEntry> entries;
    entries.reserve(snapshot.projects.size() + snapshot.sessions.size() + kActions.size());
    std::unordered_map<ProjectId, std::wstring> project_names;
    for (const Project& project : snapshot.projects) {
        project_names.emplace(project.id, WidenAscii(project.name));
    }

    for (const SessionRecord& session : snapshot.sessions) {
        PaletteEntry entry{};
        entry.kind = PaletteEntryKind::Session;
        entry.id = "session:" + session.codex.id;
        entry.title = WidenAscii(session.codex.name.empty() ? session.codex.id : session.codex.name);
        entry.subtitle = L"SESSIONS";
        entry.command = DeckCommand{DeckCommandKind::OpenThread, session.project_id, session.codex.id};
        std::wstring haystack = entry.title + L" " + WidenAscii(session.codex.id);
        if (session.project_id) {
            if (const auto project = project_names.find(*session.project_id); project != project_names.end()) {
                haystack += L" " + project->second;
            }
        }
        AddIfMatched(entries, query, std::move(entry), haystack);
    }

    for (const Project& project : snapshot.projects) {
        PaletteEntry entry{};
        entry.kind = PaletteEntryKind::Project;
        entry.id = "project:" + std::to_string(project.id);
        entry.title = WidenAscii(project.name);
        entry.subtitle = L"PROJECTS";
        entry.command = DeckCommand{DeckCommandKind::OpenWorkspace, project.id, std::nullopt};
        const std::wstring haystack = entry.title;
        AddIfMatched(entries, query, std::move(entry), haystack);
    }

    for (const StaticAction& action : kActions) {
        PaletteEntry entry{};
        entry.kind = PaletteEntryKind::Action;
        entry.id = action.id;
        entry.title = action.title;
        entry.subtitle = L"ACTIONS";
        entry.command = DeckCommand{action.command, std::nullopt, std::nullopt};
        AddIfMatched(entries, query, std::move(entry), entry.title);
    }

    std::ranges::sort(entries, BetterEntry);
    if (entries.size() > max_results) {
        entries.resize(max_results);
    }
    return entries;
}

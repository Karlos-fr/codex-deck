// ============================================================================
// Codex Glass - Implementation du chargement des textes localises
// ----------------------------------------------------------------------------
// Ce fichier lit les STRINGTABLE Windows embarquees dans l'executable et choisit
// la langue active sans dependre de fichiers externes.
// ============================================================================

#include "Localization.h"

#include "../resources/ResourceIds.h"

#include <windows.h>

#include <cwchar>
#include <optional>

namespace {

// Langue francaise utilisee comme secours.
constexpr LANGID kFrenchLanguageId = MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH);

// Langue anglaise americaine embarquee dans les ressources.
constexpr LANGID kEnglishLanguageId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

// Premier identifiant de chaine localisee embarque dans les ressources.
constexpr unsigned int kFirstLocalizedResourceId = IDS_APP_TITLE;

// Dernier identifiant de chaine localisee embarque dans les ressources.
constexpr unsigned int kLastLocalizedResourceId = IDS_ACTIVITY_WAITING_COUNT;

// Langue demandee par l'utilisateur dans les reglages.
UiLanguage g_requested_language = UiLanguage::Auto;

// Langue effectivement utilisee pour les ressources.
LANGID g_active_language_id = kFrenchLanguageId;

// ----------------------------------------------------------------------------
// Convertit le reglage de langue en LANGID de ressources.
//
// Parametres :
// - language : langue demandee par les reglages.
//
// Retour :
// - LANGID Windows a utiliser pour chercher les chaines.
// ----------------------------------------------------------------------------
LANGID ResolveLanguageId(UiLanguage language) {
    if (language == UiLanguage::French) {
        return kFrenchLanguageId;
    }

    if (language == UiLanguage::English) {
        return kEnglishLanguageId;
    }

    const LANGID user_language = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(user_language) == LANG_ENGLISH) {
        return kEnglishLanguageId;
    }

    return kFrenchLanguageId;
}

// ----------------------------------------------------------------------------
// Charge une chaine depuis un bloc STRINGTABLE precis.
//
// Parametres :
// - resource_id : identifiant de chaine defini dans ResourceIds.h.
// - language_id : LANGID de la ressource recherchee.
//
// Retour :
// - texte trouve, ou chaine vide si la ressource est absente.
// ----------------------------------------------------------------------------
std::wstring LoadStringResource(unsigned int resource_id, LANGID language_id) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const unsigned int block_id = (resource_id / 16U) + 1U;
    const unsigned int block_index = resource_id % 16U;

    HRSRC resource = FindResourceExW(
        instance,
        RT_STRING,
        MAKEINTRESOURCEW(block_id),
        language_id
    );
    if (resource == nullptr) {
        return {};
    }

    HGLOBAL loaded_resource = LoadResource(instance, resource);
    if (loaded_resource == nullptr) {
        return {};
    }

    const wchar_t* cursor = static_cast<const wchar_t*>(LockResource(loaded_resource));
    if (cursor == nullptr) {
        return {};
    }

    for (unsigned int index = 0; index < block_index; ++index) {
        cursor += 1U + static_cast<unsigned int>(*cursor);
    }

    const unsigned int length = static_cast<unsigned int>(*cursor);
    return std::wstring(cursor + 1U, cursor + 1U + length);
}

}  // namespace

// ----------------------------------------------------------------------------
// Applique la langue d'interface demandee.
//
// Parametres :
// - language : langue configuree par l'utilisateur ou mode automatique.
// ----------------------------------------------------------------------------
void SetUiLanguage(UiLanguage language) {
    g_requested_language = language;
    g_active_language_id = ResolveLanguageId(language);
}

// ----------------------------------------------------------------------------
// Retourne la langue d'interface demandee.
//
// Retour :
// - langue configuree par l'utilisateur ou mode automatique.
// ----------------------------------------------------------------------------
UiLanguage CurrentUiLanguage() {
    return g_requested_language;
}

// ----------------------------------------------------------------------------
// Retourne la langue effectivement utilisee pour les ressources.
//
// Retour :
// - langue francaise ou anglaise apres resolution du mode automatique.
// ----------------------------------------------------------------------------
UiLanguage ActiveUiLanguage() {
    return PRIMARYLANGID(g_active_language_id) == LANG_ENGLISH
        ? UiLanguage::English
        : UiLanguage::French;
}

// ----------------------------------------------------------------------------
// Charge un texte localise depuis les ressources embarquees.
//
// Parametres :
// - resource_id : identifiant de chaine defini dans ResourceIds.h.
//
// Retour :
// - texte localise ou libelle technique de secours.
// ----------------------------------------------------------------------------
std::wstring T(unsigned int resource_id) {
    std::wstring value = LoadStringResource(resource_id, g_active_language_id);
    if (!value.empty()) {
        return value;
    }

    value = LoadStringResource(resource_id, kFrenchLanguageId);
    if (!value.empty()) {
        return value;
    }

    return L"#" + std::to_wstring(resource_id);
}

// ----------------------------------------------------------------------------
// Retraduit un texte provenant de la langue precedente vers la langue active.
//
// Parametres :
// - text : texte potentiellement charge depuis une ressource localisee.
// - previous_language : langue effective utilisee avant le changement.
//
// Retour :
// - traduction courante lorsque la ressource est identifiable sans ambiguite,
//   sinon texte original.
// ----------------------------------------------------------------------------
std::wstring RelocalizeText(const std::wstring& text, UiLanguage previous_language) {
    if (text.empty()) {
        return text;
    }

    const LANGID previous_language_id = ResolveLanguageId(previous_language);
    std::optional<std::wstring> translated_text;
    for (unsigned int resource_id = kFirstLocalizedResourceId;
         resource_id <= kLastLocalizedResourceId;
         ++resource_id) {
        std::wstring previous_text = LoadStringResource(resource_id, previous_language_id);
        if (previous_text.empty()) {
            previous_text = LoadStringResource(resource_id, kFrenchLanguageId);
        }
        if (previous_text != text) {
            continue;
        }

        const std::wstring candidate = T(resource_id);
        if (translated_text.has_value() && *translated_text != candidate) {
            return text;
        }
        translated_text = candidate;
    }
    return translated_text.value_or(text);
}

// ----------------------------------------------------------------------------
// Charge un texte localise puis remplace un parametre texte.
//
// Parametres :
// - resource_id : identifiant de chaine defini dans ResourceIds.h.
// - value : valeur injectee dans le premier parametre %s.
//
// Retour :
// - texte localise formate.
// ----------------------------------------------------------------------------
std::wstring FormatT(unsigned int resource_id, const std::wstring& value) {
    wchar_t buffer[512]{};
    swprintf_s(buffer, T(resource_id).c_str(), value.c_str());
    return buffer;
}

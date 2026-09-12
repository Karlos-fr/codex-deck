// ============================================================================
// Codex Deck - Implementation de la normalisation de chemins
// ----------------------------------------------------------------------------
// Ce fichier applique une normalisation Windows stable pour comparer les roots
// de projet et les cwd Codex.
// ============================================================================

#include "PathNormalization.h"

#include <windows.h>

#include <algorithm>

namespace {

// ----------------------------------------------------------------------------
// Convertit une chaine en minuscules invariant Windows.
//
// Parametres :
// - value : chaine source.
//
// Retour :
// - chaine minuscule.
// ----------------------------------------------------------------------------
std::wstring LowerInvariant(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    std::wstring lowered(value.size(), L'\0');
    const int written = LCMapStringEx(
        LOCALE_NAME_INVARIANT,
        LCMAP_LOWERCASE,
        value.c_str(),
        static_cast<int>(value.size()),
        lowered.data(),
        static_cast<int>(lowered.size()),
        nullptr,
        nullptr,
        0
    );
    if (written <= 0) {
        std::wstring fallback = value;
        std::transform(fallback.begin(), fallback.end(), fallback.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        return fallback;
    }
    lowered.resize(static_cast<std::size_t>(written));
    return lowered;
}

// ----------------------------------------------------------------------------
// Indique si un chemin normalise se termine par une racine Windows.
//
// Parametres :
// - value : chemin texte.
//
// Retour :
// - true pour C:\ ou \\serveur\partage\.
// ----------------------------------------------------------------------------
bool IsRootPath(const std::wstring& value) {
    if (value.size() == 3 && value[1] == L':' && value[2] == L'\\') {
        return true;
    }
    if (value.rfind(L"\\\\", 0) == 0) {
        std::size_t parts = 0;
        for (wchar_t ch : value) {
            if (ch == L'\\') {
                ++parts;
            }
        }
        return parts <= 4;
    }
    return false;
}

}  // namespace

// ----------------------------------------------------------------------------
// Normalise un chemin Windows pour comparaison.
// ----------------------------------------------------------------------------
std::wstring NormalizeWindowsPath(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        normalized = path.lexically_normal();
    }
    std::wstring value = normalized.wstring();
    std::replace(value.begin(), value.end(), L'/', L'\\');
    while (value.size() > 1 && value.back() == L'\\' && !IsRootPath(value)) {
        value.pop_back();
    }
    return LowerInvariant(value);
}

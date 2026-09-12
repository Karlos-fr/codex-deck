// ============================================================================
// Codex Glass - Implementation des reglages locaux
// ----------------------------------------------------------------------------
// Ce fichier charge et sauvegarde un fichier INI simple a cote de l'executable
// afin de conserver la position, la taille et les preferences du widget.
// ============================================================================

#include "AppSettings.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <algorithm>
#include <cwchar>
#include <iterator>
#include <optional>
#include <vector>

namespace {

// Nom du fichier INI de configuration.
constexpr wchar_t kSettingsFileName[] = L"settings.ini";

// Nom de la section INI contenant les reglages.
constexpr wchar_t kSettingsSection[] = L"Settings";

// Prefixe des sections INI contenant les presets de couleurs.
constexpr wchar_t kColorPresetSectionPrefix[] = L"ColorPreset.";

// Cle INI contenant le nom affiche d'un preset.
constexpr wchar_t kColorPresetNameKey[] = L"name";

// Identifiant du preset Codex Glass.
constexpr wchar_t kCodexGlassPresetId[] = L"codex_glass";

// Identifiant du preset sombre Windows.
constexpr wchar_t kWindowsDarkPresetId[] = L"windows_dark";

// Identifiant du preset clair Windows.
constexpr wchar_t kWindowsLightPresetId[] = L"windows_light";

// Identifiant du preset vert terminal.
constexpr wchar_t kTerminalGreenPresetId[] = L"terminal_green";

// Identifiant du preset haute lisibilite.
constexpr wchar_t kHighContrastPresetId[] = L"high_readability";

// Identifiant du preset personnalise.
constexpr wchar_t kCustomPresetId[] = L"custom";

// Cle INI de la couleur de fond.
constexpr wchar_t kColorBackgroundKey[] = L"color_background";

// Cle INI de la couleur de contour.
constexpr wchar_t kColorBorderKey[] = L"color_border";

// Cle INI de la couleur de texte.
constexpr wchar_t kColorTextKey[] = L"color_text";

// Cle INI de la couleur de texte secondaire.
constexpr wchar_t kColorSecondaryTextKey[] = L"color_secondary_text";

// Cle INI de la couleur de courbe historique.
constexpr wchar_t kColorHistoryKey[] = L"color_history";

// Cle INI de la couleur de barre restante.
constexpr wchar_t kColorRemainingKey[] = L"color_remaining";

// Cle INI de la couleur de barre consommee.
constexpr wchar_t kColorConsumedKey[] = L"color_consumed";

// Cle INI de la couleur des controles actifs.
constexpr wchar_t kColorActiveControlKey[] = L"color_active_control";

// Valeur minimale acceptee pour la largeur du widget.
constexpr int kMinimumWindowWidth = 96;

// Valeur minimale acceptee pour la hauteur du widget en mode minimal.
constexpr int kMinimumWindowHeight = 80;

// Opacite minimale acceptee pour conserver la lisibilite.
constexpr double kMinimumBackgroundOpacity = 0.20;

// Opacite maximale acceptee pour un fond opaque.
constexpr double kMaximumBackgroundOpacity = 1.00;

// Taille maximale du chemin d'executable Windows lu au demarrage.
constexpr DWORD kExecutablePathBufferLength = 32767;

// ----------------------------------------------------------------------------
// Assemble deux fragments de chemin Windows.
//
// Parametres :
// - base : chemin de base.
// - child : fragment a ajouter.
//
// Retour :
// - chemin combine avec un separateur si necessaire.
// ----------------------------------------------------------------------------
std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    std::wstring result = base;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/') {
        result.push_back(L'\\');
    }
    result += child;
    return result;
}

// ----------------------------------------------------------------------------
// Supprime le nom de fichier final d'un chemin Windows.
//
// Parametres :
// - path : chemin complet a tronquer.
//
// Retour :
// - dossier parent du chemin fourni.
// ----------------------------------------------------------------------------
std::wstring DirectoryName(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return L".";
    }

    return path.substr(0, separator);
}

// ----------------------------------------------------------------------------
// Retourne le dossier contenant l'executable courant.
//
// Retour :
// - chemin du dossier de l'executable, ou dossier courant en secours.
// ----------------------------------------------------------------------------
std::wstring GetSettingsDirectory() {
    std::wstring buffer(kExecutablePathBufferLength, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return L".";
    }

    buffer.resize(length);
    return DirectoryName(buffer);
}

// ----------------------------------------------------------------------------
// Lit un entier depuis le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a lire.
// - default_value : valeur retournee si la cle est absente.
//
// Retour :
// - valeur entiere lue.
// ----------------------------------------------------------------------------
int ReadIniInt(const std::wstring& path, const wchar_t* key, int default_value) {
    return static_cast<int>(GetPrivateProfileIntW(kSettingsSection, key, default_value, path.c_str()));
}

// ----------------------------------------------------------------------------
// Lit un booleen depuis le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a lire.
// - default_value : valeur retournee si la cle est absente.
//
// Retour :
// - true si la valeur lue est non nulle.
// - false sinon.
// ----------------------------------------------------------------------------
bool ReadIniBool(const std::wstring& path, const wchar_t* key, bool default_value) {
    return ReadIniInt(path, key, default_value ? 1 : 0) != 0;
}

// ----------------------------------------------------------------------------
// Lit une chaine depuis le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a lire.
// - default_value : valeur retournee si la cle est absente.
//
// Retour :
// - chaine lue ou valeur par defaut.
// ----------------------------------------------------------------------------
std::wstring ReadIniString(const std::wstring& path, const wchar_t* key, const wchar_t* default_value) {
    wchar_t value[64]{};
    GetPrivateProfileStringW(kSettingsSection, key, default_value, value, static_cast<DWORD>(std::size(value)), path.c_str());
    return value;
}

// ----------------------------------------------------------------------------
// Lit une chaine depuis une section precise du fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - section : section a lire.
// - key : cle a lire.
// - default_value : valeur retournee si la cle est absente.
//
// Retour :
// - chaine lue ou valeur par defaut.
// ----------------------------------------------------------------------------
std::wstring ReadIniStringFromSection(
    const std::wstring& path,
    const wchar_t* section,
    const wchar_t* key,
    const wchar_t* default_value
) {
    wchar_t value[256]{};
    GetPrivateProfileStringW(section, key, default_value, value, static_cast<DWORD>(std::size(value)), path.c_str());
    return value;
}

// ----------------------------------------------------------------------------
// Lit un nombre decimal depuis le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a lire.
// - default_value : valeur retournee si la cle est absente.
//
// Retour :
// - valeur decimal lue et bornee.
// ----------------------------------------------------------------------------
double ReadIniDouble(const std::wstring& path, const wchar_t* key, double default_value) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%.3f", default_value);

    wchar_t value[64]{};
    GetPrivateProfileStringW(kSettingsSection, key, buffer, value, static_cast<DWORD>(std::size(value)), path.c_str());
    return std::wcstod(value, nullptr);
}

// ----------------------------------------------------------------------------
// Ecrit une chaine dans le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a ecrire.
// - value : valeur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniString(const std::wstring& path, const wchar_t* key, const std::wstring& value) {
    return WritePrivateProfileStringW(kSettingsSection, key, value.c_str(), path.c_str()) != FALSE;
}

// ----------------------------------------------------------------------------
// Ecrit une chaine dans une section precise du fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - section : section a modifier.
// - key : cle a ecrire.
// - value : valeur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniStringToSection(
    const std::wstring& path,
    const wchar_t* section,
    const wchar_t* key,
    const std::wstring& value
) {
    return WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) != FALSE;
}

// ----------------------------------------------------------------------------
// Ecrit un entier dans le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a ecrire.
// - value : valeur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniInt(const std::wstring& path, const wchar_t* key, int value) {
    return WriteIniString(path, key, std::to_wstring(value));
}

// ----------------------------------------------------------------------------
// Ecrit un booleen dans le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a ecrire.
// - value : valeur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniBool(const std::wstring& path, const wchar_t* key, bool value) {
    return WriteIniInt(path, key, value ? 1 : 0);
}

// ----------------------------------------------------------------------------
// Ecrit un nombre decimal dans le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a ecrire.
// - value : valeur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniDouble(const std::wstring& path, const wchar_t* key, double value) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%.3f", value);
    return WriteIniString(path, key, buffer);
}

// ----------------------------------------------------------------------------
// Convertit une couleur Win32 en chaine hexadecimale #RRGGBB.
//
// Parametres :
// - color : couleur Win32 a convertir.
//
// Retour :
// - couleur encodee pour le fichier INI.
// ----------------------------------------------------------------------------
std::wstring ColorRefToHex(COLORREF color) {
    wchar_t buffer[8]{};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

// ----------------------------------------------------------------------------
// Convertit une chaine #RRGGBB en couleur Win32.
//
// Parametres :
// - value : chaine lue depuis le fichier INI.
//
// Retour :
// - couleur convertie si le format est valide.
// - valeur vide sinon.
// ----------------------------------------------------------------------------
std::optional<COLORREF> ColorRefFromHex(const std::wstring& value) {
    if (value.size() != 7 || value.front() != L'#') {
        return std::nullopt;
    }

    unsigned int red = 0;
    unsigned int green = 0;
    unsigned int blue = 0;
    if (swscanf_s(value.c_str(), L"#%02x%02x%02x", &red, &green, &blue) != 3) {
        return std::nullopt;
    }

    if (red > 0xFF || green > 0xFF || blue > 0xFF) {
        return std::nullopt;
    }

    return RGB(red, green, blue);
}

// ----------------------------------------------------------------------------
// Lit une couleur depuis le fichier INI avec secours robuste.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a lire.
// - default_color : couleur utilisee si la cle est absente ou invalide.
//
// Retour :
// - couleur lue ou couleur par defaut.
// ----------------------------------------------------------------------------
COLORREF ReadIniColor(const std::wstring& path, const wchar_t* key, COLORREF default_color) {
    const std::wstring value = ReadIniString(path, key, ColorRefToHex(default_color).c_str());
    const std::optional<COLORREF> color = ColorRefFromHex(value);
    return color.value_or(default_color);
}

// ----------------------------------------------------------------------------
// Lit une couleur depuis une section precise du fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - section : section a lire.
// - key : cle a lire.
// - default_color : couleur utilisee si la cle est absente ou invalide.
//
// Retour :
// - couleur lue ou couleur par defaut.
// ----------------------------------------------------------------------------
COLORREF ReadIniColorFromSection(
    const std::wstring& path,
    const wchar_t* section,
    const wchar_t* key,
    COLORREF default_color
) {
    const std::wstring value = ReadIniStringFromSection(path, section, key, ColorRefToHex(default_color).c_str());
    const std::optional<COLORREF> color = ColorRefFromHex(value);
    return color.value_or(default_color);
}

// ----------------------------------------------------------------------------
// Ecrit une couleur dans le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - key : cle a ecrire.
// - color : couleur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniColor(const std::wstring& path, const wchar_t* key, COLORREF color) {
    return WriteIniString(path, key, ColorRefToHex(color));
}

// ----------------------------------------------------------------------------
// Ecrit une couleur dans une section precise du fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - section : section a modifier.
// - key : cle a ecrire.
// - color : couleur a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteIniColorToSection(
    const std::wstring& path,
    const wchar_t* section,
    const wchar_t* key,
    COLORREF color
) {
    return WriteIniStringToSection(path, section, key, ColorRefToHex(color));
}

// ----------------------------------------------------------------------------
// Construit le nom de section INI d'un preset.
//
// Parametres :
// - preset_id : identifiant du preset.
//
// Retour :
// - section INI complete.
// ----------------------------------------------------------------------------
std::wstring ColorPresetSectionName(const std::wstring& preset_id) {
    return std::wstring(kColorPresetSectionPrefix) + preset_id;
}

// ----------------------------------------------------------------------------
// Indique si une chaine commence par un prefixe.
//
// Parametres :
// - value : chaine a tester.
// - prefix : prefixe attendu.
//
// Retour :
// - true si la chaine commence par le prefixe.
// - false sinon.
// ----------------------------------------------------------------------------
bool StartsWith(const std::wstring& value, const std::wstring& prefix) {
    return value.rfind(prefix, 0) == 0;
}

// ----------------------------------------------------------------------------
// Enumere les noms de sections du fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
//
// Retour :
// - noms de sections trouves.
// ----------------------------------------------------------------------------
std::vector<std::wstring> ReadIniSectionNames(const std::wstring& path) {
    std::vector<wchar_t> buffer(4096, L'\0');
    DWORD length = GetPrivateProfileSectionNamesW(buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
    while (length == buffer.size() - 2) {
        buffer.resize(buffer.size() * 2);
        length = GetPrivateProfileSectionNamesW(buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
    }

    std::vector<std::wstring> sections;
    const wchar_t* cursor = buffer.data();
    while (*cursor != L'\0') {
        sections.emplace_back(cursor);
        cursor += sections.back().size() + 1;
    }

    return sections;
}

// ----------------------------------------------------------------------------
// Recherche un preset par identifiant.
//
// Parametres :
// - presets : liste de presets a parcourir.
// - preset_id : identifiant recherche.
//
// Retour :
// - true si le preset existe.
// - false sinon.
// ----------------------------------------------------------------------------
bool ContainsPresetId(const std::vector<WidgetColorPreset>& presets, const std::wstring& preset_id) {
    return std::any_of(
        presets.begin(),
        presets.end(),
        [&preset_id](const WidgetColorPreset& preset) {
            return preset.id == preset_id;
        }
    );
}

// ----------------------------------------------------------------------------
// Lit un preset de couleurs depuis une section INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - preset_id : identifiant du preset.
// - default_preset : preset de secours pour les valeurs absentes.
//
// Retour :
// - preset lu.
// ----------------------------------------------------------------------------
WidgetColorPreset ReadColorPreset(
    const std::wstring& path,
    const std::wstring& preset_id,
    const WidgetColorPreset& default_preset
) {
    const std::wstring section = ColorPresetSectionName(preset_id);
    WidgetColorPreset preset = default_preset;
    preset.id = preset_id;
    preset.name = ReadIniStringFromSection(path, section.c_str(), kColorPresetNameKey, default_preset.name.c_str());
    preset.colors.background = ReadIniColorFromSection(path, section.c_str(), kColorBackgroundKey, default_preset.colors.background);
    preset.colors.border = ReadIniColorFromSection(path, section.c_str(), kColorBorderKey, default_preset.colors.border);
    preset.colors.text = ReadIniColorFromSection(path, section.c_str(), kColorTextKey, default_preset.colors.text);
    preset.colors.secondary_text = ReadIniColorFromSection(
        path,
        section.c_str(),
        kColorSecondaryTextKey,
        preset.colors.text
    );
    preset.colors.history_curve = ReadIniColorFromSection(path, section.c_str(), kColorHistoryKey, default_preset.colors.history_curve);
    preset.colors.remaining_bar = ReadIniColorFromSection(path, section.c_str(), kColorRemainingKey, default_preset.colors.remaining_bar);
    preset.colors.consumed_bar = ReadIniColorFromSection(path, section.c_str(), kColorConsumedKey, default_preset.colors.consumed_bar);
    preset.colors.active_control = ReadIniColorFromSection(path, section.c_str(), kColorActiveControlKey, default_preset.colors.active_control);
    return preset;
}

// ----------------------------------------------------------------------------
// Lit tous les presets de couleurs depuis le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
//
// Retour :
// - presets embarques plus presets ajoutes manuellement.
// ----------------------------------------------------------------------------
std::vector<WidgetColorPreset> LoadColorPresets(const std::wstring& path) {
    std::vector<WidgetColorPreset> presets;
    const std::vector<WidgetColorPreset> defaults = DefaultWidgetColorPresets();
    for (const WidgetColorPreset& default_preset : defaults) {
        WidgetColorPreset preset = ReadColorPreset(path, default_preset.id, default_preset);
        preset.name = default_preset.name;
        presets.push_back(preset);
    }

    const std::wstring prefix = kColorPresetSectionPrefix;
    for (const std::wstring& section : ReadIniSectionNames(path)) {
        if (!StartsWith(section, prefix)) {
            continue;
        }

        const std::wstring preset_id = section.substr(prefix.size());
        if (preset_id.empty() || ContainsPresetId(presets, preset_id)) {
            continue;
        }

        WidgetColorPreset fallback{};
        fallback.id = preset_id;
        fallback.name = preset_id == kCustomPresetId ? T(IDS_COLOR_PRESET_CUSTOM) : preset_id;
        fallback.colors = WidgetColorSettings{};
        presets.push_back(ReadColorPreset(path, preset_id, fallback));
    }

    return presets;
}

// ----------------------------------------------------------------------------
// Ecrit un preset de couleurs dans le fichier INI.
//
// Parametres :
// - path : chemin du fichier INI.
// - preset : preset a ecrire.
//
// Retour :
// - true si l'ecriture a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool WriteColorPreset(const std::wstring& path, const WidgetColorPreset& preset) {
    const std::wstring section = ColorPresetSectionName(preset.id);
    bool success = true;
    success = WriteIniStringToSection(path, section.c_str(), kColorPresetNameKey, preset.name) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorBackgroundKey, preset.colors.background) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorBorderKey, preset.colors.border) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorTextKey, preset.colors.text) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorSecondaryTextKey, preset.colors.secondary_text) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorHistoryKey, preset.colors.history_curve) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorRemainingKey, preset.colors.remaining_bar) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorConsumedKey, preset.colors.consumed_bar) && success;
    success = WriteIniColorToSection(path, section.c_str(), kColorActiveControlKey, preset.colors.active_control) && success;
    return success;
}

// ----------------------------------------------------------------------------
// Verifie qu'un rectangle possede une taille exploitable.
//
// Parametres :
// - rect : rectangle a verifier.
//
// Retour :
// - true si la taille est suffisante.
// - false sinon.
// ----------------------------------------------------------------------------
bool IsUsableWindowRect(const RECT& rect) {
    return rect.right - rect.left >= kMinimumWindowWidth
        && rect.bottom - rect.top >= kMinimumWindowHeight;
}

// ----------------------------------------------------------------------------
// Convertit un entier INI en mode d'affichage.
//
// Parametres :
// - value : valeur entiere lue.
//
// Retour :
// - mode d'affichage correspondant.
// ----------------------------------------------------------------------------
WidgetDisplayMode DisplayModeFromInt(int value) {
    switch (value) {
    case 2:
        return WidgetDisplayMode::Minimal;

    case 3:
        return WidgetDisplayMode::Horizontal;

    case 4:
        return WidgetDisplayMode::Vertical;

    case 1:
        return WidgetDisplayMode::Complete;

    case 0:
    default:
        return WidgetDisplayMode::Compact;
    }
}

// ----------------------------------------------------------------------------
// Convertit un mode d'affichage en entier INI.
//
// Parametres :
// - mode : mode d'affichage a convertir.
//
// Retour :
// - valeur entiere persistable.
// ----------------------------------------------------------------------------
int DisplayModeToInt(WidgetDisplayMode mode) {
    switch (mode) {
    case WidgetDisplayMode::Minimal:
        return 2;

    case WidgetDisplayMode::Complete:
        return 1;

    case WidgetDisplayMode::Horizontal:
        return 3;

    case WidgetDisplayMode::Vertical:
        return 4;

    case WidgetDisplayMode::Compact:
    default:
        return 0;
    }
}

// ----------------------------------------------------------------------------
// Convertit une chaine INI en langue d'interface.
//
// Parametres :
// - value : valeur texte lue.
//
// Retour :
// - langue d'interface correspondante.
// ----------------------------------------------------------------------------
UiLanguage LanguageFromString(const std::wstring& value) {
    if (value == L"fr") {
        return UiLanguage::French;
    }

    if (value == L"en") {
        return UiLanguage::English;
    }

    return UiLanguage::Auto;
}

// ----------------------------------------------------------------------------
// Convertit une langue d'interface en chaine INI.
//
// Parametres :
// - language : langue a convertir.
//
// Retour :
// - valeur texte persistable.
// ----------------------------------------------------------------------------
std::wstring LanguageToString(UiLanguage language) {
    switch (language) {
    case UiLanguage::French:
        return L"fr";

    case UiLanguage::English:
        return L"en";

    case UiLanguage::Auto:
    default:
        return L"auto";
    }
}

}  // namespace

// ----------------------------------------------------------------------------
// Retourne le chemin du fichier de reglages local.
//
// Retour :
// - chemin complet vers settings.ini.
// ----------------------------------------------------------------------------
std::wstring GetAppSettingsPath() {
    return JoinPath(GetSettingsDirectory(), kSettingsFileName);
}

// ----------------------------------------------------------------------------
// Retourne les presets de couleurs fournis par defaut.
//
// Retour :
// - liste des presets embarques servant de secours initial.
// ----------------------------------------------------------------------------
std::vector<WidgetColorPreset> DefaultWidgetColorPresets() {
    return std::vector<WidgetColorPreset>{
        WidgetColorPreset{
            kCodexGlassPresetId,
            T(IDS_COLOR_PRESET_CODEX_GLASS),
            WidgetColorSettings{},
        },
        WidgetColorPreset{
            kWindowsDarkPresetId,
            T(IDS_COLOR_PRESET_WINDOWS_DARK),
            WidgetColorSettings{
                RGB(0x20, 0x20, 0x20),
                RGB(0x4A, 0x4A, 0x4A),
                RGB(0xF2, 0xF2, 0xF2),
                RGB(0xF2, 0xF2, 0xF2),
                RGB(0x60, 0xA5, 0xFA),
                RGB(0x60, 0xA5, 0xFA),
                RGB(0xD6, 0xD6, 0xD6),
                RGB(0x60, 0xA5, 0xFA),
            },
        },
        WidgetColorPreset{
            kWindowsLightPresetId,
            T(IDS_COLOR_PRESET_WINDOWS_LIGHT),
            WidgetColorSettings{
                RGB(0xF3, 0xF3, 0xF3),
                RGB(0xC8, 0xC8, 0xC8),
                RGB(0x1F, 0x1F, 0x1F),
                RGB(0x1F, 0x1F, 0x1F),
                RGB(0x16, 0xA3, 0x4A),
                RGB(0x16, 0xA3, 0x4A),
                RGB(0xD1, 0xD5, 0xDB),
                RGB(0x16, 0xA3, 0x4A),
            },
        },
        WidgetColorPreset{
            kTerminalGreenPresetId,
            T(IDS_COLOR_PRESET_TERMINAL_GREEN),
            WidgetColorSettings{
                RGB(0x08, 0x0C, 0x08),
                RGB(0x1F, 0x3B, 0x24),
                RGB(0xB8, 0xF7, 0xC5),
                RGB(0xB8, 0xF7, 0xC5),
                RGB(0x22, 0xC5, 0x5E),
                RGB(0x22, 0xC5, 0x5E),
                RGB(0x2E, 0x45, 0x34),
                RGB(0x22, 0xC5, 0x5E),
            },
        },
        WidgetColorPreset{
            kHighContrastPresetId,
            T(IDS_COLOR_PRESET_HIGH_CONTRAST),
            WidgetColorSettings{
                RGB(0x00, 0x00, 0x00),
                RGB(0xFF, 0xFF, 0xFF),
                RGB(0xFF, 0xFF, 0xFF),
                RGB(0xFF, 0xFF, 0xFF),
                RGB(0x00, 0xFF, 0x66),
                RGB(0x00, 0xFF, 0x66),
                RGB(0x5C, 0x5C, 0x5C),
                RGB(0x00, 0xFF, 0x66),
            },
        },
    };
}

// ----------------------------------------------------------------------------
// Convertit une valeur INI en plage temporelle de graphe.
//
// Parametres :
// - value : valeur entiere lue depuis un fichier existant.
//
// Retour :
// - plage valide, vingt-quatre heures servant de repli.
// ----------------------------------------------------------------------------
GraphRange GraphRangeFromInt(int value) {
    switch (value) {
    case 5:
        return GraphRange::Minutes5;
    case 4:
        return GraphRange::Hours1;
    case 0:
        return GraphRange::Hours5;
    case 2:
        return GraphRange::Days7;
    case 3:
        return GraphRange::Days30;
    case 1:
    default:
        return GraphRange::Hours24;
    }
}

// ----------------------------------------------------------------------------
// Convertit une plage temporelle en valeur INI stable.
//
// Parametres :
// - range : plage a persister.
//
// Retour :
// - valeur stable comprise entre zero et cinq.
// ----------------------------------------------------------------------------
int GraphRangeToInt(GraphRange range) {
    switch (range) {
    case GraphRange::Minutes5:
        return 5;
    case GraphRange::Hours1:
        return 4;
    case GraphRange::Hours5:
        return 0;
    case GraphRange::Days7:
        return 2;
    case GraphRange::Days30:
        return 3;
    case GraphRange::Hours24:
    default:
        return 1;
    }
}

// ----------------------------------------------------------------------------
// Relocalise les noms des presets embarques dans la langue courante.
// ----------------------------------------------------------------------------
void LocalizeWidgetColorPresetNames(AppSettings& settings) {
    const std::vector<WidgetColorPreset> defaults = DefaultWidgetColorPresets();
    for (WidgetColorPreset& preset : settings.color_presets) {
        const auto default_preset = std::find_if(
            defaults.begin(),
            defaults.end(),
            [&preset](const WidgetColorPreset& candidate) {
                return candidate.id == preset.id;
            }
        );
        if (default_preset != defaults.end()) {
            preset.name = default_preset->name;
            continue;
        }

        if (preset.id == kCustomPresetId) {
            preset.name = T(IDS_COLOR_PRESET_CUSTOM);
        }
    }
}

// ----------------------------------------------------------------------------
// Charge les reglages locaux depuis le fichier de configuration.
//
// Parametres :
// - default_rect : rectangle utilise quand aucun reglage valide n'existe.
//
// Retour :
// - reglages charges avec valeurs par defaut robustes.
// ----------------------------------------------------------------------------
AppSettings LoadAppSettings(const RECT& default_rect) {
    const std::wstring path = GetAppSettingsPath();

    AppSettings settings{};
    settings.window_rect = RECT{
        ReadIniInt(path, L"left", default_rect.left),
        ReadIniInt(path, L"top", default_rect.top),
        ReadIniInt(path, L"right", default_rect.right),
        ReadIniInt(path, L"bottom", default_rect.bottom),
    };
    if (!IsUsableWindowRect(settings.window_rect)) {
        settings.window_rect = default_rect;
    }

    settings.always_on_top = ReadIniBool(path, L"always_on_top", settings.always_on_top);
    settings.lock_position = ReadIniBool(path, L"lock_position", settings.lock_position);
    settings.dock_to_screen_edges = ReadIniBool(path, L"dock_to_screen_edges", settings.dock_to_screen_edges);
    settings.background_opacity = std::clamp(
        ReadIniDouble(path, L"background_opacity", settings.background_opacity),
        kMinimumBackgroundOpacity,
        kMaximumBackgroundOpacity
    );
    settings.refresh_interval_seconds = std::max(0, ReadIniInt(path, L"refresh_interval_seconds", settings.refresh_interval_seconds));
    settings.display_mode = DisplayModeFromInt(ReadIniInt(path, L"display_mode", DisplayModeToInt(settings.display_mode)));
    settings.colors.background = ReadIniColor(path, L"color_background", settings.colors.background);
    settings.colors.border = ReadIniColor(path, L"color_border", settings.colors.border);
    settings.colors.text = ReadIniColor(path, L"color_text", settings.colors.text);
    settings.colors.secondary_text = ReadIniColor(
        path,
        kColorSecondaryTextKey,
        settings.colors.text
    );
    settings.colors.history_curve = ReadIniColor(path, L"color_history", settings.colors.history_curve);
    settings.colors.remaining_bar = ReadIniColor(path, L"color_remaining", settings.colors.remaining_bar);
    settings.colors.consumed_bar = ReadIniColor(path, L"color_consumed", settings.colors.consumed_bar);
    settings.colors.active_control = ReadIniColor(path, kColorActiveControlKey, settings.colors.active_control);
    settings.language = LanguageFromString(ReadIniString(path, L"language", LanguageToString(settings.language).c_str()));
    SetUiLanguage(settings.language);
    settings.show_graph = ReadIniBool(path, L"show_graph", settings.show_graph);
    settings.quota_visibility.five_hour = ReadIniBool(
        path,
        L"show_quota_5h",
        settings.quota_visibility.five_hour
    );
    settings.quota_visibility.weekly = ReadIniBool(
        path,
        L"show_quota_weekly",
        settings.quota_visibility.weekly
    );
    settings.quota_visibility.spark_five_hour = ReadIniBool(
        path,
        L"show_quota_spark_5h",
        settings.quota_visibility.spark_five_hour
    );
    settings.quota_visibility.spark_weekly = ReadIniBool(
        path,
        L"show_quota_spark_weekly",
        settings.quota_visibility.spark_weekly
    );
    settings.graph_page = WidgetGraphPageFromStoredValue(ReadIniInt(path, L"graph_page", 0));
    settings.graph_range = GraphRangeFromInt(ReadIniInt(
        path,
        L"graph_range",
        GraphRangeToInt(settings.graph_range)
    ));
    settings.token_graph_range = GraphRangeFromInt(ReadIniInt(
        path,
        L"token_graph_range",
        GraphRangeToInt(settings.token_graph_range)
    ));
    if (settings.token_graph_range != GraphRange::Hours1
        && settings.token_graph_range != GraphRange::Hours5
        && settings.token_graph_range != GraphRange::Hours24) {
        settings.token_graph_range = GraphRange::Hours5;
    }
    settings.activity_graph_range = GraphRangeFromInt(ReadIniInt(
        path,
        L"activity_graph_range",
        GraphRangeToInt(settings.activity_graph_range)
    ));
    if (settings.activity_graph_range != GraphRange::Minutes5
        && settings.activity_graph_range != GraphRange::Days30) {
        settings.activity_graph_range = GraphRange::Days30;
    }
    settings.summary_graph_range = GraphRangeFromInt(ReadIniInt(
        path,
        L"summary_graph_range",
        GraphRangeToInt(settings.summary_graph_range)
    ));
    if (settings.summary_graph_range != GraphRange::Hours1
        && settings.summary_graph_range != GraphRange::Hours5
        && settings.summary_graph_range != GraphRange::Hours24
        && settings.summary_graph_range != GraphRange::Days7
        && settings.summary_graph_range != GraphRange::Days30) {
        settings.summary_graph_range = GraphRange::Days7;
    }
    settings.click_through = ReadIniBool(path, L"click_through", settings.click_through);
    settings.hide_when_fullscreen = ReadIniBool(path, L"hide_when_fullscreen", settings.hide_when_fullscreen);
    settings.start_with_windows = ReadIniBool(path, L"start_with_windows", settings.start_with_windows);
    settings.glass_effect_mode = GlassEffectModeFromString(
        ReadIniString(path, L"glass_effect_mode", GlassEffectModeToString(settings.glass_effect_mode).c_str())
    );
    const GlassEffectPreset legacy_glass_effect_preset = GlassEffectPresetFromString(
        ReadIniString(path, L"glass_effect_preset", GlassEffectPresetToString(settings.glass_effect.preset).c_str())
    );
    const LegacyGlassEffectAnimationMode legacy_glass_effect_animation_mode = LegacyGlassEffectAnimationModeFromString(ReadIniString(
        path,
        L"glass_effect_animation_mode",
        LegacyGlassEffectAnimationModeToString(DefaultLegacyGlassEffectAnimationMode()).c_str()
    ));
    LegacyGlassEffectAnimationOptions legacy_glass_effect_animation{};
    legacy_glass_effect_animation.intensity_percent = ReadIniInt(
        path,
        L"glass_effect_animation_intensity_percent",
        legacy_glass_effect_animation.intensity_percent
    );
    legacy_glass_effect_animation.speed_percent = ReadIniInt(
        path,
        L"glass_effect_animation_speed_percent",
        legacy_glass_effect_animation.speed_percent
    );
    legacy_glass_effect_animation.rain_density_percent = ReadIniInt(
        path,
        L"glass_effect_rain_density_percent",
        legacy_glass_effect_animation.rain_density_percent
    );
    legacy_glass_effect_animation.rain_ring_size_percent = ReadIniInt(
        path,
        L"glass_effect_rain_ring_size_percent",
        legacy_glass_effect_animation.rain_ring_size_percent
    );
    legacy_glass_effect_animation = NormalizeLegacyGlassEffectAnimationOptions(legacy_glass_effect_animation);
    const int glass_effect_schema_version = ReadIniInt(path, L"glass_effect_schema_version", 0);
    if (glass_effect_schema_version >= 1) {
        settings.glass_effect.schema_version = glass_effect_schema_version;
        settings.glass_effect.appearance.diffusion_percent = ReadIniInt(
            path, L"glass_appearance_diffusion_percent", settings.glass_effect.appearance.diffusion_percent
        );
        settings.glass_effect.appearance.tint_percent = ReadIniInt(
            path, L"glass_appearance_tint_percent", settings.glass_effect.appearance.tint_percent
        );
        settings.glass_effect.appearance.grain_percent = ReadIniInt(
            path, L"glass_appearance_grain_percent", settings.glass_effect.appearance.grain_percent
        );
        settings.glass_effect.appearance.edge_refraction_percent = ReadIniInt(
            path, L"glass_appearance_edge_refraction_percent", settings.glass_effect.appearance.edge_refraction_percent
        );
        settings.glass_effect.appearance.edge_width_percent = ReadIniInt(
            path, L"glass_appearance_edge_width_percent", settings.glass_effect.appearance.edge_width_percent
        );
        settings.glass_effect.appearance.chromatic_aberration_percent = ReadIniInt(
            path, L"glass_appearance_chromatic_percent", settings.glass_effect.appearance.chromatic_aberration_percent
        );
        settings.glass_effect.appearance.indicator_refraction_percent = ReadIniInt(
            path, L"glass_appearance_indicator_refraction_percent", settings.glass_effect.appearance.indicator_refraction_percent
        );
        settings.glass_effect.appearance.indicator_width_percent = ReadIniInt(
            path, L"glass_appearance_indicator_width_percent", settings.glass_effect.appearance.indicator_width_percent
        );
        settings.glass_effect.appearance.element_glass_enabled = ReadIniBool(
            path, L"glass_appearance_element_glass_enabled", settings.glass_effect.appearance.element_glass_enabled
        );
        settings.glass_effect.appearance.element_softness_percent = ReadIniInt(
            path, L"glass_appearance_element_softness_percent", settings.glass_effect.appearance.element_softness_percent
        );
        settings.glass_effect.calm_water.enabled = ReadIniBool(
            path, L"glass_calm_water_enabled", settings.glass_effect.calm_water.enabled
        );
        settings.glass_effect.calm_water.intensity_percent = ReadIniInt(
            path, L"glass_calm_water_intensity_percent", settings.glass_effect.calm_water.intensity_percent
        );
        settings.glass_effect.calm_water.speed_percent = ReadIniInt(
            path, L"glass_calm_water_speed_percent", settings.glass_effect.calm_water.speed_percent
        );
        settings.glass_effect.calm_water.wavelength_percent = ReadIniInt(
            path, L"glass_calm_water_wavelength_percent", settings.glass_effect.calm_water.wavelength_percent
        );
        settings.glass_effect.calm_water.noise_percent = ReadIniInt(
            path, L"glass_calm_water_noise_percent", settings.glass_effect.calm_water.noise_percent
        );
        settings.glass_effect.liquid.enabled = ReadIniBool(
            path, L"glass_liquid_enabled", settings.glass_effect.liquid.enabled
        );
        settings.glass_effect.liquid.intensity_percent = ReadIniInt(
            path, L"glass_liquid_intensity_percent", settings.glass_effect.liquid.intensity_percent
        );
        settings.glass_effect.liquid.speed_percent = ReadIniInt(
            path, L"glass_liquid_speed_percent", settings.glass_effect.liquid.speed_percent
        );
        settings.glass_effect.liquid.wavelength_percent = ReadIniInt(
            path, L"glass_liquid_wavelength_percent", settings.glass_effect.liquid.wavelength_percent
        );
        settings.glass_effect.liquid.fluidity_percent = ReadIniInt(
            path, L"glass_liquid_fluidity_percent", settings.glass_effect.liquid.fluidity_percent
        );
        settings.glass_effect.liquid.noise_percent = ReadIniInt(
            path, L"glass_liquid_noise_percent", settings.glass_effect.liquid.noise_percent
        );
        settings.glass_effect.rain.enabled = ReadIniBool(
            path, L"glass_rain_enabled", settings.glass_effect.rain.enabled
        );
        settings.glass_effect.rain.intensity_percent = ReadIniInt(
            path, L"glass_rain_intensity_percent", settings.glass_effect.rain.intensity_percent
        );
        settings.glass_effect.rain.speed_percent = ReadIniInt(
            path, L"glass_rain_speed_percent", settings.glass_effect.rain.speed_percent
        );
        settings.glass_effect.rain.density_percent = ReadIniInt(
            path, L"glass_rain_density_percent", settings.glass_effect.rain.density_percent
        );
        settings.glass_effect.rain.ring_size_percent = ReadIniInt(
            path, L"glass_rain_ring_size_percent", settings.glass_effect.rain.ring_size_percent
        );
        settings.glass_effect.rain.fade_percent = ReadIniInt(
            path, L"glass_rain_fade_percent", settings.glass_effect.rain.fade_percent
        );
    } else {
        settings.glass_effect = DefaultGlassEffectSettings();
        settings.glass_effect.preset = legacy_glass_effect_preset;
        settings.glass_effect.appearance = GlassEffectAppearanceForPreset(legacy_glass_effect_preset);
        settings.glass_effect.rain.density_percent = legacy_glass_effect_animation.rain_density_percent;
        settings.glass_effect.rain.ring_size_percent = legacy_glass_effect_animation.rain_ring_size_percent;
        switch (legacy_glass_effect_animation_mode) {
        case LegacyGlassEffectAnimationMode::CalmWater:
            settings.glass_effect.calm_water.enabled = true;
            settings.glass_effect.calm_water.intensity_percent = legacy_glass_effect_animation.intensity_percent;
            settings.glass_effect.calm_water.speed_percent = legacy_glass_effect_animation.speed_percent;
            break;
        case LegacyGlassEffectAnimationMode::Liquid:
            settings.glass_effect.liquid.enabled = true;
            settings.glass_effect.liquid.intensity_percent = legacy_glass_effect_animation.intensity_percent;
            settings.glass_effect.liquid.speed_percent = legacy_glass_effect_animation.speed_percent;
            break;
        case LegacyGlassEffectAnimationMode::Rain:
            settings.glass_effect.rain.enabled = true;
            settings.glass_effect.rain.intensity_percent = legacy_glass_effect_animation.intensity_percent;
            settings.glass_effect.rain.speed_percent = legacy_glass_effect_animation.speed_percent;
            break;
        case LegacyGlassEffectAnimationMode::Off:
        default:
            break;
        }
    }
    settings.glass_effect = NormalizeGlassEffectSettings(settings.glass_effect);
    WidgetVibrationSettings legacy_vibration{};
    legacy_vibration.enabled = ReadIniBool(path, L"vibration_enabled", legacy_vibration.enabled);
    legacy_vibration.motion_enabled = ReadIniBool(path, L"vibration_motion_enabled", legacy_vibration.motion_enabled);
    legacy_vibration.glass_effect_enabled = ReadIniBool(
        path,
        L"vibration_glass_effect_enabled",
        legacy_vibration.glass_effect_enabled
    );
    legacy_vibration.usage_drop_threshold_percent = ReadIniInt(
        path,
        L"vibration_usage_drop_threshold_percent",
        legacy_vibration.usage_drop_threshold_percent
    );
    legacy_vibration.minimum_interval_seconds = ReadIniInt(
        path,
        L"vibration_minimum_interval_seconds",
        legacy_vibration.minimum_interval_seconds
    );
    legacy_vibration.intensity_percent = ReadIniInt(
        path,
        L"vibration_intensity_percent",
        legacy_vibration.intensity_percent
    );
    legacy_vibration.duration_ms = ReadIniInt(
        path,
        L"vibration_duration_ms",
        legacy_vibration.duration_ms
    );
    legacy_vibration.style = WidgetVibrationStyleFromString(
        ReadIniString(path, L"vibration_style", WidgetVibrationStyleToString(legacy_vibration.style).c_str())
    );
    legacy_vibration = NormalizeWidgetVibrationSettings(legacy_vibration);
    settings.motion_effects = LoadWidgetMotionEffectsSettings(path, legacy_vibration);
    settings.color_presets = LoadColorPresets(path);
    return settings;
}

// ----------------------------------------------------------------------------
// Sauvegarde les reglages locaux dans le fichier de configuration.
//
// Parametres :
// - settings : reglages a persister.
//
// Retour :
// - true si la sauvegarde a reussi.
// - false sinon.
// ----------------------------------------------------------------------------
bool SaveAppSettings(const AppSettings& settings) {
    const std::wstring path = GetAppSettingsPath();

    bool success = true;
    success = WriteIniInt(path, L"left", settings.window_rect.left) && success;
    success = WriteIniInt(path, L"top", settings.window_rect.top) && success;
    success = WriteIniInt(path, L"right", settings.window_rect.right) && success;
    success = WriteIniInt(path, L"bottom", settings.window_rect.bottom) && success;
    success = WriteIniBool(path, L"always_on_top", settings.always_on_top) && success;
    success = WriteIniBool(path, L"lock_position", settings.lock_position) && success;
    success = WriteIniBool(path, L"dock_to_screen_edges", settings.dock_to_screen_edges) && success;
    success = WriteIniDouble(path, L"background_opacity", settings.background_opacity) && success;
    success = WriteIniInt(path, L"refresh_interval_seconds", settings.refresh_interval_seconds) && success;
    success = WriteIniInt(path, L"display_mode", DisplayModeToInt(settings.display_mode)) && success;
    success = WriteIniColor(path, L"color_background", settings.colors.background) && success;
    success = WriteIniColor(path, L"color_border", settings.colors.border) && success;
    success = WriteIniColor(path, L"color_text", settings.colors.text) && success;
    success = WriteIniColor(path, kColorSecondaryTextKey, settings.colors.secondary_text) && success;
    success = WriteIniColor(path, L"color_history", settings.colors.history_curve) && success;
    success = WriteIniColor(path, L"color_remaining", settings.colors.remaining_bar) && success;
    success = WriteIniColor(path, L"color_consumed", settings.colors.consumed_bar) && success;
    success = WriteIniColor(path, kColorActiveControlKey, settings.colors.active_control) && success;
    success = WriteIniString(path, L"language", LanguageToString(settings.language)) && success;
    success = WriteIniBool(path, L"show_graph", settings.show_graph) && success;
    success = WriteIniBool(
        path,
        L"show_quota_5h",
        settings.quota_visibility.five_hour
    ) && success;
    success = WriteIniBool(
        path,
        L"show_quota_weekly",
        settings.quota_visibility.weekly
    ) && success;
    success = WriteIniBool(
        path,
        L"show_quota_spark_5h",
        settings.quota_visibility.spark_five_hour
    ) && success;
    success = WriteIniBool(
        path,
        L"show_quota_spark_weekly",
        settings.quota_visibility.spark_weekly
    ) && success;
    success = WriteIniInt(
        path,
        L"graph_page",
        WidgetGraphPageToStoredValue(settings.graph_page)
    ) && success;
    success = WriteIniInt(path, L"graph_range", GraphRangeToInt(settings.graph_range)) && success;
    success = WriteIniInt(
        path,
        L"token_graph_range",
        GraphRangeToInt(settings.token_graph_range)
    ) && success;
    success = WriteIniInt(
        path,
        L"activity_graph_range",
        GraphRangeToInt(settings.activity_graph_range)
    ) && success;
    success = WriteIniInt(
        path,
        L"summary_graph_range",
        GraphRangeToInt(settings.summary_graph_range)
    ) && success;
    success = WriteIniBool(path, L"click_through", settings.click_through) && success;
    success = WriteIniBool(path, L"hide_when_fullscreen", settings.hide_when_fullscreen) && success;
    success = WriteIniBool(path, L"start_with_windows", settings.start_with_windows) && success;
    success = WriteIniString(path, L"glass_effect_mode", GlassEffectModeToString(settings.glass_effect_mode)) && success;
    const GlassEffectSettings glass_effect = NormalizeGlassEffectSettings(settings.glass_effect);
    success = WriteIniInt(path, L"glass_effect_schema_version", glass_effect.schema_version) && success;
    success = WriteIniString(path, L"glass_effect_preset", GlassEffectPresetToString(glass_effect.preset)) && success;
    success = WriteIniInt(path, L"glass_appearance_diffusion_percent", glass_effect.appearance.diffusion_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_tint_percent", glass_effect.appearance.tint_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_grain_percent", glass_effect.appearance.grain_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_edge_refraction_percent", glass_effect.appearance.edge_refraction_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_edge_width_percent", glass_effect.appearance.edge_width_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_chromatic_percent", glass_effect.appearance.chromatic_aberration_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_indicator_refraction_percent", glass_effect.appearance.indicator_refraction_percent) && success;
    success = WriteIniInt(path, L"glass_appearance_indicator_width_percent", glass_effect.appearance.indicator_width_percent) && success;
    success = WriteIniBool(path, L"glass_appearance_element_glass_enabled", glass_effect.appearance.element_glass_enabled) && success;
    success = WriteIniInt(path, L"glass_appearance_element_softness_percent", glass_effect.appearance.element_softness_percent) && success;
    success = WriteIniBool(path, L"glass_calm_water_enabled", glass_effect.calm_water.enabled) && success;
    success = WriteIniInt(path, L"glass_calm_water_intensity_percent", glass_effect.calm_water.intensity_percent) && success;
    success = WriteIniInt(path, L"glass_calm_water_speed_percent", glass_effect.calm_water.speed_percent) && success;
    success = WriteIniInt(path, L"glass_calm_water_wavelength_percent", glass_effect.calm_water.wavelength_percent) && success;
    success = WriteIniInt(path, L"glass_calm_water_noise_percent", glass_effect.calm_water.noise_percent) && success;
    success = WriteIniBool(path, L"glass_liquid_enabled", glass_effect.liquid.enabled) && success;
    success = WriteIniInt(path, L"glass_liquid_intensity_percent", glass_effect.liquid.intensity_percent) && success;
    success = WriteIniInt(path, L"glass_liquid_speed_percent", glass_effect.liquid.speed_percent) && success;
    success = WriteIniInt(path, L"glass_liquid_wavelength_percent", glass_effect.liquid.wavelength_percent) && success;
    success = WriteIniInt(path, L"glass_liquid_fluidity_percent", glass_effect.liquid.fluidity_percent) && success;
    success = WriteIniInt(path, L"glass_liquid_noise_percent", glass_effect.liquid.noise_percent) && success;
    success = WriteIniBool(path, L"glass_rain_enabled", glass_effect.rain.enabled) && success;
    success = WriteIniInt(path, L"glass_rain_intensity_percent", glass_effect.rain.intensity_percent) && success;
    success = WriteIniInt(path, L"glass_rain_speed_percent", glass_effect.rain.speed_percent) && success;
    success = WriteIniInt(path, L"glass_rain_density_percent", glass_effect.rain.density_percent) && success;
    success = WriteIniInt(path, L"glass_rain_ring_size_percent", glass_effect.rain.ring_size_percent) && success;
    success = WriteIniInt(path, L"glass_rain_fade_percent", glass_effect.rain.fade_percent) && success;
    success = SaveWidgetMotionEffectsSettings(path, settings.motion_effects) && success;
    for (const WidgetColorPreset& preset : settings.color_presets) {
        success = WriteColorPreset(path, preset) && success;
    }
    return success;
}

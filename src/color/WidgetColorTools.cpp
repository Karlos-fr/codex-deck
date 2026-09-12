// ============================================================================
// Codex Glass - Implementation des outils de couleurs
// ----------------------------------------------------------------------------
// Ce fichier regroupe les operations de couleur reutilisables par le panneau
// integre, le menu et les anciennes interfaces pendant la migration.
// ============================================================================

#include "WidgetColorTools.h"

#include "../localization/Localization.h"
#include "../resources/ResourceIds.h"

#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <random>

namespace {

// Identifiant INI du preset personnalise.
constexpr wchar_t kCustomColorPresetId[] = L"custom";

// Valeur maximale d'un canal RGB.
constexpr double kMaximumColorChannel = 255.0;

// ----------------------------------------------------------------------------
// Encadre une valeur flottante dans un intervalle.
//
// Parametres :
// - value : valeur a encadrer.
// - minimum : borne basse.
// - maximum : borne haute.
//
// Retour :
// - valeur encadree.
// ----------------------------------------------------------------------------
double ClampDouble(double value, double minimum, double maximum) {
    return std::clamp(value, minimum, maximum);
}

// ----------------------------------------------------------------------------
// Convertit une composante normalisee en canal RGB.
//
// Parametres :
// - value : composante normalisee entre 0 et 1.
//
// Retour :
// - canal RGB entre 0 et 255.
// ----------------------------------------------------------------------------
BYTE ToRgbChannel(double value) {
    return static_cast<BYTE>(std::round(ClampDouble(value, 0.0, 1.0) * kMaximumColorChannel));
}

// ----------------------------------------------------------------------------
// Convertit une couleur HSL en couleur Win32.
//
// Parametres :
// - hue : teinte entre 0 et 1.
// - saturation : saturation entre 0 et 1.
// - lightness : luminosite entre 0 et 1.
//
// Retour :
// - couleur RGB Win32.
// ----------------------------------------------------------------------------
COLORREF HslToColorRef(double hue, double saturation, double lightness) {
    hue = hue - std::floor(hue);
    saturation = ClampDouble(saturation, 0.0, 1.0);
    lightness = ClampDouble(lightness, 0.0, 1.0);

    const double chroma = (1.0 - std::abs((2.0 * lightness) - 1.0)) * saturation;
    const double hue_section = hue * 6.0;
    const double x = chroma * (1.0 - std::abs(std::fmod(hue_section, 2.0) - 1.0));
    const double match = lightness - (chroma / 2.0);

    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    if (hue_section < 1.0) {
        red = chroma;
        green = x;
    } else if (hue_section < 2.0) {
        red = x;
        green = chroma;
    } else if (hue_section < 3.0) {
        green = chroma;
        blue = x;
    } else if (hue_section < 4.0) {
        green = x;
        blue = chroma;
    } else if (hue_section < 5.0) {
        red = x;
        blue = chroma;
    } else {
        red = chroma;
        blue = x;
    }

    return RGB(ToRgbChannel(red + match), ToRgbChannel(green + match), ToRgbChannel(blue + match));
}

// ----------------------------------------------------------------------------
// Melange deux couleurs RGB.
//
// Parametres :
// - first : premiere couleur.
// - second : seconde couleur.
// - ratio : part de la seconde couleur.
//
// Retour :
// - couleur melangee.
// ----------------------------------------------------------------------------
COLORREF MixColors(COLORREF first, COLORREF second, double ratio) {
    ratio = ClampDouble(ratio, 0.0, 1.0);
    const double inverse_ratio = 1.0 - ratio;
    return RGB(
        static_cast<BYTE>(std::round((GetRValue(first) * inverse_ratio) + (GetRValue(second) * ratio))),
        static_cast<BYTE>(std::round((GetGValue(first) * inverse_ratio) + (GetGValue(second) * ratio))),
        static_cast<BYTE>(std::round((GetBValue(first) * inverse_ratio) + (GetBValue(second) * ratio)))
    );
}

// ----------------------------------------------------------------------------
// Retourne une valeur aleatoire dans un intervalle.
//
// Parametres :
// - generator : generateur pseudo-aleatoire.
// - minimum : borne basse.
// - maximum : borne haute.
//
// Retour :
// - valeur aleatoire.
// ----------------------------------------------------------------------------
double RandomDouble(std::mt19937& generator, double minimum, double maximum) {
    std::uniform_real_distribution<double> distribution(minimum, maximum);
    return distribution(generator);
}

}  // namespace

// ----------------------------------------------------------------------------
// Affiche le selecteur de couleur Windows natif.
// ----------------------------------------------------------------------------
bool ChooseWidgetColor(HWND hwnd, COLORREF& color) {
    static COLORREF custom_colors[16]{};
    CHOOSECOLORW choose_color{};
    choose_color.lStructSize = sizeof(choose_color);
    choose_color.hwndOwner = hwnd;
    choose_color.rgbResult = color;
    choose_color.lpCustColors = custom_colors;
    choose_color.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&choose_color) == FALSE) {
        return false;
    }

    color = choose_color.rgbResult;
    return true;
}

// ----------------------------------------------------------------------------
// Lit une couleur depuis les reglages par champ logique.
// ----------------------------------------------------------------------------
COLORREF GetWidgetColorField(const WidgetColorSettings& colors, WidgetColorField field) {
    switch (field) {
    case WidgetColorField::Background:
        return colors.background;
    case WidgetColorField::Border:
        return colors.border;
    case WidgetColorField::Text:
        return colors.text;
    case WidgetColorField::SecondaryText:
        return colors.secondary_text;
    case WidgetColorField::RemainingBar:
        return colors.remaining_bar;
    case WidgetColorField::ConsumedBar:
        return colors.consumed_bar;
    case WidgetColorField::HistoryCurve:
        return colors.history_curve;
    case WidgetColorField::ActiveControl:
        return colors.active_control;
    default:
        return colors.text;
    }
}

// ----------------------------------------------------------------------------
// Ecrit une couleur dans les reglages par champ logique.
// ----------------------------------------------------------------------------
void SetWidgetColorField(WidgetColorSettings& colors, WidgetColorField field, COLORREF color) {
    switch (field) {
    case WidgetColorField::Background:
        colors.background = color;
        break;
    case WidgetColorField::Border:
        colors.border = color;
        break;
    case WidgetColorField::Text:
        colors.text = color;
        break;
    case WidgetColorField::SecondaryText:
        colors.secondary_text = color;
        break;
    case WidgetColorField::RemainingBar:
        colors.remaining_bar = color;
        break;
    case WidgetColorField::ConsumedBar:
        colors.consumed_bar = color;
        break;
    case WidgetColorField::HistoryCurve:
        colors.history_curve = color;
        break;
    case WidgetColorField::ActiveControl:
        colors.active_control = color;
        break;
    }
}

// ----------------------------------------------------------------------------
// Genere un theme aleatoire lisible pour le widget.
// ----------------------------------------------------------------------------
WidgetColorSettings GenerateRandomWidgetColors() {
    std::random_device random_device;
    std::mt19937 generator(random_device());

    const double accent_hue = RandomDouble(generator, 0.0, 1.0);
    const double second_hue = accent_hue + RandomDouble(generator, 0.08, 0.18);
    const bool dark_theme = RandomDouble(generator, 0.0, 1.0) < 0.72;

    WidgetColorSettings colors{};
    if (dark_theme) {
        colors.background = HslToColorRef(RandomDouble(generator, 0.0, 1.0), 0.12, RandomDouble(generator, 0.10, 0.18));
        colors.text = HslToColorRef(accent_hue, 0.12, RandomDouble(generator, 0.88, 0.94));
        colors.secondary_text = MixColors(
            colors.text,
            colors.background,
            RandomDouble(generator, 0.04, 0.16)
        );
        colors.border = MixColors(colors.background, colors.text, 0.18);
        colors.remaining_bar = HslToColorRef(accent_hue, RandomDouble(generator, 0.64, 0.82), RandomDouble(generator, 0.48, 0.58));
        colors.active_control = colors.remaining_bar;
        colors.history_curve = HslToColorRef(second_hue, RandomDouble(generator, 0.58, 0.78), RandomDouble(generator, 0.50, 0.62));
        colors.consumed_bar = MixColors(colors.background, colors.text, 0.74);
    } else {
        colors.background = HslToColorRef(RandomDouble(generator, 0.0, 1.0), 0.16, RandomDouble(generator, 0.88, 0.95));
        colors.text = HslToColorRef(accent_hue, 0.18, RandomDouble(generator, 0.10, 0.18));
        colors.secondary_text = MixColors(
            colors.text,
            colors.background,
            RandomDouble(generator, 0.04, 0.16)
        );
        colors.border = MixColors(colors.background, colors.text, 0.20);
        colors.remaining_bar = HslToColorRef(accent_hue, RandomDouble(generator, 0.60, 0.78), RandomDouble(generator, 0.38, 0.48));
        colors.active_control = colors.remaining_bar;
        colors.history_curve = HslToColorRef(second_hue, RandomDouble(generator, 0.56, 0.74), RandomDouble(generator, 0.34, 0.46));
        colors.consumed_bar = MixColors(colors.background, colors.text, 0.62);
    }

    return colors;
}

// ----------------------------------------------------------------------------
// Met a jour ou cree le preset personnalise avec les couleurs courantes.
// ----------------------------------------------------------------------------
void UpsertCustomColorPreset(AppSettings& settings) {
    for (WidgetColorPreset& preset : settings.color_presets) {
        if (preset.id == kCustomColorPresetId) {
            preset.name = T(IDS_COLOR_PRESET_CUSTOM);
            preset.colors = settings.colors;
            return;
        }
    }

    settings.color_presets.push_back(WidgetColorPreset{
        kCustomColorPresetId,
        T(IDS_COLOR_PRESET_CUSTOM),
        settings.colors,
    });
}

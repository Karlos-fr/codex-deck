// ============================================================================
// Codex Glass - Implementation du moteur de micro-deplacement
// ----------------------------------------------------------------------------
// Ce fichier convertit la timeline de vibration en decalage horizontal applique
// via Win32, sans modifier durablement la position stable du widget.
// ============================================================================

#include "WidgetVibrationMotion.h"
#include "../motion/WidgetMotionCurves.h"

#include <cmath>

namespace {

// ----------------------------------------------------------------------------
// Deplace la fenetre par rapport a sa position courante sans changer sa taille
// ni son ordre.
//
// Parametres :
// - hwnd : fenetre a deplacer.
// - current_rect : rectangle courant deja observe.
// - delta_x : deplacement horizontal relatif en pixels physiques.
//
// Retour :
// - true si Win32 accepte le deplacement.
// - false sinon.
// ----------------------------------------------------------------------------
bool MoveWindowBy(HWND hwnd, const RECT& current_rect, int delta_x, int delta_y = 0) {
    if (delta_x == 0 && delta_y == 0) {
        return true;
    }

    return SetWindowPos(
        hwnd,
        nullptr,
        current_rect.left + delta_x,
        current_rect.top + delta_y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    ) != FALSE;
}

}  // namespace

// ----------------------------------------------------------------------------
// Capture le rectangle stable avec les axes et la frequence configurables.
// ----------------------------------------------------------------------------
void WidgetVibrationMotion::Start(const RECT& window_rect, const WidgetMotionVibrationSettings& settings) {
    WidgetMotionEffectsSettings container{};
    container.vibration = settings;
    const WidgetMotionVibrationSettings normalized = NormalizeWidgetMotionEffectsSettings(container).vibration;
    stable_rect_ = window_rect;
    intensity_scale_ = static_cast<double>(normalized.intensity_percent) / 100.0;
    frequency_scale_ = static_cast<double>(normalized.frequency_percent) / 100.0;
    current_offset_x_ = 0;
    current_offset_y_ = 0;
    horizontal_enabled_ = normalized.horizontal_enabled;
    vertical_enabled_ = normalized.vertical_enabled;
    started_ = normalized.enabled;
}

// ----------------------------------------------------------------------------
// Applique l'offset correspondant a la progression courante.
//
// Parametres :
// - hwnd : fenetre a deplacer.
// - progress : phase d'oscillation, non bornee pour garder une vitesse stable.
// - intensity : intensite amortie entre 0 et 1.
//
// Retour :
// - true si le deplacement a ete applique ou n'etait pas necessaire.
// - false sinon.
// ----------------------------------------------------------------------------
bool WidgetVibrationMotion::Apply(HWND hwnd, double progress, double intensity) {
    if (!started_ || hwnd == nullptr) {
        return false;
    }

    WidgetMotionVibrationSettings frame_settings{};
    frame_settings.intensity_percent = static_cast<int>(std::lround(intensity_scale_ * 100.0));
    frame_settings.frequency_percent = static_cast<int>(std::lround(frequency_scale_ * 100.0));
    frame_settings.horizontal_enabled = horizontal_enabled_;
    frame_settings.vertical_enabled = vertical_enabled_;
    const WidgetMotionOffset offset = EvaluateWidgetMotionOffset(progress, intensity, frame_settings);
    const int next_offset_x = offset.x;
    const int next_offset_y = offset.y;
    if (next_offset_x == current_offset_x_ && next_offset_y == current_offset_y_) {
        return true;
    }

    RECT current_rect{};
    if (!GetWindowRect(hwnd, &current_rect)) {
        return false;
    }

    stable_rect_ = current_rect;
    OffsetRect(&stable_rect_, -current_offset_x_, -current_offset_y_);

    const int delta_x = next_offset_x - current_offset_x_;
    const int delta_y = next_offset_y - current_offset_y_;
    if (!MoveWindowBy(hwnd, current_rect, delta_x, delta_y)) {
        return false;
    }

    current_offset_x_ = next_offset_x;
    current_offset_y_ = next_offset_y;
    return true;
}

// ----------------------------------------------------------------------------
// Replace la fenetre sur son rectangle stable.
//
// Parametres :
// - hwnd : fenetre a replacer.
//
// Retour :
// - true si le retour a ete applique ou n'etait pas necessaire.
// - false sinon.
// ----------------------------------------------------------------------------
bool WidgetVibrationMotion::Finish(HWND hwnd) {
    if (!started_) {
        return true;
    }

    if (hwnd != nullptr && (current_offset_x_ != 0 || current_offset_y_ != 0)) {
        RECT current_rect{};
        if (!GetWindowRect(hwnd, &current_rect)
            || !MoveWindowBy(hwnd, current_rect, -current_offset_x_, -current_offset_y_)) {
            return false;
        }
    }

    current_offset_x_ = 0;
    current_offset_y_ = 0;
    intensity_scale_ = 1.0;
    frequency_scale_ = 1.0;
    started_ = false;
    return true;
}

// ----------------------------------------------------------------------------
// Indique si un offset temporaire est actuellement applique.
//
// Retour :
// - true si la fenetre est decalee par le moteur.
// - false sinon.
// ----------------------------------------------------------------------------
bool WidgetVibrationMotion::HasTemporaryOffset() const {
    return started_ && (current_offset_x_ != 0 || current_offset_y_ != 0);
}

// ----------------------------------------------------------------------------
// Retourne le rectangle stable deduit de la position courante.
//
// Parametres :
// - hwnd : fenetre dont la position courante doit etre observee.
//
// Retour :
// - rectangle stable sans offset temporaire.
// ----------------------------------------------------------------------------
RECT WidgetVibrationMotion::StableRect(HWND hwnd) const {
    RECT current_rect{};
    if (started_ && hwnd != nullptr && GetWindowRect(hwnd, &current_rect)) {
        OffsetRect(&current_rect, -current_offset_x_, -current_offset_y_);
        return current_rect;
    }

    return stable_rect_;
}

// ============================================================================
// Codex Glass - Implementation de l'animation des chiffres
// ----------------------------------------------------------------------------
// Ce fichier calcule une progression courte pour remplacer verticalement les
// chiffres modifies d'un indicateur de consommation.
// ============================================================================

#include "WidgetRollingNumberAnimation.h"

#include <algorithm>
#include <cwchar>

namespace {

// Duree de l'animation de remplacement des chiffres.
constexpr auto kRollingNumberDuration = std::chrono::milliseconds{420};

// ----------------------------------------------------------------------------
// Convertit une duree en ratio borne.
// ----------------------------------------------------------------------------
double ProgressFromElapsed(WidgetRollingNumberAnimation::TimePoint::duration elapsed) {
    const double elapsed_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    const double duration_ms = std::chrono::duration<double, std::milli>(kRollingNumberDuration).count();
    return std::clamp(elapsed_ms / duration_ms, 0.0, 1.0);
}

// ----------------------------------------------------------------------------
// Adoucit la progression pour eviter un mouvement mecanique.
// ----------------------------------------------------------------------------
double EaseOutCubic(double value) {
    const double inverted = 1.0 - std::clamp(value, 0.0, 1.0);
    return 1.0 - (inverted * inverted * inverted);
}

// ----------------------------------------------------------------------------
// Copie un texte court dans un buffer fixe termine par zero.
// ----------------------------------------------------------------------------
size_t CopyText(std::array<wchar_t, 32>& target, const std::wstring& text) {
    const size_t copied_length = std::min(text.size(), target.size() - 1);
    std::wmemset(target.data(), 0, target.size());
    if (copied_length > 0) {
        std::wmemcpy(target.data(), text.data(), copied_length);
    }
    return copied_length;
}

// ----------------------------------------------------------------------------
// Convertit le buffer fixe en chaine pour le renderer.
// ----------------------------------------------------------------------------
std::wstring TextFromBuffer(const std::array<wchar_t, 32>& text, size_t length) {
    return std::wstring(text.data(), std::min(length, text.size() - 1));
}

}  // namespace

// ----------------------------------------------------------------------------
// Initialise la valeur sans declencher d'animation.
// ----------------------------------------------------------------------------
void WidgetRollingNumberAnimation::Reset(const std::wstring& text) {
    current_text_length_ = CopyText(current_text_, text);
    previous_text_ = current_text_;
    previous_text_length_ = current_text_length_;
    active_ = false;
    initialized_ = true;
}

// ----------------------------------------------------------------------------
// Met a jour la valeur et demarre une animation si elle change.
// ----------------------------------------------------------------------------
void WidgetRollingNumberAnimation::SetText(const std::wstring& text, TimePoint now) {
    if (!initialized_) {
        Reset(text);
        return;
    }

    const std::wstring current_text = TextFromBuffer(current_text_, current_text_length_);
    if (text == current_text) {
        return;
    }

    previous_text_ = current_text_;
    previous_text_length_ = current_text_length_;
    current_text_length_ = CopyText(current_text_, text);
    started_at_ = now;
    active_ = true;
}

// ----------------------------------------------------------------------------
// Indique si une animation est encore active.
// ----------------------------------------------------------------------------
bool WidgetRollingNumberAnimation::IsActive(TimePoint now) const {
    return active_ && now >= started_at_ && now - started_at_ < kRollingNumberDuration;
}

// ----------------------------------------------------------------------------
// Retourne l'etat de rendu courant.
// ----------------------------------------------------------------------------
WidgetRollingNumberFrame WidgetRollingNumberAnimation::Frame(TimePoint now) const {
    const std::wstring current_text = TextFromBuffer(current_text_, current_text_length_);
    if (!IsActive(now)) {
        return WidgetRollingNumberFrame{current_text, current_text, 1.0, false};
    }

    return WidgetRollingNumberFrame{
        TextFromBuffer(previous_text_, previous_text_length_),
        current_text,
        EaseOutCubic(ProgressFromElapsed(now - started_at_)),
        true
    };
}

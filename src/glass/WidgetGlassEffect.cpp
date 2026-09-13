// ============================================================================
// Codex Glass - Implementation du coordinateur GlassEffect
// ----------------------------------------------------------------------------
// Ce fichier coordonne le socle GlassEffect. La capture est declenchee hors WM_PAINT
// et le rendu capture est expose au renderer principal.
// ============================================================================

#include "WidgetGlassEffect.h"

// ----------------------------------------------------------------------------
// Initialise paresseusement le runtime GlassEffect.
//
// Retour :
// - true si le runtime est pret.
// - false si le socle D3D11 n'est pas disponible.
// ----------------------------------------------------------------------------
bool WidgetGlassEffect::EnsureInitialized(HWND hwnd) {
    if (!renderer_.Initialize()) {
        return false;
    }

    return capture_.Initialize(hwnd, renderer_.Device(), renderer_.Context());
}

// ----------------------------------------------------------------------------
// Capture le fond hors du chemin de peinture principal.
// ----------------------------------------------------------------------------
bool WidgetGlassEffect::TickCapture() {
    return capture_.CaptureNextFrame();
}

// ----------------------------------------------------------------------------
// Associe une fenetre compagne a la capture DXGI principale.
//
// Parametres :
// - hwnd : fenetre compagne, ou nullptr pour la retirer.
// ----------------------------------------------------------------------------
void WidgetGlassEffect::SetCompanionWindow(HWND hwnd) {
    capture_.SetCompanionWindow(hwnd);
}

// ----------------------------------------------------------------------------
// Retourne la derniere frame capturee disponible pour le rendu.
// ----------------------------------------------------------------------------
const WidgetGlassEffectFrame* WidgetGlassEffect::LatestFrame() const {
    return capture_.LatestFrame();
}

// ----------------------------------------------------------------------------
// Retourne la frame capturee pour la fenetre compagne.
// ----------------------------------------------------------------------------
const WidgetGlassEffectFrame* WidgetGlassEffect::LatestCompanionFrame() const {
    return capture_.LatestCompanionFrame();
}

// ----------------------------------------------------------------------------
// Libere les ressources GlassEffect.
// ----------------------------------------------------------------------------
void WidgetGlassEffect::Shutdown() {
    capture_.Shutdown();
    renderer_.Shutdown();
}

// ----------------------------------------------------------------------------
// Indique si le runtime GlassEffect est initialise.
//
// Retour :
// - true si EnsureInitialized a reussi.
// ----------------------------------------------------------------------------
bool WidgetGlassEffect::IsInitialized() const {
    return renderer_.IsInitialized();
}

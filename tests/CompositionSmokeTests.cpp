// ============================================================================
// Codex Deck - Smoke test DirectComposition
// ----------------------------------------------------------------------------
// Ce fichier valide l'initialisation minimale du pipeline graphique natif sur
// une fenetre Win32 cachee, sans afficher l'application.
// ============================================================================

#include "graphics/CompositionHost.h"

#include <windows.h>

namespace {

// Nom de classe Win32 reserve au smoke test de composition.
constexpr wchar_t kCompositionSmokeWindowClass[] = L"CodexDeckCompositionSmokeWindow";

// Code de sortie utilise lorsque la classe Win32 ne peut pas etre enregistree.
constexpr int kRegisterFailure = 1;

// Code de sortie utilise lorsque la fenetre cachee ne peut pas etre creee.
constexpr int kWindowFailure = 2;

// Code de sortie utilise lorsque l'initialisation de composition echoue.
constexpr int kInitializeFailure = 3;

// Code de sortie utilise lorsque le dessin ne peut pas commencer.
constexpr int kBeginDrawFailure = 4;

// Code de sortie utilise lorsque le dessin ne peut pas etre termine.
constexpr int kEndDrawFailure = 5;

// ----------------------------------------------------------------------------
// Procedure minimale de la fenetre de test.
// ----------------------------------------------------------------------------
LRESULT CALLBACK SmokeWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

// ----------------------------------------------------------------------------
// Execute un cycle minimal Initialize, BeginDraw, Clear, EndDraw.
//
// Retour :
// - zero si DirectComposition accepte le dessin cache, sinon le code fautif.
// ----------------------------------------------------------------------------
int main() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = SmokeWindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kCompositionSmokeWindowClass;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return kRegisterFailure;
    }

    HWND hwnd = CreateWindowExW(
        0,
        kCompositionSmokeWindowClass,
        L"Composition smoke",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320,
        240,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
    if (hwnd == nullptr) {
        return kWindowFailure;
    }

    CompositionHost host;
    if (!host.Initialize(hwnd).has_value()) {
        DestroyWindow(hwnd);
        return kInitializeFailure;
    }
    host.Resize(320, 240, 96.0F);
    auto context = host.BeginDraw();
    if (!context.has_value()) {
        DestroyWindow(hwnd);
        return kBeginDrawFailure;
    }
    (*context)->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 1.0F));
    if (!host.EndDraw().has_value()) {
        DestroyWindow(hwnd);
        return kEndDrawFailure;
    }

    DestroyWindow(hwnd);
    return 0;
}

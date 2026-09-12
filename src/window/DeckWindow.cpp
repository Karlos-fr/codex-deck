// ============================================================================
// Codex Deck - Implementation de la fenetre principale Win32
// ----------------------------------------------------------------------------
// Ce fichier cree une fenetre desktop classique, centree et redimensionnable. Il
// laisse le cycle de vie et le rendu a DeckApp et DeckRenderer.
// ============================================================================

#include "DeckWindow.h"

#include "../resources/ResourceIds.h"

#include <dwmapi.h>

#include <algorithm>

namespace {

// Nom de classe Win32 reserve a la fenetre principale Codex Deck.
constexpr wchar_t kDeckWindowClassName[] = L"CodexDeckMainWindow";

// Titre initial affiche par Windows avant le premier rendu Direct2D.
constexpr wchar_t kDeckWindowTitle[] = L"Codex Deck";

// Largeur minimale du client principal.
constexpr LONG kDeckMinimumClientWidth = 960;

// Hauteur minimale du client principal.
constexpr LONG kDeckMinimumClientHeight = 640;

// Largeur initiale du client principal.
constexpr LONG kDeckInitialClientWidth = 1180;

// Hauteur initiale du client principal.
constexpr LONG kDeckInitialClientHeight = 760;

// ----------------------------------------------------------------------------
// Calcule le rectangle de fenetre centre sur le moniteur principal.
//
// Retour :
// - rectangle Win32 en coordonnees ecran.
// ----------------------------------------------------------------------------
RECT BuildCenteredWindowRect() {
    RECT rect{0, 0, kDeckInitialClientWidth, kDeckInitialClientHeight};
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);
    rect.left = std::max(0, (screen_width - width) / 2);
    rect.top = std::max(0, (screen_height - height) / 2);
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    return rect;
}

}  // namespace

// ----------------------------------------------------------------------------
// Retourne la taille minimale du client principal.
//
// Retour :
// - dimensions minimales en pixels logiques Win32.
// ----------------------------------------------------------------------------
SIZE DeckMinimumClientSize() {
    return SIZE{kDeckMinimumClientWidth, kDeckMinimumClientHeight};
}

// ----------------------------------------------------------------------------
// Enregistre la classe Win32 de la fenetre principale.
//
// Parametres :
// - instance : instance du module executable.
// - window_proc : procedure de fenetre fournie par l'orchestrateur.
//
// Retour :
// - true si la classe est disponible ou deja enregistree.
// ----------------------------------------------------------------------------
bool RegisterDeckWindowClass(HINSTANCE instance, WNDPROC window_proc) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    window_class.hIconSm = window_class.hIcon;
    window_class.lpszClassName = kDeckWindowClassName;

    if (RegisterClassExW(&window_class) != 0) {
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

// ----------------------------------------------------------------------------
// Cree la fenetre principale redimensionnable.
//
// Parametres :
// - instance : instance du module executable.
// - window_proc : procedure de fenetre associee a la classe.
// - create_parameter : pointeur transmis a WM_NCCREATE.
//
// Retour :
// - handle de fenetre cree, ou nullptr en cas d'erreur Win32.
// ----------------------------------------------------------------------------
HWND CreateDeckMainWindow(HINSTANCE instance, WNDPROC window_proc, void* create_parameter) {
    if (!RegisterDeckWindowClass(instance, window_proc)) {
        return nullptr;
    }

    const RECT rect = BuildCenteredWindowRect();
    return CreateWindowExW(
        0,
        kDeckWindowClassName,
        kDeckWindowTitle,
        WS_OVERLAPPEDWINDOW,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        create_parameter
    );
}

// ----------------------------------------------------------------------------
// Applique les attributs systeme correspondant au theme demande.
//
// Parametres :
// - hwnd : fenetre a mettre a jour.
// - dark : true pour demander le mode sombre systeme.
// ----------------------------------------------------------------------------
void ApplyDeckWindowTheme(HWND hwnd, bool dark) {
    const BOOL enabled = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof(enabled));
}

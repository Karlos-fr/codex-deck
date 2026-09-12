// ============================================================================
// Codex Glass - Implementation de l'icone applicative partagee
// ----------------------------------------------------------------------------
// Ce fichier centralise le chargement de l'icone embarquee afin d'eviter que
// les modules fenetre et shell dupliquent l'acces aux ressources Windows.
// ============================================================================

#include "AppIcon.h"

#include "../resources/ResourceIds.h"

namespace {

// ----------------------------------------------------------------------------
// Charge une icone embarquee avec les dimensions demandees.
//
// Parametres :
// - resource_id : identifiant Win32 de la ressource ICON.
// - width : largeur demandee en pixels, ou 0 pour la taille par defaut.
// - height : hauteur demandee en pixels, ou 0 pour la taille par defaut.
//
// Retour :
// - handle partage de l'icone, ou nullptr en cas d'echec.
// ----------------------------------------------------------------------------
HICON LoadEmbeddedIcon(UINT resource_id, int width, int height) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const UINT flags = LR_SHARED | LR_DEFAULTCOLOR | ((width == 0 || height == 0) ? LR_DEFAULTSIZE : 0);
    return reinterpret_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(resource_id),
        IMAGE_ICON,
        width,
        height,
        flags
    ));
}

} // namespace

// ----------------------------------------------------------------------------
// Charge l'icone applicative embarquee avec une taille adaptee au contexte.
//
// Parametres :
// - width : largeur demandee en pixels, ou 0 pour la taille par defaut.
// - height : hauteur demandee en pixels, ou 0 pour la taille par defaut.
//
// Retour :
// - handle partage de l'icone applicative, ou icone systeme par defaut en repli.
// ----------------------------------------------------------------------------
HICON LoadApplicationIcon(int width, int height) {
    HICON icon = LoadEmbeddedIcon(IDI_APP_ICON, width, height);
    if (icon != nullptr) {
        return icon;
    }

    return LoadIconW(nullptr, IDI_APPLICATION);
}

// ----------------------------------------------------------------------------
// Charge l'icone simplifiee destinee a la zone de notification.
//
// Parametres :
// - width : largeur demandee en pixels, ou 0 pour la taille par defaut.
// - height : hauteur demandee en pixels, ou 0 pour la taille par defaut.
//
// Retour :
// - handle partage de l'icone tray, ou icone principale en repli.
// ----------------------------------------------------------------------------
HICON LoadTrayIcon(int width, int height) {
    HICON icon = LoadEmbeddedIcon(IDI_TRAY_ICON, width, height);
    return icon != nullptr ? icon : LoadApplicationIcon(width, height);
}

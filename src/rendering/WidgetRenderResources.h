// ============================================================================
// Codex Glass - Ressources natives du rendu
// ----------------------------------------------------------------------------
// Ce fichier declare le conteneur des factories, formats, brosses et render
// target Direct2D/DirectWrite. Il ne dessine pas le widget.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <windows.h>

// ----------------------------------------------------------------------------
// Possede les ressources Direct2D et DirectWrite reutilisees par le renderer.
// ----------------------------------------------------------------------------
class WidgetRenderResources {
public:
    // ------------------------------------------------------------------------
    // Initialise les factories Direct2D et DirectWrite independantes de la
    // fenetre.
    //
    // Retour :
    // - true si les factories sont disponibles.
    // - false si l'initialisation a echoue.
    // ------------------------------------------------------------------------
    bool Initialize();

    // ------------------------------------------------------------------------
    // Libere les ressources Direct2D dependant de la fenetre.
    // ------------------------------------------------------------------------
    void DiscardDeviceResources();

    // ------------------------------------------------------------------------
    // Cree les ressources Direct2D dependant du render target de la fenetre.
    //
    // Parametres :
    // - hwnd : handle de la fenetre a laquelle associer le render target.
    // - settings : reglages visuels courants.
    //
    // Retour :
    // - true si toutes les ressources sont disponibles.
    // - false si Direct2D a signale une erreur.
    // ------------------------------------------------------------------------
    bool CreateDeviceResources(HWND hwnd, const AppSettings& settings);

    // ------------------------------------------------------------------------
    // Redimensionne le render target Direct2D lorsque la fenetre change.
    //
    // Parametres :
    // - hwnd : handle de la fenetre redimensionnee.
    // ------------------------------------------------------------------------
    void Resize(HWND hwnd);

protected:
    // Factory Direct2D partagee par les ressources de rendu.
    Microsoft::WRL::ComPtr<ID2D1Factory> d2d_factory;

    // Factory DirectWrite partagee par les formats de texte.
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;

    // Render target Direct2D associe a la fenetre principale.
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target;

    // Brosse utilisee pour le fond global de la fenetre.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> window_background_brush;

    // Brosse utilisee pour le fond arrondi du panneau principal.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> panel_background_brush;

    // Brosse utilisee pour le contour du panneau et du graphe.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border_brush;

    // Brosse de fond legerement eclaircie des cartes KPI minimales.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> minimal_card_background_brush;

    // Brosse sombre translucide de l'ombre des cartes KPI minimales.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> minimal_card_shadow_brush;

    // Brosse utilisee pour le titre du widget.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> title_text_brush;

    // Brosse utilisee pour les textes principaux.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> body_text_brush;

    // Brosse utilisee pour les textes secondaires.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> muted_text_brush;

    // Brosse utilisee pour le fond des barres de progression.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> progress_background_brush;

    // Brosse d'accent restante utilisee par la barre 5 h et le mode minimal.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> five_hour_remaining_accent_brush;

    // Brosse d'accent restante utilisee par la barre hebdomadaire et le mode minimal.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> weekly_remaining_accent_brush;

    // Brosse utilisee pour la courbe du graphe historique.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> history_curve_brush;

    // Format DirectWrite utilise pour le titre.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_text_format;

    // Format DirectWrite utilise pour le titre centre du mode minimal.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> minimal_title_text_format;

    // Format DirectWrite utilise pour les libelles principaux.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> body_text_format;

    // Format DirectWrite utilise pour les textes secondaires.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> caption_text_format;

    // Format DirectWrite utilise pour le resume provider aligne a droite.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> provider_summary_text_format;

    // Format DirectWrite utilise pour les valeurs de quotas alignees a droite.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> usage_value_text_format;

    // Format DirectWrite utilise pour le statut aligne a droite.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> status_text_format;

    // Format DirectWrite utilise pour le statut anime fixe a gauche.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> refreshing_status_text_format;

    // Format DirectWrite utilise pour les libelles du panneau couleurs.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> color_panel_label_text_format;

    // Format DirectWrite utilise pour les boutons du panneau couleurs.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> color_panel_button_text_format;

    // Format DirectWrite utilise pour les libelles du mode minimal.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> minimal_label_text_format;

    // Format DirectWrite utilise pour les valeurs du mode minimal.
    Microsoft::WRL::ComPtr<IDWriteTextFormat> minimal_value_text_format;

private:
    // ------------------------------------------------------------------------
    // Cree les formats de texte DirectWrite reutilises par le rendu.
    //
    // Retour :
    // - true si tous les formats ont ete crees.
    // - false si DirectWrite a signale une erreur.
    // ------------------------------------------------------------------------
    bool CreateTextFormats();
};

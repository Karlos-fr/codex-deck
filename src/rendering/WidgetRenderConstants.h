// ============================================================================
// Codex Glass - Constantes partagees du rendu
// ----------------------------------------------------------------------------
// Ce fichier centralise les dimensions et seuils Direct2D utilises par plusieurs
// modules de rendu. Les constantes strictement locales restent dans leur module.
// ============================================================================

#pragma once

#include <windows.h>

// DPI de reference utilise par les dimensions Direct2D en DIPs.
constexpr float kReferenceDpi = 96.0F;

// Marge exterieure du panneau principal en DIPs.
constexpr float kPanelMargin = 0.0F;

// Marge interieure du panneau principal en DIPs.
constexpr float kPanelPadding = 18.0F;

// Hauteur d'une barre de progression en DIPs.
constexpr float kProgressBarHeight = 8.0F;

// Rayon des coins arrondis des barres de progression en DIPs.
constexpr float kProgressCornerRadius = 4.0F;

// Espacement vertical entre les blocs d'usage en DIPs.
constexpr float kRowSpacing = 58.0F;

// Hauteur occupee par un bloc d'usage complet en DIPs.
constexpr float kUsageRowHeight = 54.0F;

// Marge verticale entre le bloc semaine et le graphe en DIPs.
constexpr float kGraphTopSpacing = 14.0F;

// Taille du texte de titre en DIPs.
constexpr float kTitleTextSize = 17.0F;

// Taille du texte principal en DIPs.
constexpr float kBodyTextSize = 13.0F;

// Taille du texte secondaire en DIPs.
constexpr float kCaptionTextSize = 11.0F;

// Marge horizontale commune aux hints compacts en DIPs.
constexpr float kWidgetHintHorizontalPadding = 9.0F;

// Taille du texte discret des statuts en DIPs.
constexpr float kStatusTextSize = 9.0F;

// Taille du texte compact du panneau couleurs en DIPs.
constexpr float kColorPanelTextSize = 10.0F;

// Rayon des carres couleur du panneau en DIPs.
constexpr float kColorPanelSwatchRadius = 4.0F;

// Marge interieure de reference des modes orientes en DIPs.
constexpr float kMinimalPanelPadding = 14.0F;

// Taille renforcee des libelles des cartes KPI en DIPs.
constexpr float kMinimalLabelTextSize = 13.0F;

// Taille renforcee des valeurs des cartes KPI en DIPs.
constexpr float kMinimalValueTextSize = 19.0F;

// Largeur reservee au statut de rafraichissement en DIPs.
constexpr float kFreshnessStatusWidth = 190.0F;

// Opacite minimale acceptee pour le fond du widget.
constexpr double kMinimumBackgroundOpacity = 0.20;

// Opacite maximale acceptee pour le fond du widget.
constexpr double kMaximumBackgroundOpacity = 1.00;

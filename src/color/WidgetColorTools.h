// ============================================================================
// Codex Glass - Outils de personnalisation des couleurs
// ----------------------------------------------------------------------------
// Ce fichier declare les helpers independants de l'interface pour choisir,
// appliquer et generer les couleurs personnalisables du widget.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"

#include <windows.h>

// ----------------------------------------------------------------------------
// Liste les champs couleur personnalisables du widget.
// ----------------------------------------------------------------------------
enum class WidgetColorField {
    Background,
    Border,
    Text,
    SecondaryText,
    RemainingBar,
    ConsumedBar,
    HistoryCurve,
    ActiveControl,
};

// ----------------------------------------------------------------------------
// Affiche le selecteur de couleur Windows natif.
//
// Parametres :
// - hwnd : handle de la fenetre proprietaire.
// - color : couleur initiale puis couleur choisie.
//
// Retour :
// - true si l'utilisateur a choisi une couleur.
// - false si la selection a ete annulee.
// ----------------------------------------------------------------------------
bool ChooseWidgetColor(HWND hwnd, COLORREF& color);

// ----------------------------------------------------------------------------
// Lit une couleur depuis les reglages par champ logique.
//
// Parametres :
// - colors : couleurs source.
// - field : champ a lire.
//
// Retour :
// - couleur du champ demande.
// ----------------------------------------------------------------------------
COLORREF GetWidgetColorField(const WidgetColorSettings& colors, WidgetColorField field);

// ----------------------------------------------------------------------------
// Ecrit une couleur dans les reglages par champ logique.
//
// Parametres :
// - colors : couleurs a modifier.
// - field : champ a ecrire.
// - color : nouvelle couleur.
// ----------------------------------------------------------------------------
void SetWidgetColorField(WidgetColorSettings& colors, WidgetColorField field, COLORREF color);

// ----------------------------------------------------------------------------
// Genere un theme aleatoire lisible pour le widget.
//
// Retour :
// - couleurs coherentes et contrastees.
// ----------------------------------------------------------------------------
WidgetColorSettings GenerateRandomWidgetColors();

// ----------------------------------------------------------------------------
// Met a jour ou cree le preset personnalise avec les couleurs courantes.
//
// Parametres :
// - settings : reglages contenant les couleurs courantes.
// ----------------------------------------------------------------------------
void UpsertCustomColorPreset(AppSettings& settings);

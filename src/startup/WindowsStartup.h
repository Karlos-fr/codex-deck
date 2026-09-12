// ============================================================================
// Codex Glass - Declaration du lancement avec Windows
// ----------------------------------------------------------------------------
// Ce fichier expose la gestion de l'entree utilisateur Windows Run. Il ne
// decide pas de la preference et ne manipule pas le fichier de configuration.
// ============================================================================

#pragma once

// ----------------------------------------------------------------------------
// Active ou desactive le lancement de Codex Glass avec la session Windows.
//
// Parametres :
// - enabled : true pour enregistrer l'executable courant, false pour le retirer.
//
// Retour :
// - true si l'etat demande a ete applique ou etait deja absent.
// ----------------------------------------------------------------------------
bool SetStartWithWindowsEnabled(bool enabled);

// ----------------------------------------------------------------------------
// Indique si la commande enregistree pointe vers l'executable courant.
//
// Retour :
// - true si la valeur utilisateur Run contient la commande attendue.
// ----------------------------------------------------------------------------
bool IsStartWithWindowsEnabled();

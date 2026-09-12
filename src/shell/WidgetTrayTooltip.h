// ============================================================================
// Codex Glass - Formatage de l'infobulle de l'icone tray
// ----------------------------------------------------------------------------
// Ce fichier declare la presentation compacte des quotas visibles. Il ne gere
// ni l'installation de l'icone Windows ni le rafraichissement des donnees.
// ============================================================================

#pragma once

#include "../settings/AppSettings.h"
#include "../usage/UsageSnapshot.h"

#include <string>

// ----------------------------------------------------------------------------
// Construit l'infobulle compacte de l'icone tray.
//
// Parametres :
// - snapshot : dernier releve de quotas disponible.
// - visibility : choix des lignes affichees dans le widget.
//
// Retour : titre de l'application suivi des pourcentages restants visibles.
// ----------------------------------------------------------------------------
std::wstring BuildWidgetTrayTooltip(
    const UsageSnapshot& snapshot,
    const WidgetQuotaVisibilitySettings& visibility
);

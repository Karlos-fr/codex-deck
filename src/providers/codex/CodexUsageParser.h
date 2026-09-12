// ============================================================================
// Codex Glass - Parsing des donnees d'usage Codex
// ----------------------------------------------------------------------------
// Ce fichier declare la conversion d'une reponse JSON brute vers les modeles
// generiques d'usage. Il ne lit aucun fichier et ne realise aucun appel reseau.
// ============================================================================

#pragma once

#include "../../usage/UsageSnapshot.h"

#include <optional>
#include <string>

// ----------------------------------------------------------------------------
// Convertit les reponses JSON Codex en snapshots generiques.
// ----------------------------------------------------------------------------
class CodexUsageParser {
public:
    // ------------------------------------------------------------------------
    // Parse une reponse complete de l'endpoint d'usage.
    //
    // Parametres :
    // - json_text : reponse JSON UTF-8.
    // - error_message : erreur de format renseignee en cas d'echec.
    //
    // Retour :
    // - snapshot utilisable si une limite principale est valide.
    // ------------------------------------------------------------------------
    static std::optional<UsageSnapshot> Parse(
        const std::string& json_text,
        std::wstring* error_message
    );
};

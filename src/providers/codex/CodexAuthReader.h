// ============================================================================
// Codex Glass - Lecture de l'authentification Codex
// ----------------------------------------------------------------------------
// Ce fichier declare la lecture locale de auth.json. Il ne realise aucun appel
// reseau et ne journalise ni ne persiste les secrets lus.
// ============================================================================

#pragma once

#include <optional>
#include <string>

// ----------------------------------------------------------------------------
// Regroupe les secrets necessaires au transport Codex.
// ----------------------------------------------------------------------------
struct CodexAuthCredentials {
    // Jeton OAuth transmis uniquement au client HTTP.
    std::string access_token;

    // Identifiant de compte optionnel utilise pour le scope ChatGPT.
    std::string account_id;
};

// ----------------------------------------------------------------------------
// Lit les identifiants Codex depuis le profil local.
// ----------------------------------------------------------------------------
class CodexAuthReader {
public:
    // ------------------------------------------------------------------------
    // Resolut le chemin du fichier auth.json actif.
    //
    // Retour :
    // - chemin base sur CODEX_HOME ou USERPROFILE.
    // ------------------------------------------------------------------------
    static std::wstring ResolveAuthJsonPath();

    // ------------------------------------------------------------------------
    // Lit les identifiants necessaires a l'appel d'usage.
    //
    // Parametres :
    // - error_message : message renseigne en cas d'echec.
    //
    // Retour :
    // - identifiants exploitables, ou rien.
    // ------------------------------------------------------------------------
    static std::optional<CodexAuthCredentials> Read(std::wstring* error_message);
};

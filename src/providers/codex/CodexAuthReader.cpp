// ============================================================================
// Codex Glass - Lecture de l'authentification Codex
// ----------------------------------------------------------------------------
// Ce fichier charge et parse auth.json de facon structuree. Les secrets restent
// en memoire uniquement le temps necessaire a la requete distante.
// ============================================================================

#include "CodexAuthReader.h"

#include <windows.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {

// ----------------------------------------------------------------------------
// Assemble deux fragments de chemin Windows.
// ----------------------------------------------------------------------------
std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    std::wstring result = base;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/') {
        result.push_back(L'\\');
    }
    result += child;
    return result;
}

// ----------------------------------------------------------------------------
// Lit une variable d'environnement Windows.
// ----------------------------------------------------------------------------
std::optional<std::wstring> ReadEnvironmentVariable(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) {
        return std::nullopt;
    }
    std::wstring value(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), size);
    if (written == 0 || written >= size) {
        return std::nullopt;
    }
    value.resize(written);
    return value;
}

// ----------------------------------------------------------------------------
// Lit un fichier UTF-8 complet.
// ----------------------------------------------------------------------------
std::optional<std::string> LoadFileUtf8(const std::wstring& path, std::wstring* error_message) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error_message != nullptr) {
            *error_message = L"auth.json Codex introuvable";
        }
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// ----------------------------------------------------------------------------
// Lit une chaine optionnelle dans un objet JSON.
// ----------------------------------------------------------------------------
std::optional<std::string> ReadString(const nlohmann::json& object, const char* key) {
    const auto iterator = object.find(key);
    if (iterator == object.end() || !iterator->is_string()) {
        return std::nullopt;
    }
    return iterator->get<std::string>();
}

} // namespace

// ----------------------------------------------------------------------------
// Resolut le chemin du fichier auth.json actif.
// ----------------------------------------------------------------------------
std::wstring CodexAuthReader::ResolveAuthJsonPath() {
    if (const auto home = ReadEnvironmentVariable(L"CODEX_HOME"); home.has_value() && !home->empty()) {
        return JoinPath(*home, L"auth.json");
    }
    if (const auto profile = ReadEnvironmentVariable(L"USERPROFILE"); profile.has_value() && !profile->empty()) {
        return JoinPath(JoinPath(*profile, L".codex"), L"auth.json");
    }
    return L".codex\\auth.json";
}

// ----------------------------------------------------------------------------
// Lit les identifiants necessaires a l'appel d'usage.
// ----------------------------------------------------------------------------
std::optional<CodexAuthCredentials> CodexAuthReader::Read(std::wstring* error_message) {
    const auto text = LoadFileUtf8(ResolveAuthJsonPath(), error_message);
    if (!text.has_value()) {
        return std::nullopt;
    }

    const nlohmann::json root = nlohmann::json::parse(*text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        if (error_message != nullptr) {
            *error_message = L"auth.json Codex contient un JSON invalide";
        }
        return std::nullopt;
    }

    const nlohmann::json* tokens = &root;
    if (const auto iterator = root.find("tokens"); iterator != root.end() && iterator->is_object()) {
        tokens = &(*iterator);
    }

    const auto access_token = ReadString(*tokens, "access_token");
    if (!access_token.has_value() || access_token->empty()) {
        if (error_message != nullptr) {
            *error_message = L"auth.json ne contient pas tokens.access_token";
        }
        return std::nullopt;
    }

    CodexAuthCredentials credentials{};
    credentials.access_token = *access_token;
    if (const auto token_account_id = ReadString(*tokens, "account_id"); token_account_id.has_value()) {
        credentials.account_id = *token_account_id;
    } else if (const auto root_account_id = ReadString(root, "account_id"); root_account_id.has_value()) {
        credentials.account_id = *root_account_id;
    }
    return credentials;
}

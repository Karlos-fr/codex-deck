// ============================================================================
// Codex Deck - Selection native de dossier
// ----------------------------------------------------------------------------
// Ce module encapsule IFileDialog et ne conserve aucun etat applicatif.
// ============================================================================

#pragma once

#include <windows.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

// Erreur renvoyee par une integration plateforme.
struct PlatformError {
    // Code HRESULT natif.
    long code = 0;
    // Diagnostic utilisateur ou journalisable.
    std::wstring message;
};

// Ouvre le picker Windows de dossiers.
std::expected<std::optional<std::filesystem::path>, PlatformError> PickFolder(HWND owner);

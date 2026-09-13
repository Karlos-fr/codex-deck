// ============================================================================
// Codex Deck - Implementation du picker natif de dossier
// ----------------------------------------------------------------------------
// Ce fichier utilise IFileOpenDialog en mode filesystem et distingue
// l'annulation utilisateur d'une erreur COM.
// ============================================================================

#include "FolderPicker.h"

#include <shobjidl.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

// Construit une erreur plateforme depuis un HRESULT.
PlatformError ErrorFromHresult(HRESULT result, const wchar_t* message) {
    return PlatformError{result, message};
}

}  // namespace

// Ouvre le picker Windows de dossiers.
std::expected<std::optional<std::filesystem::path>, PlatformError> PickFolder(HWND owner) {
    ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.GetAddressOf()));
    if (FAILED(result)) {
        return std::unexpected(ErrorFromHresult(result, L"Impossible de creer le selecteur de dossier"));
    }
    DWORD options = 0;
    if (FAILED(result = dialog->GetOptions(&options))) {
        return std::unexpected(ErrorFromHresult(result, L"Impossible de lire les options du selecteur"));
    }
    result = dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (FAILED(result)) {
        return std::unexpected(ErrorFromHresult(result, L"Impossible de configurer le selecteur de dossier"));
    }
    result = dialog->Show(owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return std::optional<std::filesystem::path>{};
    }
    if (FAILED(result)) {
        return std::unexpected(ErrorFromHresult(result, L"Le selecteur de dossier a echoue"));
    }
    ComPtr<IShellItem> item;
    if (FAILED(result = dialog->GetResult(item.GetAddressOf()))) {
        return std::unexpected(ErrorFromHresult(result, L"Le dossier choisi est indisponible"));
    }
    PWSTR raw_path = nullptr;
    if (FAILED(result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path))) {
        return std::unexpected(ErrorFromHresult(result, L"Le chemin choisi est invalide"));
    }
    const std::filesystem::path path(raw_path);
    CoTaskMemFree(raw_path);
    return std::optional<std::filesystem::path>{path};
}

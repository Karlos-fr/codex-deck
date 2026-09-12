// ============================================================================
// Codex Glass - Client HTTP d'usage Codex
// ----------------------------------------------------------------------------
// Ce fichier encapsule WinHTTP, les headers OAuth, les statuts HTTP et la
// lecture du corps. Il ne journalise jamais les identifiants transmis.
// ============================================================================

#include "CodexUsageClient.h"

#include <windows.h>
#include <winhttp.h>

#include <vector>

namespace {

// Hote HTTPS de l'API d'usage Codex.
constexpr wchar_t kCodexUsageHost[] = L"chatgpt.com";

// Chemin de l'endpoint d'usage Codex.
constexpr wchar_t kCodexUsagePath[] = L"/backend-api/wham/usage";

// User-Agent minimal du client.
constexpr wchar_t kCodexGlassUserAgent[] = L"CodexGlass/0.1";

// Delai maximal de chaque etape WinHTTP en millisecondes.
constexpr DWORD kHttpTimeoutMs = 15000;

// ----------------------------------------------------------------------------
// Convertit une chaine UTF-8 en UTF-16.
// ----------------------------------------------------------------------------
std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring output(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), size);
    return output;
}

// ----------------------------------------------------------------------------
// Ajoute un header HTTP et renseigne une erreur lisible.
// ----------------------------------------------------------------------------
bool AddHttpHeader(HINTERNET request, const std::wstring& header, std::wstring* error_message) {
    if (WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD)) {
        return true;
    }
    if (error_message != nullptr) {
        *error_message = L"Erreur WinHTTP pendant l'ajout des headers";
    }
    return false;
}

// ----------------------------------------------------------------------------
// Lit entierement le corps d'une reponse HTTP.
// ----------------------------------------------------------------------------
std::optional<std::string> ReadHttpResponseBody(HINTERNET request, std::wstring* error_message) {
    std::string body;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            if (error_message != nullptr) {
                *error_message = L"Erreur reseau pendant la lecture";
            }
            return std::nullopt;
        }
        if (available == 0) {
            return body;
        }
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) {
            if (error_message != nullptr) {
                *error_message = L"Erreur reseau pendant la lecture";
            }
            return std::nullopt;
        }
        body.append(buffer.data(), read);
    }
}

} // namespace

// ----------------------------------------------------------------------------
// Recupere le corps JSON brut de l'endpoint d'usage.
// ----------------------------------------------------------------------------
std::optional<std::string> CodexUsageClient::Fetch(
    const CodexAuthCredentials& credentials,
    std::wstring* error_message
) {
    cancellation_requested_.store(false, std::memory_order_release);
    HINTERNET session = WinHttpOpen(
        kCodexGlassUserAgent,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (session == nullptr) {
        if (error_message != nullptr) {
            *error_message = L"Erreur WinHTTP a l'ouverture";
        }
        return std::nullopt;
    }

    HINTERNET connection = nullptr;
    HINTERNET request = nullptr;
    std::optional<std::string> response;
    do {
        connection = WinHttpConnect(session, kCodexUsageHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connection == nullptr) {
            if (error_message != nullptr) {
                *error_message = L"Erreur WinHTTP a la connexion";
            }
            break;
        }
        request = WinHttpOpenRequest(
            connection,
            L"GET",
            kCodexUsagePath,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );
        if (request == nullptr) {
            if (error_message != nullptr) {
                *error_message = L"Erreur WinHTTP a la creation de requete";
            }
            break;
        }

        active_request_.store(request, std::memory_order_release);
        WinHttpSetTimeouts(request, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs);
        const std::vector<std::wstring> headers{
            L"Authorization: Bearer " + Utf8ToWide(credentials.access_token),
            L"Accept: application/json",
            L"Cache-Control: no-cache",
            L"Pragma: no-cache",
            credentials.account_id.empty() ? L"" : L"ChatGPT-Account-Id: " + Utf8ToWide(credentials.account_id),
        };
        bool headers_ok = true;
        for (const auto& header : headers) {
            if (!header.empty() && !AddHttpHeader(request, header, error_message)) {
                headers_ok = false;
                break;
            }
        }
        if (!headers_ok || cancellation_requested_.load(std::memory_order_acquire)) {
            break;
        }
        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            if (error_message != nullptr) {
                *error_message = cancellation_requested_.load(std::memory_order_acquire)
                    ? L"Rafraichissement annule"
                    : L"Erreur reseau pendant l'envoi";
            }
            break;
        }
        if (cancellation_requested_.load(std::memory_order_acquire) || !WinHttpReceiveResponse(request, nullptr)) {
            if (error_message != nullptr) {
                *error_message = cancellation_requested_.load(std::memory_order_acquire)
                    ? L"Rafraichissement annule"
                    : L"Erreur reseau pendant la reception";
            }
            break;
        }

        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code,
            &status_size,
            WINHTTP_NO_HEADER_INDEX
        );
        if (status_code == 401 || status_code == 403) {
            if (error_message != nullptr) {
                *error_message = L"Authentification Codex refusee";
            }
            break;
        }
        if (status_code < 200 || status_code > 299) {
            if (error_message != nullptr) {
                *error_message = L"API Codex indisponible HTTP " + std::to_wstring(status_code);
            }
            break;
        }
        response = ReadHttpResponseBody(request, error_message);
    } while (false);

    HINTERNET owned_request = static_cast<HINTERNET>(active_request_.exchange(nullptr));
    if (owned_request != nullptr) {
        WinHttpCloseHandle(owned_request);
    }
    if (connection != nullptr) {
        WinHttpCloseHandle(connection);
    }
    WinHttpCloseHandle(session);
    return response;
}

// ----------------------------------------------------------------------------
// Annule la requete WinHTTP active.
// ----------------------------------------------------------------------------
void CodexUsageClient::Cancel() {
    cancellation_requested_.store(true, std::memory_order_release);
    HINTERNET request = static_cast<HINTERNET>(active_request_.exchange(nullptr));
    if (request != nullptr) {
        WinHttpCloseHandle(request);
    }
}

// ============================================================================
// Codex Deck - Implementation de l'orchestration applicative
// ----------------------------------------------------------------------------
// Ce fichier contient la boucle Win32 principale et reste limite a la
// coordination entre fenetre, messages et renderer.
// ============================================================================

#include "DeckApp.h"

#include "../projects/GitProjectProbe.h"
#include "../storage/AppDataPaths.h"
#include "../storage/SchemaMigrator.h"
#include "../storage/SqliteDatabase.h"
#include "../sync/SessionSyncService.h"
#include "../window/DeckWindow.h"

#include <windowsx.h>

#include <vector>

// ----------------------------------------------------------------------------
// Libere les ressources applicatives possedees.
// ----------------------------------------------------------------------------
DeckApp::~DeckApp() {
    StopStorageWorker();
    codex_supervisor_.Stop();
}

// ----------------------------------------------------------------------------
// Lance la boucle de messages de l'application.
// ----------------------------------------------------------------------------
int DeckApp::Run(HINSTANCE instance, int command_show) {
    HWND hwnd = CreateDeckMainWindow(instance, DeckApp::WindowProc, this);
    if (hwnd == nullptr) {
        return 1;
    }

    RefreshTheme(hwnd);
    ShowWindow(hwnd, command_show);
    UpdateWindow(hwnd);
    StartStorageWorker();
    codex_supervisor_.SetConnectedClientHandler([this](CodexClient& client) {
        RefreshSessionsFromCodex(client);
    });
    codex_supervisor_.SetResyncRequiredHandler([this](CodexClient& client) {
        RefreshSessionsFromCodex(client);
    });
    codex_supervisor_.Start();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

// ----------------------------------------------------------------------------
// Procedure Win32 statique redirigeant vers l'instance applicative.
// ----------------------------------------------------------------------------
LRESULT CALLBACK DeckApp::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        auto* app = static_cast<DeckApp*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }

    auto* app = reinterpret_cast<DeckApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app == nullptr) {
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    return app->HandleWindowMessage(hwnd, message, wparam, lparam);
}

// ----------------------------------------------------------------------------
// Traite un message Win32 pour la fenetre principale.
// ----------------------------------------------------------------------------
LRESULT DeckApp::HandleWindowMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        RefreshTheme(hwnd);
        renderer_.Initialize(hwnd);
        return 0;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            renderer_.Resize(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL:
        renderer_.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wparam));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        renderer_.OnPointerDown(
            static_cast<float>(GET_X_LPARAM(lparam)),
            static_cast<float>(GET_Y_LPARAM(lparam))
        );
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_KEYDOWN:
        if (renderer_.OnKeyDown(wparam)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_CHAR:
        if (renderer_.OnChar(static_cast<wchar_t>(wparam))) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_GETMINMAXINFO: {
        const SIZE minimum = DeckMinimumClientSize();
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        RECT rect{0, 0, minimum.cx, minimum.cy};
        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
        info->ptMinTrackSize.x = rect.right - rect.left;
        info->ptMinTrackSize.y = rect.bottom - rect.top;
        return 0;
    }
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(
            hwnd,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE
        );
        renderer_.Resize(hwnd);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        renderer_.Render(hwnd, visual_state_, PaletteForTheme(resolved_theme_));
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        if (settings_.theme_mode == ThemeMode::System) {
            RefreshTheme(hwnd);
            renderer_.DiscardDeviceResources();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        codex_supervisor_.Stop();
        StopStorageWorker();
        renderer_.DiscardDeviceResources();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

// ----------------------------------------------------------------------------
// Recalcule le theme courant et applique les attributs systeme.
// ----------------------------------------------------------------------------
void DeckApp::RefreshTheme(HWND hwnd) {
    resolved_theme_ = ResolveTheme(settings_.theme_mode, IsSystemDarkTheme());
    ApplySystemWindowTheme(hwnd, resolved_theme_);
}

// ----------------------------------------------------------------------------
// Demarre le worker de stockage cache-first.
// ----------------------------------------------------------------------------
void DeckApp::StartStorageWorker() {
    StopStorageWorker();
    storage_stopping_ = false;
    storage_worker_ = std::jthread([this](std::stop_token stop_token) {
        auto database_path = CodexDeckDatabasePath();
        if (!database_path) {
            return;
        }
        auto database = OpenDatabase(*database_path);
        if (!database) {
            return;
        }
        SchemaMigrator migrator;
        if (auto migrated = migrator.Migrate(*database); !migrated) {
            return;
        }

        GitProjectProbe git_probe;
        SessionSyncService sync_service(*database, session_catalog_, git_probe, [](ThreadListOptions) {
            return std::unexpected(CodexError{CodexErrorCode::Disconnected, L"Aucun client Codex connecte"});
        });
        [[maybe_unused]] const auto cached = sync_service.LoadCachedState();

        std::unique_lock lock(storage_mutex_);
        while (!stop_token.stop_requested() && !storage_stopping_) {
            storage_condition_.wait(lock, [this, &stop_token]() {
                return stop_token.stop_requested() || storage_stopping_ || storage_refresh_requested_;
            });
            if (stop_token.stop_requested() || storage_stopping_) {
                break;
            }
            storage_refresh_requested_ = false;
            lock.unlock();
            [[maybe_unused]] const auto refreshed = sync_service.RequestRefreshFromCodex();
            lock.lock();
        }
    });
}

// ----------------------------------------------------------------------------
// Arrete le worker de stockage cache-first.
// ----------------------------------------------------------------------------
void DeckApp::StopStorageWorker() {
    storage_stopping_ = true;
    storage_condition_.notify_all();
    if (storage_worker_.joinable()) {
        storage_worker_.request_stop();
        storage_worker_.join();
    }
    storage_stopping_ = false;
}

// ----------------------------------------------------------------------------
// Demande une reconciliation Codex/local au worker de stockage.
// ----------------------------------------------------------------------------
void DeckApp::RequestSessionRefresh() {
    {
        std::lock_guard lock(storage_mutex_);
        storage_refresh_requested_ = true;
    }
    storage_condition_.notify_one();
}

// ----------------------------------------------------------------------------
// Rafraichit le catalogue depuis un client Codex connecte.
// ----------------------------------------------------------------------------
void DeckApp::RefreshSessionsFromCodex(CodexClient& client) {
    client.ListThreads(ThreadListOptions{}, [this](std::expected<std::vector<CodexThreadSummary>, CodexError> response) mutable {
        if (!response || storage_stopping_) {
            return;
        }
        auto database_path = CodexDeckDatabasePath();
        if (!database_path) {
            return;
        }
        auto database = OpenDatabase(*database_path);
        if (!database) {
            return;
        }
        SchemaMigrator migrator;
        if (auto migrated = migrator.Migrate(*database); !migrated) {
            return;
        }

        GitProjectProbe git_probe;
        SessionSyncService sync_service(*database, session_catalog_, git_probe, [threads = std::move(*response)](ThreadListOptions) mutable {
            return std::expected<std::vector<CodexThreadSummary>, CodexError>{std::move(threads)};
        });
        [[maybe_unused]] const auto refreshed = sync_service.RequestRefreshFromCodex();
    });
}

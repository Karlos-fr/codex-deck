// ============================================================================
// Codex Deck - Implementation de l'orchestration applicative
// ----------------------------------------------------------------------------
// Ce fichier contient la boucle Win32 principale et reste limite a la
// coordination entre fenetre, messages et renderer.
// ============================================================================

#include "DeckApp.h"

#include "../projects/GitProjectProbe.h"
#include "../projects/ProjectAssignmentService.h"
#include "../projects/ProjectManagementController.h"
#include "../projects/ProjectRepository.h"
#include "../platform/FolderPicker.h"
#include "../sessions/FavoriteService.h"
#include "../sessions/ArchiveSessionService.h"
#include "../sessions/SessionCreationController.h"
#include "../settings/DeckPreferencesService.h"
#include "../storage/AppDataPaths.h"
#include "../storage/SchemaMigrator.h"
#include "../storage/SqliteDatabase.h"
#include "../sync/SessionSyncService.h"
#include "../window/DeckWindow.h"

#include <windowsx.h>

#include <algorithm>
#include <vector>

namespace {

// Message prive demandant un repaint apres publication du catalogue.
constexpr UINT WM_CODEX_DECK_CATALOG_UPDATED = WM_APP + 1;

// Message prive publiant le catalogue de modeles.
constexpr UINT WM_CODEX_DECK_MODELS_UPDATED = WM_APP + 2;

// Message prive selectionnant une session nouvellement creee.
constexpr UINT WM_CODEX_DECK_SESSION_CREATED = WM_APP + 3;

// Message prive signalant une erreur de creation.
constexpr UINT WM_CODEX_DECK_SESSION_CREATE_FAILED = WM_APP + 4;

// Message prive terminant un probe de sessions externes.
constexpr UINT WM_CODEX_DECK_PROBE_FINISHED = WM_APP + 5;

// Message prive selectionnant un projet nouvellement cree.
constexpr UINT WM_CODEX_DECK_PROJECT_CREATED = WM_APP + 6;

// Message prive signalant une erreur de gestion de projet.
constexpr UINT WM_CODEX_DECK_PROJECT_FAILED = WM_APP + 7;

// Message prive fermant un overlay projet apres mutation sans cible.
constexpr UINT WM_CODEX_DECK_PROJECT_COMPLETED = WM_APP + 8;

// Message prive publiant la liste d'archives chargee a la demande.
constexpr UINT WM_CODEX_DECK_ARCHIVE_LOADED = WM_APP + 9;

// Message prive appliquant les preferences chargees hors thread UI.
constexpr UINT WM_CODEX_DECK_PREFERENCES_LOADED = WM_APP + 10;

// Identifiant du timer surveillant le premier snapshot cache.
constexpr UINT_PTR kCatalogWarmupTimer = 1;

// Identifiant du timer d'animation UI.
constexpr UINT_PTR kUiAnimationTimer = 2;

// Identifiant du timer de probes externes.
constexpr UINT_PTR kExternalRefreshTimer = 3;

// Delai du timer de premier snapshot cache.
constexpr UINT kCatalogWarmupTimerMs = 250;

// Cadence du timer d'animation UI.
constexpr UINT kUiAnimationTimerMs = 16;

// Cadence d'evaluation du scheduler de probes.
constexpr UINT kExternalRefreshTimerMs = 1000;

// Contexte garde vivant jusqu'a la completion d'une creation Codex.
struct SessionCreationWork {
    // Connexion SQLite dediee au worker Codex.
    SqliteDatabase database;
    // Repository de metadonnees lie a la connexion.
    SessionMetadataRepository metadata;
    // Service d'association locale.
    ProjectAssignmentService assignments;
    // Controller de creation compose.
    SessionCreationController controller;

    // Construit le contexte autour des services non possedes.
    SessionCreationWork(SqliteDatabase source, CodexClient& client, SessionCatalog& catalog)
        : database(std::move(source)),
          metadata(database),
          assignments(catalog, metadata),
          controller(client, catalog, assignments) {
    }
};

// Convertit un diagnostic UTF-8 de stockage en texte Win32.
std::wstring WidenStorageMessage(std::string_view message) {
    if (message.empty()) {
        return L"Storage operation failed";
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, message.data(), static_cast<int>(message.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(message.begin(), message.end());
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, message.data(), static_cast<int>(message.size()), result.data(), size);
    return result;
}

// Republie les projets SQLite sans alterer les sessions du snapshot courant.
std::expected<void, StorageError> PublishStoredProjects(SqliteDatabase& database, SessionCatalog& catalog) {
    ProjectRepository repository(database);
    auto projects = repository.List();
    if (!projects) {
        return std::unexpected(projects.error());
    }
    SessionCatalogSnapshot snapshot = catalog.Current() ? *catalog.Current() : SessionCatalogSnapshot{};
    snapshot.projects = std::move(*projects);
    for (SessionRecord& session : snapshot.sessions) {
        if (session.project_id && std::ranges::none_of(snapshot.projects, [&session](const Project& project) {
                return project.id == *session.project_id;
            })) {
            session.project_id.reset();
        }
    }
    catalog.Publish(std::move(snapshot));
    return {};
}

}  // namespace

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
    renderer_.SetCommandHandler([this, hwnd](DeckCommand command) {
        HandleDeckCommand(hwnd, std::move(command));
    });
    ShowWindow(hwnd, command_show);
    UpdateWindow(hwnd);
    StartStorageWorker();
    SubmitStorageTask([hwnd](SqliteDatabase& database) {
        DeckPreferencesService service(database);
        if (auto preferences = service.Load(); preferences) {
            PostMessageW(hwnd, WM_CODEX_DECK_PREFERENCES_LOADED, 0,
                reinterpret_cast<LPARAM>(new DeckPreferences(std::move(*preferences))));
        }
    });
    codex_supervisor_.SetConnectedClientHandler([this, hwnd](CodexClient& client) {
        RefreshSessionsFromCodex(client, hwnd);
        client.ListModels([hwnd](std::expected<std::vector<CodexModelInfo>, CodexError> models) {
            if (models) {
                PostMessageW(hwnd, WM_CODEX_DECK_MODELS_UPDATED, 0, reinterpret_cast<LPARAM>(new std::vector<CodexModelInfo>(std::move(*models))));
            }
        });
    });
    codex_supervisor_.SetResyncRequiredHandler([this, hwnd](CodexClient& client) {
        RefreshSessionsFromCodex(client, hwnd);
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
    case WM_CODEX_DECK_CATALOG_UPDATED:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_CODEX_DECK_MODELS_UPDATED: {
        std::unique_ptr<std::vector<CodexModelInfo>> models(reinterpret_cast<std::vector<CodexModelInfo>*>(lparam));
        renderer_.SetAvailableModels(std::move(*models));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CODEX_DECK_SESSION_CREATED: {
        std::unique_ptr<CodexThreadId> thread_id(reinterpret_cast<CodexThreadId*>(lparam));
        renderer_.SelectThread(*thread_id);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CODEX_DECK_SESSION_CREATE_FAILED: {
        std::unique_ptr<std::wstring> error(reinterpret_cast<std::wstring*>(lparam));
        renderer_.SetSessionCreationError(std::move(*error));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CODEX_DECK_PROBE_FINISHED:
        external_refresh_scheduler_.CompleteProbe(std::chrono::steady_clock::now());
        return 0;
    case WM_CODEX_DECK_PROJECT_CREATED: {
        std::unique_ptr<ProjectId> project_id(reinterpret_cast<ProjectId*>(lparam));
        renderer_.SelectProject(*project_id);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CODEX_DECK_PROJECT_FAILED: {
        std::unique_ptr<std::wstring> error(reinterpret_cast<std::wstring*>(lparam));
        renderer_.SetProjectEditorError(std::move(*error));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CODEX_DECK_PROJECT_COMPLETED:
        renderer_.CloseProjectEditor();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_CODEX_DECK_ARCHIVE_LOADED: {
        std::unique_ptr<std::vector<CodexThreadSummary>> threads(reinterpret_cast<std::vector<CodexThreadSummary>*>(lparam));
        renderer_.SetArchiveThreads(std::move(*threads));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CODEX_DECK_PREFERENCES_LOADED: {
        std::unique_ptr<DeckPreferences> preferences(reinterpret_cast<DeckPreferences*>(lparam));
        preferences_ = std::move(*preferences);
        settings_.theme_mode = preferences_.theme_mode;
        RefreshTheme(hwnd);
        renderer_.DiscardDeviceResources();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_CREATE:
        RefreshTheme(hwnd);
        renderer_.Initialize(hwnd);
        SetTimer(hwnd, kCatalogWarmupTimer, kCatalogWarmupTimerMs, nullptr);
        SetTimer(hwnd, kUiAnimationTimer, kUiAnimationTimerMs, nullptr);
        SetTimer(hwnd, kExternalRefreshTimer, kExternalRefreshTimerMs, nullptr);
        return 0;
    case WM_TIMER:
        if (wparam == kCatalogWarmupTimer && session_catalog_.Current()) {
            KillTimer(hwnd, kCatalogWarmupTimer);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wparam == kUiAnimationTimer) {
            if (renderer_.AdvanceAnimations()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (wparam == kExternalRefreshTimer
            && external_refresh_scheduler_.TryBeginProbe(std::chrono::steady_clock::now())) {
            RequestExternalSessionProbe(hwnd);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            renderer_.Resize(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_ACTIVATEAPP:
        external_refresh_scheduler_.SetForeground(wparam != FALSE, std::chrono::steady_clock::now());
        if (wparam != FALSE) {
            external_refresh_scheduler_.RequestImmediateProbe();
        }
        return 0;
    case WM_MOUSEWHEEL:
        renderer_.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wparam));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(hwnd);
        renderer_.OnPointerDown(
            static_cast<float>(GET_X_LPARAM(lparam)),
            static_cast<float>(GET_Y_LPARAM(lparam))
        );
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tracking{};
        tracking.cbSize = sizeof(tracking);
        tracking.dwFlags = TME_LEAVE;
        tracking.hwndTrack = hwnd;
        TrackMouseEvent(&tracking);
        if (renderer_.IsPointOnSplitter(
                static_cast<float>(GET_X_LPARAM(lparam)),
                static_cast<float>(GET_Y_LPARAM(lparam))
            )) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        }
        renderer_.OnPointerMove(
            static_cast<float>(GET_X_LPARAM(lparam)),
            static_cast<float>(GET_Y_LPARAM(lparam))
        );
        renderer_.AdvanceAnimations();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSELEAVE:
        renderer_.OnPointerLeave();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        renderer_.OnPointerUp(
            static_cast<float>(GET_X_LPARAM(lparam)),
            static_cast<float>(GET_Y_LPARAM(lparam))
        );
        ReleaseCapture();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(hwnd, &point);
        if (renderer_.IsPointOnTextInput(static_cast<float>(point.x), static_cast<float>(point.y))) {
            SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
            return TRUE;
        }
        if (renderer_.IsPointOnSplitter(static_cast<float>(point.x), static_cast<float>(point.y))) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
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
        renderer_.Render(hwnd, visual_state_, PaletteForTheme(resolved_theme_), session_catalog_.Current());
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
        KillTimer(hwnd, kCatalogWarmupTimer);
        KillTimer(hwnd, kUiAnimationTimer);
        KillTimer(hwnd, kExternalRefreshTimer);
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

// Traite une commande de cycle de vie emise par le renderer.
void DeckApp::HandleDeckCommand(HWND hwnd, DeckCommand command) {
    if (command.kind == DeckCommandKind::SetThemeSystem
        || command.kind == DeckCommandKind::SetThemeLight
        || command.kind == DeckCommandKind::SetThemeDark) {
        const ThemeMode mode = command.kind == DeckCommandKind::SetThemeLight
            ? ThemeMode::Light
            : command.kind == DeckCommandKind::SetThemeDark ? ThemeMode::Dark : ThemeMode::System;
        settings_.theme_mode = mode;
        preferences_.theme_mode = mode;
        RefreshTheme(hwnd);
        renderer_.DiscardDeviceResources();
        InvalidateRect(hwnd, nullptr, FALSE);
        const DeckPreferences saved = preferences_;
        SubmitStorageTask([saved](SqliteDatabase& database) {
            DeckPreferencesService service(database);
            [[maybe_unused]] const auto persisted = service.Save(saved);
        });
        return;
    }
    if (command.kind == DeckCommandKind::ToggleFavorite && command.thread_id) {
        SubmitStorageTask([this, hwnd, thread_id = *command.thread_id](SqliteDatabase& database) {
            SessionMetadataRepository repository(database);
            FavoriteService favorites(session_catalog_, repository);
            [[maybe_unused]] const auto toggled = favorites.ToggleFavorite(thread_id);
            PostMessageW(hwnd, WM_CODEX_DECK_CATALOG_UPDATED, 0, 0);
        });
        return;
    }
    if (command.kind == DeckCommandKind::ArchiveThread && command.thread_id) {
        codex_supervisor_.Submit([this, hwnd, thread_id = *command.thread_id](CodexClient& client) mutable {
            client.ArchiveThread(std::move(thread_id), [this, &client, hwnd](std::expected<void, CodexError> result) {
                if (result) {
                    RefreshSessionsFromCodex(client, hwnd);
                }
            });
        });
        return;
    }
    if (command.kind == DeckCommandKind::OpenArchive) {
        codex_supervisor_.Submit([hwnd](CodexClient& client) {
            auto model = std::make_shared<ArchiveViewModel>();
            auto service = std::make_shared<ArchiveSessionService>(client, *model, [] {});
            service->LoadArchive([hwnd, model, service](std::expected<void, CodexError> result) {
                if (result) {
                    PostMessageW(hwnd, WM_CODEX_DECK_ARCHIVE_LOADED, 0,
                        reinterpret_cast<LPARAM>(new std::vector<CodexThreadSummary>(std::move(model->threads))));
                }
            });
        });
        return;
    }
    if (command.kind == DeckCommandKind::RestoreArchivedThread && command.thread_id) {
        codex_supervisor_.Submit([this, hwnd, thread_id = *command.thread_id](CodexClient& client) mutable {
            auto model = std::make_shared<ArchiveViewModel>();
            auto service = std::make_shared<ArchiveSessionService>(client, *model, [this, &client, hwnd] {
                RefreshSessionsFromCodex(client, hwnd);
            });
            service->RestoreArchivedThread(std::move(thread_id), [model, service](std::expected<void, CodexError>) {});
        });
        return;
    }
    if (command.kind == DeckCommandKind::PickSessionWorkspace) {
        auto folder = PickFolder(hwnd);
        if (folder && *folder) {
            renderer_.SetNewSessionWorkspacePath(std::move(**folder));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
    if (command.kind == DeckCommandKind::OpenFolderAsSession) {
        auto folder = PickFolder(hwnd);
        if (folder && *folder) {
            DeckCommand create{DeckCommandKind::NewSession, std::nullopt, std::nullopt};
            create.cwd = std::move(**folder);
            SubmitSessionCreation(hwnd, std::move(create));
        }
        return;
    }
    if ((command.kind == DeckCommandKind::NewSession
            || command.kind == DeckCommandKind::NewSessionInCurrentProject)
        && command.cwd) {
        SubmitSessionCreation(hwnd, std::move(command));
        return;
    }
    if (command.kind == DeckCommandKind::NewProject && !command.cwd) {
        auto folder = PickFolder(hwnd);
        if (folder && *folder) {
            renderer_.OpenNewProjectEditor(std::move(**folder));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }

    if (command.kind != DeckCommandKind::NewProject
        && command.kind != DeckCommandKind::RenameProject
        && command.kind != DeckCommandKind::DeleteProject) {
        return;
    }

    SubmitStorageTask([this, hwnd, command = std::move(command)](SqliteDatabase& database) mutable {
        ProjectRepository repository(database);
        ProjectManagementController controller(repository);
        std::expected<void, StorageError> result{};
        std::optional<ProjectId> created_id;
        if (command.kind == DeckCommandKind::NewProject && command.cwd && command.display_name) {
            auto created = controller.CreateProject(*command.display_name, *command.cwd);
            if (created) {
                created_id = created->id;
            } else {
                result = std::unexpected(created.error());
            }
        } else if (command.kind == DeckCommandKind::RenameProject && command.project_id && command.display_name) {
            result = controller.RenameProject(*command.project_id, *command.display_name);
        } else if (command.kind == DeckCommandKind::DeleteProject && command.project_id) {
            result = controller.DeleteProject(*command.project_id);
        } else {
            result = std::unexpected(StorageError{StorageErrorCode::ConstraintFailed, 0, "Commande projet incomplete"});
        }
        if (result) {
            result = PublishStoredProjects(database, session_catalog_);
        }
        if (!result) {
            PostMessageW(hwnd, WM_CODEX_DECK_PROJECT_FAILED, 0, reinterpret_cast<LPARAM>(new std::wstring(WidenStorageMessage(result.error().message))));
            return;
        }
        const std::optional<ProjectId> selection = created_id
            ? created_id
            : command.kind == DeckCommandKind::RenameProject ? command.project_id : std::nullopt;
        if (selection) {
            PostMessageW(hwnd, WM_CODEX_DECK_PROJECT_CREATED, 0, reinterpret_cast<LPARAM>(new ProjectId(*selection)));
        } else {
            PostMessageW(hwnd, WM_CODEX_DECK_PROJECT_COMPLETED, 0, 0);
        }
    });
}

// Lance une creation de session sur le worker Codex.
void DeckApp::SubmitSessionCreation(HWND hwnd, DeckCommand command) {
    codex_supervisor_.Submit([this, hwnd, command = std::move(command)](CodexClient& client) mutable {
        auto database_path = CodexDeckDatabasePath();
        if (!database_path) {
            PostMessageW(hwnd, WM_CODEX_DECK_SESSION_CREATE_FAILED, 0,
                reinterpret_cast<LPARAM>(new std::wstring(WidenStorageMessage(database_path.error().message))));
            return;
        }
        auto database = OpenDatabase(*database_path);
        if (!database) {
            PostMessageW(hwnd, WM_CODEX_DECK_SESSION_CREATE_FAILED, 0,
                reinterpret_cast<LPARAM>(new std::wstring(WidenStorageMessage(database.error().message))));
            return;
        }
        SchemaMigrator migrator;
        if (auto migrated = migrator.Migrate(*database); !migrated) {
            PostMessageW(hwnd, WM_CODEX_DECK_SESSION_CREATE_FAILED, 0,
                reinterpret_cast<LPARAM>(new std::wstring(WidenStorageMessage(migrated.error().message))));
            return;
        }

        auto work = std::make_shared<SessionCreationWork>(std::move(*database), client, session_catalog_);
        CreateSessionRequest request{command.project_id, *command.cwd, command.model, command.initial_prompt};
        work->controller.CreateSession(std::move(request), [this, hwnd, work](
            std::expected<CreatedSession, SessionCreationError> result
        ) mutable {
            if (!result) {
                std::wstring message = L"Unable to create the session";
                if (result.error().codex) {
                    message = result.error().codex->message;
                } else if (result.error().storage) {
                    message = WidenStorageMessage(result.error().storage->message);
                }
                PostMessageW(hwnd, WM_CODEX_DECK_SESSION_CREATE_FAILED, 0,
                    reinterpret_cast<LPARAM>(new std::wstring(std::move(message))));
                return;
            }
            PostMessageW(hwnd, WM_CODEX_DECK_SESSION_CREATED, 0,
                reinterpret_cast<LPARAM>(new CodexThreadId(result->thread.id)));
        });
    });
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
                return stop_token.stop_requested() || storage_stopping_ || storage_refresh_requested_ || !storage_tasks_.empty();
            });
            if (stop_token.stop_requested() || storage_stopping_) {
                break;
            }
            const bool refresh = storage_refresh_requested_;
            storage_refresh_requested_ = false;
            std::deque<std::move_only_function<void(SqliteDatabase&)>> tasks;
            tasks.swap(storage_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task(*database);
            }
            if (refresh) {
                [[maybe_unused]] const auto refreshed = sync_service.RequestRefreshFromCodex();
            }
            lock.lock();
        }
    });
}

// Place une operation SQLite dans la file du worker stockage.
void DeckApp::SubmitStorageTask(std::move_only_function<void(SqliteDatabase&)> task) {
    {
        std::lock_guard lock(storage_mutex_);
        storage_tasks_.push_back(std::move(task));
    }
    storage_condition_.notify_one();
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
void DeckApp::RefreshSessionsFromCodex(CodexClient& client, HWND hwnd) {
    client.ListThreads(ThreadListOptions{}, [this, hwnd](std::expected<std::vector<CodexThreadSummary>, CodexError> response) mutable {
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
        PostMessageW(hwnd, WM_CODEX_DECK_CATALOG_UPDATED, 0, 0);
    });
}

// Compare les threads recents et lance une reconciliation complete si necessaire.
void DeckApp::RequestExternalSessionProbe(HWND hwnd) {
    codex_supervisor_.Submit([this, hwnd](CodexClient& client) {
        ThreadListOptions options{};
        options.max_items = 100;
        options.use_state_db_only = true;
        client.ListThreads(options, [this, &client, hwnd](std::expected<std::vector<CodexThreadSummary>, CodexError> threads) {
            if (threads) {
                const std::uint64_t fingerprint = RecentSessionFingerprint(*threads);
                if (external_session_fingerprint_ && *external_session_fingerprint_ != fingerprint) {
                    RefreshSessionsFromCodex(client, hwnd);
                }
                external_session_fingerprint_ = fingerprint;
            }
            PostMessageW(hwnd, WM_CODEX_DECK_PROBE_FINISHED, 0, 0);
        });
    });
}

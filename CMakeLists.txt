cmake_minimum_required(VERSION 3.24)

project(CodexDeck
    VERSION 0.1.0
    DESCRIPTION "Codex Deck"
    LANGUAGES C CXX RC
)

include(CTest)
include(CheckCXXCompilerFlag)
include(FetchContent)

# Active C++23 strict sans laisser MSVC retomber sur /std:c++latest.
function(codex_deck_enable_cpp23 target_name)
    if(MSVC)
        check_cxx_compiler_flag("/std:c++23" CODEX_DECK_HAS_STD_CPP23)
        if(CODEX_DECK_HAS_STD_CPP23)
            target_compile_options(${target_name} PRIVATE /std:c++23)
        else()
            check_cxx_compiler_flag("/std:c++23preview" CODEX_DECK_HAS_STD_CPP23_PREVIEW)
            if(NOT CODEX_DECK_HAS_STD_CPP23_PREVIEW)
                message(FATAL_ERROR "Codex Deck requires an MSVC toolchain with C++23 support")
            endif()
            target_compile_options(${target_name} PRIVATE /std:c++23preview)
        endif()
    else()
        target_compile_features(${target_name} PRIVATE cxx_std_23)
    endif()
endfunction()

# Applique les options MSVC communes aux cibles C++ du projet.
function(codex_deck_enable_msvc_defaults target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
        )
    endif()
endfunction()

# Privilegie la taille dans tous les artefacts Release MSVC.
if(MSVC)
    string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# Evite les objets ABI obsoletes avec Ninja sur un Windows francais.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    string(ASCII 194 160 CODEX_DECK_UTF8_NBSP)
    set(CODEX_DECK_SHOWINCLUDES_PREFIX
        "Remarque${CODEX_DECK_UTF8_NBSP}: inclusion du fichier${CODEX_DECK_UTF8_NBSP}:"
    )
    set(CMAKE_C_CL_SHOWINCLUDES_PREFIX "${CODEX_DECK_SHOWINCLUDES_PREFIX}")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CODEX_DECK_SHOWINCLUDES_PREFIX}")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CODEX_DECK_SHOWINCLUDES_PREFIX}")
endif()

set(CODEX_DECK_VERSION_BUILD "1" CACHE STRING "Windows file version build number")
set(CODEX_DECK_VERSION_COMPANY "Codex Deck" CACHE STRING "Windows version metadata company name")
set(CODEX_DECK_VERSION_COPYRIGHT "Copyright (C) 2026 Codex Deck" CACHE STRING "Windows version metadata copyright")

if(NOT CODEX_DECK_VERSION_BUILD MATCHES "^[0-9]+$")
    message(FATAL_ERROR "CODEX_DECK_VERSION_BUILD must be a non-negative integer")
endif()

set(CODEX_DECK_VERSION_STRING "${PROJECT_VERSION}")
set(CODEX_DECK_FILE_VERSION_STRING "${PROJECT_VERSION}.${CODEX_DECK_VERSION_BUILD}")

configure_file(
    src/resources/VersionInfo.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/VersionInfo.rc
    @ONLY
)

FetchContent_Declare(sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(sqlite3 STATIC
    ${sqlite3_SOURCE_DIR}/sqlite3.c
)

target_include_directories(sqlite3 PUBLIC
    ${sqlite3_SOURCE_DIR}
)

target_compile_definitions(sqlite3 PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION
    $<$<CONFIG:Release>:SQLITE_DQS=0>
    $<$<CONFIG:Release>:SQLITE_DEFAULT_MEMSTATUS=0>
    $<$<CONFIG:Release>:SQLITE_LIKE_DOESNT_MATCH_BLOBS>
    $<$<CONFIG:Release>:SQLITE_MAX_EXPR_DEPTH=0>
    $<$<CONFIG:Release>:SQLITE_OMIT_DECLTYPE>
    $<$<CONFIG:Release>:SQLITE_OMIT_DEPRECATED>
    $<$<CONFIG:Release>:SQLITE_OMIT_PROGRESS_CALLBACK>
    $<$<CONFIG:Release>:SQLITE_OMIT_SHARED_CACHE>
    $<$<CONFIG:Release>:SQLITE_STRICT_SUBTYPE=1>
)

add_executable(CodexDeck WIN32
    ${CMAKE_CURRENT_BINARY_DIR}/generated/VersionInfo.rc
    src/app/DeckApp.cpp
    src/codex/CodexClient.cpp
    src/codex/CodexExecutableResolver.cpp
    src/codex/CodexProcess.cpp
    src/codex/CodexProtocol.cpp
    src/codex/CodexSupervisor.cpp
    src/codex/JsonRpcTransport.cpp
    src/graphics/CompositionHost.cpp
    src/input/KeyboardShortcuts.cpp
    src/main.cpp
    src/model/ActivityOrdering.cpp
    src/model/SessionCatalog.cpp
    src/navigation/ActivityBarModel.cpp
    src/navigation/ActivityBarView.cpp
    src/navigation/ArchiveViewModel.cpp
    src/navigation/CommandPaletteModel.cpp
    src/navigation/CommandPaletteView.cpp
    src/navigation/ProjectTreeModel.cpp
    src/navigation/ProjectTreeView.cpp
    src/navigation/TreeHitTesting.cpp
    src/navigation/TreeDragController.cpp
    src/platform/FolderPicker.cpp
    src/projects/GitProjectProbe.cpp
    src/projects/PathNormalization.cpp
    src/projects/ProjectDetector.cpp
    src/projects/ProjectEditorView.cpp
    src/projects/ProjectManagementController.cpp
    src/projects/ProjectAssignmentService.cpp
    src/projects/ProjectRepository.cpp
    src/rendering/DeckRenderer.cpp
    src/resources/CodexDeck.rc
    src/storage/AppDataPaths.cpp
    src/storage/SchemaMigrator.cpp
    src/storage/SessionMetadataRepository.cpp
    src/storage/SqliteDatabase.cpp
    src/sync/SessionSyncService.cpp
    src/sync/ExternalSessionProbe.cpp
    src/sync/ExternalSessionRefreshScheduler.cpp
    src/search/FuzzyMatcher.cpp
    src/settings/DeckPreferencesService.cpp
    src/sessions/SessionCreationController.cpp
    src/sessions/NewSessionOverlayModel.cpp
    src/sessions/NewSessionOverlayView.cpp
    src/sessions/ArchiveSessionService.cpp
    src/sessions/FavoriteService.cpp
    src/ui/MainLayout.cpp
    src/ui/ScrollState.cpp
    src/ui/ScrollbarGeometry.cpp
    src/ui/TreeVisualAnimation.cpp
    src/ui/VirtualListLayout.cpp
    src/theme/Theme.cpp
    src/win32/SingleInstance.cpp
    src/window/DeckWindow.cpp
)

codex_deck_enable_cpp23(CodexDeck)
codex_deck_enable_msvc_defaults(CodexDeck)

set_source_files_properties(src/resources/CodexDeck.rc PROPERTIES
    OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceIds.h;${CMAKE_CURRENT_SOURCE_DIR}/src/resources/strings.fr.rc;${CMAKE_CURRENT_SOURCE_DIR}/src/resources/strings.en.rc"
)

target_compile_definitions(CodexDeck PRIVATE
    CODEX_DECK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CODEX_DECK_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CODEX_DECK_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    CODEX_DECK_VERSION_BUILD=${CODEX_DECK_VERSION_BUILD}
    CODEX_DECK_VERSION_STRING=L"${CODEX_DECK_VERSION_STRING}"
    CODEX_DECK_FILE_VERSION_STRING=L"${CODEX_DECK_FILE_VERSION_STRING}"
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(CodexDeck PRIVATE
    d2d1
    d3d11
    dxgi
    dcomp
    dwrite
    dwmapi
    user32
    shell32
    ole32
    gdi32
    uxtheme
    advapi32
    sqlite3
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(sqlite3 PRIVATE
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
    )

    target_compile_options(CodexDeck PRIVATE
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
    )

    target_link_options(CodexDeck PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/settings/DeckPreferencesServiceTests.cpp")
    add_executable(DeckPreferencesServiceTests
        tests/settings/DeckPreferencesServiceTests.cpp
        src/settings/DeckPreferencesService.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(DeckPreferencesServiceTests)
    codex_deck_enable_msvc_defaults(DeckPreferencesServiceTests)
    target_include_directories(DeckPreferencesServiceTests PRIVATE src)
    target_compile_definitions(DeckPreferencesServiceTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(DeckPreferencesServiceTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME DeckPreferencesServiceTests COMMAND DeckPreferencesServiceTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sync/ExternalSessionRefreshSchedulerTests.cpp")
    add_executable(ExternalSessionRefreshSchedulerTests
        tests/sync/ExternalSessionRefreshSchedulerTests.cpp
        src/sync/ExternalSessionRefreshScheduler.cpp
    )
    codex_deck_enable_cpp23(ExternalSessionRefreshSchedulerTests)
    codex_deck_enable_msvc_defaults(ExternalSessionRefreshSchedulerTests)
    target_include_directories(ExternalSessionRefreshSchedulerTests PRIVATE src)
    add_test(NAME ExternalSessionRefreshSchedulerTests COMMAND ExternalSessionRefreshSchedulerTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sync/ExternalSessionProbeTests.cpp")
    add_executable(ExternalSessionProbeTests
        tests/sync/ExternalSessionProbeTests.cpp
        src/sync/ExternalSessionProbe.cpp
    )
    codex_deck_enable_cpp23(ExternalSessionProbeTests)
    codex_deck_enable_msvc_defaults(ExternalSessionProbeTests)
    target_include_directories(ExternalSessionProbeTests PRIVATE src)
    target_link_libraries(ExternalSessionProbeTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME ExternalSessionProbeTests COMMAND ExternalSessionProbeTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sessions/FavoriteServiceTests.cpp")
    add_executable(FavoriteServiceTests
        tests/sessions/FavoriteServiceTests.cpp
        src/model/SessionCatalog.cpp
        src/sessions/FavoriteService.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SessionMetadataRepository.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(FavoriteServiceTests)
    codex_deck_enable_msvc_defaults(FavoriteServiceTests)
    target_include_directories(FavoriteServiceTests PRIVATE src)
    target_compile_definitions(FavoriteServiceTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(FavoriteServiceTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME FavoriteServiceTests COMMAND FavoriteServiceTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sessions/ArchiveSessionServiceTests.cpp")
    add_executable(ArchiveSessionServiceTests
        tests/sessions/ArchiveSessionServiceTests.cpp
        src/codex/CodexClient.cpp
        src/codex/CodexProcess.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
        src/navigation/ArchiveViewModel.cpp
        src/sessions/ArchiveSessionService.cpp
    )
    codex_deck_enable_cpp23(ArchiveSessionServiceTests)
    codex_deck_enable_msvc_defaults(ArchiveSessionServiceTests)
    target_include_directories(ArchiveSessionServiceTests PRIVATE src)
    target_compile_definitions(ArchiveSessionServiceTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(ArchiveSessionServiceTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME ArchiveSessionServiceTests COMMAND ArchiveSessionServiceTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sessions/NewSessionOverlayModelTests.cpp")
    add_executable(NewSessionOverlayModelTests
        tests/sessions/NewSessionOverlayModelTests.cpp
        src/sessions/NewSessionOverlayModel.cpp
    )
    codex_deck_enable_cpp23(NewSessionOverlayModelTests)
    codex_deck_enable_msvc_defaults(NewSessionOverlayModelTests)
    target_include_directories(NewSessionOverlayModelTests PRIVATE src)
    target_link_libraries(NewSessionOverlayModelTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME NewSessionOverlayModelTests COMMAND NewSessionOverlayModelTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sessions/SessionCreationControllerTests.cpp")
    add_executable(SessionCreationControllerTests
        tests/sessions/SessionCreationControllerTests.cpp
        src/codex/CodexClient.cpp
        src/codex/CodexProcess.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
        src/model/SessionCatalog.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectAssignmentService.cpp
        src/projects/ProjectRepository.cpp
        src/sessions/SessionCreationController.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SessionMetadataRepository.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(SessionCreationControllerTests)
    codex_deck_enable_msvc_defaults(SessionCreationControllerTests)
    target_include_directories(SessionCreationControllerTests PRIVATE src)
    target_compile_definitions(SessionCreationControllerTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(SessionCreationControllerTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME SessionCreationControllerTests COMMAND SessionCreationControllerTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/projects/ProjectManagementControllerTests.cpp")
    add_executable(ProjectManagementControllerTests
        tests/projects/ProjectManagementControllerTests.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectManagementController.cpp
        src/projects/ProjectRepository.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SessionMetadataRepository.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(ProjectManagementControllerTests)
    codex_deck_enable_msvc_defaults(ProjectManagementControllerTests)
    target_include_directories(ProjectManagementControllerTests PRIVATE src)
    target_compile_definitions(ProjectManagementControllerTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(ProjectManagementControllerTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME ProjectManagementControllerTests COMMAND ProjectManagementControllerTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/SingleInstanceTests.cpp")
    add_executable(SingleInstanceTests
        tests/SingleInstanceTests.cpp
        src/win32/SingleInstance.cpp
    )
    codex_deck_enable_cpp23(SingleInstanceTests)
    codex_deck_enable_msvc_defaults(SingleInstanceTests)
    target_include_directories(SingleInstanceTests PRIVATE src)
    target_compile_definitions(SingleInstanceTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_test(NAME SingleInstanceTests COMMAND SingleInstanceTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/DeckWindowGeometryTests.cpp")
    add_executable(DeckWindowGeometryTests
        tests/DeckWindowGeometryTests.cpp
        src/window/DeckWindow.cpp
    )
    codex_deck_enable_cpp23(DeckWindowGeometryTests)
    codex_deck_enable_msvc_defaults(DeckWindowGeometryTests)
    target_include_directories(DeckWindowGeometryTests PRIVATE src)
    target_compile_definitions(DeckWindowGeometryTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(DeckWindowGeometryTests PRIVATE dwmapi user32)
    add_test(NAME DeckWindowGeometryTests COMMAND DeckWindowGeometryTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CompositionSmokeTests.cpp")
    add_executable(CompositionSmokeTests
        tests/CompositionSmokeTests.cpp
        src/graphics/CompositionHost.cpp
    )
    codex_deck_enable_cpp23(CompositionSmokeTests)
    codex_deck_enable_msvc_defaults(CompositionSmokeTests)
    target_include_directories(CompositionSmokeTests PRIVATE src)
    target_compile_definitions(CompositionSmokeTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(CompositionSmokeTests PRIVATE d2d1 dcomp dwrite d3d11 dxgi user32)
    add_test(NAME CompositionSmokeTests COMMAND CompositionSmokeTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ThemeTests.cpp")
    add_executable(ThemeTests
        tests/ThemeTests.cpp
        src/theme/Theme.cpp
    )
    codex_deck_enable_cpp23(ThemeTests)
    codex_deck_enable_msvc_defaults(ThemeTests)
    target_include_directories(ThemeTests PRIVATE src)
    target_compile_definitions(ThemeTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(ThemeTests PRIVATE d2d1 dwmapi advapi32 user32)
    add_test(NAME ThemeTests COMMAND ThemeTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexProtocolTests.cpp")
    add_executable(CodexProtocolTests
        tests/codex/CodexProtocolTests.cpp
        src/codex/CodexProtocol.cpp
    )
    codex_deck_enable_cpp23(CodexProtocolTests)
    codex_deck_enable_msvc_defaults(CodexProtocolTests)
    target_include_directories(CodexProtocolTests PRIVATE src)
    target_compile_definitions(CodexProtocolTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(CodexProtocolTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CodexProtocolTests COMMAND CodexProtocolTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexExecutableResolverTests.cpp")
    add_executable(CodexExecutableResolverTests
        tests/codex/CodexExecutableResolverTests.cpp
        src/codex/CodexExecutableResolver.cpp
    )
    codex_deck_enable_cpp23(CodexExecutableResolverTests)
    codex_deck_enable_msvc_defaults(CodexExecutableResolverTests)
    target_include_directories(CodexExecutableResolverTests PRIVATE src)
    target_compile_definitions(CodexExecutableResolverTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_test(NAME CodexExecutableResolverTests COMMAND CodexExecutableResolverTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fakes/FakeAppServer.cpp")
    add_executable(FakeAppServer
        tests/fakes/FakeAppServer.cpp
    )
    codex_deck_enable_cpp23(FakeAppServer)
    codex_deck_enable_msvc_defaults(FakeAppServer)
    target_link_libraries(FakeAppServer PRIVATE nlohmann_json::nlohmann_json)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexProcessTests.cpp")
    add_executable(CodexProcessTests
        tests/codex/CodexProcessTests.cpp
        src/codex/CodexProcess.cpp
    )
    codex_deck_enable_cpp23(CodexProcessTests)
    codex_deck_enable_msvc_defaults(CodexProcessTests)
    target_include_directories(CodexProcessTests PRIVATE src)
    target_compile_definitions(CodexProcessTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_test(NAME CodexProcessTests COMMAND CodexProcessTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/JsonRpcTransportTests.cpp")
    add_executable(JsonRpcTransportTests
        tests/codex/JsonRpcTransportTests.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
    )
    codex_deck_enable_cpp23(JsonRpcTransportTests)
    codex_deck_enable_msvc_defaults(JsonRpcTransportTests)
    target_include_directories(JsonRpcTransportTests PRIVATE src)
    target_compile_definitions(JsonRpcTransportTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(JsonRpcTransportTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME JsonRpcTransportTests COMMAND JsonRpcTransportTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexClientTests.cpp")
    add_executable(CodexClientTests
        tests/codex/CodexClientTests.cpp
        src/codex/CodexClient.cpp
        src/codex/CodexProcess.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
    )
    codex_deck_enable_cpp23(CodexClientTests)
    codex_deck_enable_msvc_defaults(CodexClientTests)
    target_include_directories(CodexClientTests PRIVATE src)
    target_compile_definitions(CodexClientTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(CodexClientTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CodexClientTests COMMAND CodexClientTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexModelListTests.cpp")
    add_executable(CodexModelListTests
        tests/codex/CodexModelListTests.cpp
        src/codex/CodexClient.cpp
        src/codex/CodexProcess.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
    )
    codex_deck_enable_cpp23(CodexModelListTests)
    codex_deck_enable_msvc_defaults(CodexModelListTests)
    target_include_directories(CodexModelListTests PRIVATE src)
    target_compile_definitions(CodexModelListTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(CodexModelListTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CodexModelListTests COMMAND CodexModelListTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexThreadListOptionsTests.cpp")
    add_executable(CodexThreadListOptionsTests
        tests/codex/CodexThreadListOptionsTests.cpp
        src/codex/CodexClient.cpp
        src/codex/CodexProcess.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
    )
    codex_deck_enable_cpp23(CodexThreadListOptionsTests)
    codex_deck_enable_msvc_defaults(CodexThreadListOptionsTests)
    target_include_directories(CodexThreadListOptionsTests PRIVATE src)
    target_compile_definitions(CodexThreadListOptionsTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(CodexThreadListOptionsTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CodexThreadListOptionsTests COMMAND CodexThreadListOptionsTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/SessionRuntimeTests.cpp")
    add_executable(SessionRuntimeTests
        tests/model/SessionRuntimeTests.cpp
        src/model/SessionRuntimeRegistry.cpp
    )
    codex_deck_enable_cpp23(SessionRuntimeTests)
    codex_deck_enable_msvc_defaults(SessionRuntimeTests)
    target_include_directories(SessionRuntimeTests PRIVATE src)
    target_link_libraries(SessionRuntimeTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME SessionRuntimeTests COMMAND SessionRuntimeTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexApprovalTests.cpp")
    add_executable(CodexApprovalTests
        tests/codex/CodexApprovalTests.cpp
        src/codex/CodexEventRouter.cpp
        src/codex/CodexProtocol.cpp
        src/codex/JsonRpcTransport.cpp
        src/model/SessionRuntimeRegistry.cpp
    )
    codex_deck_enable_cpp23(CodexApprovalTests)
    codex_deck_enable_msvc_defaults(CodexApprovalTests)
    target_include_directories(CodexApprovalTests PRIVATE src)
    target_compile_definitions(CodexApprovalTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(CodexApprovalTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CodexApprovalTests COMMAND CodexApprovalTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/codex/CodexSupervisorTests.cpp")
    add_executable(CodexSupervisorTests
        tests/codex/CodexSupervisorTests.cpp
        src/codex/CodexClient.cpp
        src/codex/CodexExecutableResolver.cpp
        src/codex/CodexProcess.cpp
        src/codex/CodexProtocol.cpp
        src/codex/CodexSupervisor.cpp
        src/codex/JsonRpcTransport.cpp
    )
    codex_deck_enable_cpp23(CodexSupervisorTests)
    codex_deck_enable_msvc_defaults(CodexSupervisorTests)
    target_include_directories(CodexSupervisorTests PRIVATE src)
    target_compile_definitions(CodexSupervisorTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(CodexSupervisorTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CodexSupervisorTests COMMAND CodexSupervisorTests $<TARGET_FILE:FakeAppServer>)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/storage/SqliteDatabaseTests.cpp")
    add_executable(SqliteDatabaseTests
        tests/storage/SqliteDatabaseTests.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(SqliteDatabaseTests)
    codex_deck_enable_msvc_defaults(SqliteDatabaseTests)
    target_include_directories(SqliteDatabaseTests PRIVATE src)
    target_compile_definitions(SqliteDatabaseTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(SqliteDatabaseTests PRIVATE sqlite3)
    add_test(NAME SqliteDatabaseTests COMMAND SqliteDatabaseTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/projects/ProjectRepositoryTests.cpp")
    add_executable(ProjectRepositoryTests
        tests/projects/ProjectRepositoryTests.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectRepository.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(ProjectRepositoryTests)
    codex_deck_enable_msvc_defaults(ProjectRepositoryTests)
    target_include_directories(ProjectRepositoryTests PRIVATE src)
    target_compile_definitions(ProjectRepositoryTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(ProjectRepositoryTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME ProjectRepositoryTests COMMAND ProjectRepositoryTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/storage/SessionMetadataRepositoryTests.cpp")
    add_executable(SessionMetadataRepositoryTests
        tests/storage/SessionMetadataRepositoryTests.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectRepository.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SessionMetadataRepository.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(SessionMetadataRepositoryTests)
    codex_deck_enable_msvc_defaults(SessionMetadataRepositoryTests)
    target_include_directories(SessionMetadataRepositoryTests PRIVATE src)
    target_compile_definitions(SessionMetadataRepositoryTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(SessionMetadataRepositoryTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME SessionMetadataRepositoryTests COMMAND SessionMetadataRepositoryTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/projects/ProjectDetectorTests.cpp")
    add_executable(ProjectDetectorTests
        tests/projects/ProjectDetectorTests.cpp
        src/projects/GitProjectProbe.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectDetector.cpp
    )
    codex_deck_enable_cpp23(ProjectDetectorTests)
    codex_deck_enable_msvc_defaults(ProjectDetectorTests)
    target_include_directories(ProjectDetectorTests PRIVATE src)
    target_compile_definitions(ProjectDetectorTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(ProjectDetectorTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME ProjectDetectorTests COMMAND ProjectDetectorTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/SessionCatalogTests.cpp")
    add_executable(SessionCatalogTests
        tests/model/SessionCatalogTests.cpp
        src/model/SessionCatalog.cpp
    )
    codex_deck_enable_cpp23(SessionCatalogTests)
    codex_deck_enable_msvc_defaults(SessionCatalogTests)
    target_include_directories(SessionCatalogTests PRIVATE src)
    target_link_libraries(SessionCatalogTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME SessionCatalogTests COMMAND SessionCatalogTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sync/SessionSyncTests.cpp")
    add_executable(SessionSyncTests
        tests/sync/SessionSyncTests.cpp
        src/model/SessionCatalog.cpp
        src/projects/GitProjectProbe.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectDetector.cpp
        src/projects/ProjectRepository.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SessionMetadataRepository.cpp
        src/storage/SqliteDatabase.cpp
        src/sync/SessionSyncService.cpp
        src/sync/ExternalSessionProbe.cpp
    )
    codex_deck_enable_cpp23(SessionSyncTests)
    codex_deck_enable_msvc_defaults(SessionSyncTests)
    target_include_directories(SessionSyncTests PRIVATE src)
    target_compile_definitions(SessionSyncTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(SessionSyncTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME SessionSyncTests COMMAND SessionSyncTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/ActivityOrderingTests.cpp")
    add_executable(ActivityOrderingTests
        tests/model/ActivityOrderingTests.cpp
        src/model/ActivityOrdering.cpp
    )
    codex_deck_enable_cpp23(ActivityOrderingTests)
    codex_deck_enable_msvc_defaults(ActivityOrderingTests)
    target_include_directories(ActivityOrderingTests PRIVATE src)
    target_link_libraries(ActivityOrderingTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME ActivityOrderingTests COMMAND ActivityOrderingTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/projects/ProjectAssignmentServiceTests.cpp")
    add_executable(ProjectAssignmentServiceTests
        tests/projects/ProjectAssignmentServiceTests.cpp
        src/model/SessionCatalog.cpp
        src/projects/ProjectAssignmentService.cpp
        src/projects/PathNormalization.cpp
        src/projects/ProjectRepository.cpp
        src/storage/SchemaMigrator.cpp
        src/storage/SessionMetadataRepository.cpp
        src/storage/SqliteDatabase.cpp
    )
    codex_deck_enable_cpp23(ProjectAssignmentServiceTests)
    codex_deck_enable_msvc_defaults(ProjectAssignmentServiceTests)
    target_include_directories(ProjectAssignmentServiceTests PRIVATE src)
    target_compile_definitions(ProjectAssignmentServiceTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(ProjectAssignmentServiceTests PRIVATE sqlite3 nlohmann_json::nlohmann_json)
    add_test(NAME ProjectAssignmentServiceTests COMMAND ProjectAssignmentServiceTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/storage/AppDataPathsTests.cpp")
    add_executable(AppDataPathsTests
        tests/storage/AppDataPathsTests.cpp
        src/storage/AppDataPaths.cpp
    )
    codex_deck_enable_cpp23(AppDataPathsTests)
    codex_deck_enable_msvc_defaults(AppDataPathsTests)
    target_include_directories(AppDataPathsTests PRIVATE src)
    target_compile_definitions(AppDataPathsTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(AppDataPathsTests PRIVATE shell32 ole32)
    add_test(NAME AppDataPathsTests COMMAND AppDataPathsTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/navigation/ProjectTreeModelTests.cpp")
    add_executable(ProjectTreeModelTests
        tests/navigation/ProjectTreeModelTests.cpp
        src/model/ActivityOrdering.cpp
        src/navigation/ProjectTreeModel.cpp
    )
    codex_deck_enable_cpp23(ProjectTreeModelTests)
    codex_deck_enable_msvc_defaults(ProjectTreeModelTests)
    target_include_directories(ProjectTreeModelTests PRIVATE src)
    target_link_libraries(ProjectTreeModelTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME ProjectTreeModelTests COMMAND ProjectTreeModelTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/VirtualListLayoutTests.cpp")
    add_executable(VirtualListLayoutTests
        tests/ui/VirtualListLayoutTests.cpp
        src/ui/ScrollState.cpp
        src/ui/VirtualListLayout.cpp
    )
    codex_deck_enable_cpp23(VirtualListLayoutTests)
    codex_deck_enable_msvc_defaults(VirtualListLayoutTests)
    target_include_directories(VirtualListLayoutTests PRIVATE src)
    add_test(NAME VirtualListLayoutTests COMMAND VirtualListLayoutTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/navigation/TreeHitTestingTests.cpp")
    add_executable(TreeHitTestingTests
        tests/navigation/TreeHitTestingTests.cpp
        src/navigation/TreeHitTesting.cpp
        src/ui/ScrollState.cpp
    )
    codex_deck_enable_cpp23(TreeHitTestingTests)
    codex_deck_enable_msvc_defaults(TreeHitTestingTests)
    target_include_directories(TreeHitTestingTests PRIVATE src)
    target_link_libraries(TreeHitTestingTests PRIVATE d2d1)
    add_test(NAME TreeHitTestingTests COMMAND TreeHitTestingTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/navigation/ActivityBarModelTests.cpp")
    add_executable(ActivityBarModelTests
        tests/navigation/ActivityBarModelTests.cpp
        src/navigation/ActivityBarModel.cpp
    )
    codex_deck_enable_cpp23(ActivityBarModelTests)
    codex_deck_enable_msvc_defaults(ActivityBarModelTests)
    target_include_directories(ActivityBarModelTests PRIVATE src)
    target_link_libraries(ActivityBarModelTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME ActivityBarModelTests COMMAND ActivityBarModelTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/search/FuzzyMatcherTests.cpp")
    add_executable(FuzzyMatcherTests
        tests/search/FuzzyMatcherTests.cpp
        src/search/FuzzyMatcher.cpp
    )
    codex_deck_enable_cpp23(FuzzyMatcherTests)
    codex_deck_enable_msvc_defaults(FuzzyMatcherTests)
    target_include_directories(FuzzyMatcherTests PRIVATE src)
    add_test(NAME FuzzyMatcherTests COMMAND FuzzyMatcherTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/navigation/CommandPaletteModelTests.cpp")
    add_executable(CommandPaletteModelTests
        tests/navigation/CommandPaletteModelTests.cpp
        src/navigation/CommandPaletteModel.cpp
        src/search/FuzzyMatcher.cpp
    )
    codex_deck_enable_cpp23(CommandPaletteModelTests)
    codex_deck_enable_msvc_defaults(CommandPaletteModelTests)
    target_include_directories(CommandPaletteModelTests PRIVATE src)
    target_link_libraries(CommandPaletteModelTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME CommandPaletteModelTests COMMAND CommandPaletteModelTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/input/KeyboardShortcutsTests.cpp")
    add_executable(KeyboardShortcutsTests
        tests/input/KeyboardShortcutsTests.cpp
        src/input/KeyboardShortcuts.cpp
    )
    codex_deck_enable_cpp23(KeyboardShortcutsTests)
    codex_deck_enable_msvc_defaults(KeyboardShortcutsTests)
    target_include_directories(KeyboardShortcutsTests PRIVATE src)
    target_compile_definitions(KeyboardShortcutsTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(KeyboardShortcutsTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME KeyboardShortcutsTests COMMAND KeyboardShortcutsTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/navigation/TreeDragControllerTests.cpp")
    add_executable(TreeDragControllerTests
        tests/navigation/TreeDragControllerTests.cpp
        src/navigation/TreeDragController.cpp
    )
    codex_deck_enable_cpp23(TreeDragControllerTests)
    codex_deck_enable_msvc_defaults(TreeDragControllerTests)
    target_include_directories(TreeDragControllerTests PRIVATE src)
    target_link_libraries(TreeDragControllerTests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME TreeDragControllerTests COMMAND TreeDragControllerTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/MainLayoutTests.cpp")
    add_executable(MainLayoutTests
        tests/ui/MainLayoutTests.cpp
        src/ui/MainLayout.cpp
    )
    codex_deck_enable_cpp23(MainLayoutTests)
    codex_deck_enable_msvc_defaults(MainLayoutTests)
    target_include_directories(MainLayoutTests PRIVATE src)
    add_test(NAME MainLayoutTests COMMAND MainLayoutTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/ScrollbarGeometryTests.cpp")
    add_executable(ScrollbarGeometryTests
        tests/ui/ScrollbarGeometryTests.cpp
        src/ui/ScrollbarGeometry.cpp
    )
    codex_deck_enable_cpp23(ScrollbarGeometryTests)
    codex_deck_enable_msvc_defaults(ScrollbarGeometryTests)
    target_include_directories(ScrollbarGeometryTests PRIVATE src)
    add_test(NAME ScrollbarGeometryTests COMMAND ScrollbarGeometryTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui/TreeVisualAnimationTests.cpp")
    add_executable(TreeVisualAnimationTests
        tests/ui/TreeVisualAnimationTests.cpp
        src/ui/TreeVisualAnimation.cpp
    )
    codex_deck_enable_cpp23(TreeVisualAnimationTests)
    codex_deck_enable_msvc_defaults(TreeVisualAnimationTests)
    target_include_directories(TreeVisualAnimationTests PRIVATE src)
    add_test(NAME TreeVisualAnimationTests COMMAND TreeVisualAnimationTests)
endif()

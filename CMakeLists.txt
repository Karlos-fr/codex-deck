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
    src/main.cpp
    src/rendering/DeckRenderer.cpp
    src/resources/CodexDeck.rc
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

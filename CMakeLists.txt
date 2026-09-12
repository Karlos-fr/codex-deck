cmake_minimum_required(VERSION 3.24)

project(CodexDeck
    VERSION 0.1.0
    DESCRIPTION "Codex Deck"
    LANGUAGES C CXX RC
)

include(CTest)
include(CheckCXXCompilerFlag)

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

# Privilegie la taille dans tous les artefacts Release MSVC sans empiler /O2
# et /O1 sur la meme ligne de commande. Les autres configurations sont intactes.
if(MSVC)
    string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# Evite les objets ABI obsoletes avec Ninja sur un Windows francais. CMake 4.4
# detecte ici un prefixe /showIncludes mojibake qui empeche Ninja de suivre les
# en-tetes. La chaine est reconstruite en UTF-8 avec les espaces insecables reels.
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
option(CODEX_DECK_ENABLE_ASAN "Enable AddressSanitizer for memory corruption diagnostics" OFF)

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

include(FetchContent)

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
    src/icon/AppIcon.cpp
    src/settings/AppSettings.cpp
    src/settings/AppSettingsSaveWorker.cpp
    src/startup/WindowsStartup.cpp
    src/win32/SingleInstance.cpp
    src/motion/WidgetMotionEffectsSettings.cpp
    src/motion/WidgetMotionCurves.cpp
    src/motion/WidgetMotionWaveField.cpp
    src/resources/CodexDeck.rc
    src/usage/UsageProviderFactory.cpp
    src/providers/codex/CodexAuthReader.cpp
    src/providers/codex/CodexUsageClient.cpp
    src/providers/codex/CodexUsageParser.cpp
    src/providers/codex/CodexUsageProvider.cpp
    src/localization/Localization.cpp
    src/usage/UsageFormatting.cpp
    src/usage/UsageHistoryStore.cpp
    src/usage/UsageRefreshWorker.cpp
    src/tokens/CodexSessionScanner.cpp
    src/tokens/TokenUsageBuckets.cpp
    src/tokens/TokenKpiSummary.cpp
    src/tokens/TokenHeatmapData.cpp
    src/tokens/TokenUsageStore.cpp
    src/tokens/TokenUsageRefreshWorker.cpp
    src/activity/CodexActivityMonitor.cpp
    src/activity/WidgetActivityController.cpp
    src/activity/WidgetActivityText.cpp
    src/app/WidgetApp.cpp
    src/rendering/WidgetButton.cpp
    src/rendering/WidgetRenderColorPanel.cpp
    src/rendering/WidgetRenderPalette.cpp
    src/rendering/WidgetRenderProviderInfo.cpp
    src/rendering/WidgetRenderActivityVein.cpp
    src/rendering/WidgetRenderMotionPulse.cpp
    src/rendering/WidgetRenderMotionWave.cpp
    src/rendering/WidgetRenderResources.cpp
    src/rendering/WidgetRenderUsage.cpp
    src/color/WidgetColorPanel.cpp
    src/color/WidgetColorTools.cpp
    src/window/WidgetDocking.cpp
    src/window/WidgetColorPanelWindow.cpp
    src/glass/WidgetGlassSettings.cpp
    src/glass/WidgetGlassEffect.cpp
    src/glass/WidgetGlassEffectCapture.cpp
    src/glass/WidgetGlassEffectDistortion.cpp
    src/glass/WidgetGlassEffectGpuProcessor.cpp
    src/glass/WidgetGlassEffectRenderer.cpp
    src/rendering/WidgetRenderGlassEffect.cpp
    src/rendering/WidgetGlassElementLenses.cpp
    src/rendering/WidgetGraphData.cpp
    src/rendering/WidgetGraphFormatting.cpp
    src/rendering/WidgetGraphInfoIcon.cpp
    src/rendering/WidgetRenderQuotaGraph.cpp
    src/rendering/WidgetRenderGraphControls.cpp
    src/rendering/WidgetRenderKpiSummary.cpp
    src/rendering/WidgetRenderTokenGraph.cpp
    src/rendering/WidgetRenderTokenHeatmap.cpp
    src/rendering/WidgetTrayHideButton.cpp
    src/rendering/WidgetTokenGraphState.cpp
    src/rendering/WidgetGraphLayout.cpp
    src/rendering/WidgetGraphInteraction.cpp
    src/menu/WidgetMenu.cpp
    src/menu/WidgetMenuBanner.cpp
    src/menu/WidgetMenuBuilder.cpp
    src/menu/WidgetMenuCheckbox.cpp
    src/menu/WidgetMenuOwnerDraw.cpp
    src/menu/WidgetMenuRadio.cpp
    src/menu/WidgetMenuSlider.cpp
    src/menu/WidgetMenuToggleSubmenu.cpp
    src/shell/WidgetShellActions.cpp
    src/shell/WidgetShellMessages.cpp
    src/shell/WidgetScreenshotClipboard.cpp
    src/shell/WidgetTrayIcon.cpp
    src/shell/WidgetTrayTooltip.cpp
    src/rendering/WidgetRendering.cpp
    src/animation/WidgetQuotaGraphAnimation.cpp
    src/animation/WidgetRollingNumberAnimation.cpp
    src/app/WidgetClickThrough.cpp
    src/app/WidgetColorInteraction.cpp
    src/app/WidgetCommandHandler.cpp
    src/app/WidgetGlassEffectController.cpp
    src/app/WidgetGraphController.cpp
    src/app/WidgetTimers.cpp
    src/app/WidgetTrayHideButtonController.cpp
    src/app/WidgetActivityVeinController.cpp
    src/app/WidgetUsageAnimationController.cpp
    src/app/WidgetUsageController.cpp
    src/app/WidgetVisibility.cpp
    src/vibration/WidgetVibrationAnimation.cpp
    src/vibration/WidgetVibrationController.cpp
    src/vibration/WidgetVibrationGlassEffect.cpp
    src/vibration/WidgetVibrationMotion.cpp
    src/vibration/WidgetVibrationSettings.cpp
    src/window/WidgetWindow.cpp
    src/main.cpp
)

codex_deck_enable_cpp23(CodexDeck)

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
    dwmapi
    dwrite
    d3d11
    d3dcompiler
    dxgi
    user32
    winhttp
    shell32
    gdi32
    comdlg32
    uxtheme
    advapi32
    wtsapi32
    sqlite3
    nlohmann_json::nlohmann_json
)

if(MSVC)
    # Isole les fonctions et donnees avant l'optimisation globale Release.
    target_compile_options(sqlite3 PRIVATE
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
    )

    target_compile_options(CodexDeck PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/Gw>
    )

    # Elimine et fusionne le code inutilise uniquement dans l'executable livre.
    target_link_options(CodexDeck PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )

    if(CODEX_DECK_ENABLE_ASAN)
        target_compile_options(CodexDeck PRIVATE
            /fsanitize=address
            /Zi
        )
        target_link_options(CodexDeck PRIVATE
            /fsanitize=address
            /DEBUG
        )
    endif()
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/WidgetMotionEffectsTests.cpp")
    add_executable(WidgetMotionEffectsTests
        tests/WidgetMotionEffectsTests.cpp
        src/motion/WidgetMotionEffectsSettings.cpp
        src/motion/WidgetMotionCurves.cpp
        src/motion/WidgetMotionWaveField.cpp
        src/glass/WidgetGlassEffectDistortion.cpp
        src/glass/WidgetGlassEffectGpuProcessor.cpp
        src/glass/WidgetGlassSettings.cpp
        src/rendering/WidgetRenderMotionPulse.cpp
        src/rendering/WidgetRenderMotionWave.cpp
        src/vibration/WidgetVibrationAnimation.cpp
        src/vibration/WidgetVibrationGlassEffect.cpp
        src/vibration/WidgetVibrationSettings.cpp
    )
    codex_deck_enable_cpp23(WidgetMotionEffectsTests)
    target_include_directories(WidgetMotionEffectsTests PRIVATE src)
    target_compile_definitions(WidgetMotionEffectsTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(WidgetMotionEffectsTests PRIVATE d2d1 d3d11 d3dcompiler gdi32 user32)
    if(MSVC)
        target_compile_options(WidgetMotionEffectsTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()
    add_test(NAME WidgetMotionEffectsTests COMMAND WidgetMotionEffectsTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/WidgetVibrationControllerTests.cpp")
    add_executable(WidgetVibrationControllerTests
        tests/WidgetVibrationControllerTests.cpp
        src/motion/WidgetMotionEffectsSettings.cpp
        src/vibration/WidgetVibrationController.cpp
        src/vibration/WidgetVibrationSettings.cpp
    )
    codex_deck_enable_cpp23(WidgetVibrationControllerTests)
    target_include_directories(WidgetVibrationControllerTests PRIVATE src)
    target_compile_definitions(WidgetVibrationControllerTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(WidgetVibrationControllerTests PRIVATE user32)
    if(MSVC)
        target_compile_options(WidgetVibrationControllerTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()
    add_test(NAME WidgetVibrationControllerTests COMMAND WidgetVibrationControllerTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CodexUsageTests.cpp")
    add_executable(CodexUsageTests
        tests/CodexUsageTests.cpp
        src/usage/UsageProviderFactory.cpp
        src/usage/UsageHistoryStore.cpp
        src/usage/UsageRefreshWorker.cpp
        src/providers/codex/CodexAuthReader.cpp
        src/providers/codex/CodexUsageClient.cpp
        src/providers/codex/CodexUsageParser.cpp
        src/providers/codex/CodexUsageProvider.cpp
        src/tokens/CodexSessionScanner.cpp
        src/tokens/TokenUsageBuckets.cpp
        src/tokens/TokenHeatmapData.cpp
        src/tokens/TokenUsageStore.cpp
        src/tokens/TokenUsageRefreshWorker.cpp
    )
    codex_deck_enable_cpp23(CodexUsageTests)
    target_include_directories(CodexUsageTests PRIVATE src)
    target_compile_definitions(CodexUsageTests PRIVATE
        CODEX_DECK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(CodexUsageTests PRIVATE
        nlohmann_json::nlohmann_json
        sqlite3
        winhttp
        user32
    )
    if(MSVC)
        target_compile_options(CodexUsageTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()
    add_test(NAME CodexUsageTests COMMAND CodexUsageTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/WidgetUiTests.cpp")
    add_executable(WidgetUiTests
        tests/WidgetUiTests.cpp
        src/resources/CodexDeck.rc
        src/localization/Localization.cpp
        src/color/WidgetColorPanel.cpp
        src/color/WidgetColorTools.cpp
        src/usage/UsageFormatting.cpp
        src/rendering/WidgetGraphData.cpp
        src/rendering/WidgetGraphFormatting.cpp
        src/rendering/WidgetGraphInteraction.cpp
        src/rendering/WidgetTokenGraphState.cpp
        src/rendering/WidgetGraphLayout.cpp
        src/rendering/WidgetRenderProviderInfo.cpp
        src/rendering/WidgetRenderUsage.cpp
        src/animation/WidgetQuotaGraphAnimation.cpp
        src/animation/WidgetRollingNumberAnimation.cpp
    )
    codex_deck_enable_cpp23(WidgetUiTests)
    target_include_directories(WidgetUiTests PRIVATE src)
    target_compile_definitions(WidgetUiTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(WidgetUiTests PRIVATE comdlg32 d2d1 dwrite user32)
    if(MSVC)
        target_compile_options(WidgetUiTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()
    add_test(NAME WidgetUiTests COMMAND WidgetUiTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/WindowsStartupTests.cpp")
    add_executable(WindowsStartupTests
        tests/WindowsStartupTests.cpp
        src/startup/WindowsStartup.cpp
    )
    codex_deck_enable_cpp23(WindowsStartupTests)
    target_include_directories(WindowsStartupTests PRIVATE src)
    target_compile_definitions(WindowsStartupTests PRIVATE
        CODEX_DECK_STARTUP_TEST_REGISTRY
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(WindowsStartupTests PRIVATE advapi32)
    if(MSVC)
        target_compile_options(WindowsStartupTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()
    add_test(NAME WindowsStartupTests COMMAND WindowsStartupTests)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/SingleInstanceTests.cpp")
    add_executable(SingleInstanceTests
        tests/SingleInstanceTests.cpp
        src/win32/SingleInstance.cpp
    )
    codex_deck_enable_cpp23(SingleInstanceTests)
    target_include_directories(SingleInstanceTests PRIVATE src)
    target_compile_definitions(SingleInstanceTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    if(MSVC)
        target_compile_options(SingleInstanceTests PRIVATE /W4 /permissive- /Zc:__cplusplus)
    endif()
    add_test(NAME SingleInstanceTests COMMAND SingleInstanceTests)
endif()

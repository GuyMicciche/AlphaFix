#==============================================================================
# AlphaFix — CMake Build Configuration
#
# Builds the AlphaFix After Effects SmartFX plugin with FFmpeg integration.
#
# Prerequisites:
#   - After Effects SDK (set AE_SDK_PATH)
#   - FFmpeg dev libraries (set FFMPEG_PATH)
#   - Visual Studio 2022 (Windows)
#
# Build (from project root):
#   cmake -B build -S . -G "Visual Studio 17 2022" -A x64 ^
#       -DAE_SDK_PATH="C:/path/to/AfterEffectsSDK" ^
#       -DFFMPEG_PATH="C:/path/to/ffmpeg-shared"
#   cmake --build build --config Release
#
#==============================================================================

cmake_minimum_required(VERSION 3.20)
project(AlphaFix VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── User-configurable paths ──────────────────────────────────────────────────

set(AE_SDK_PATH "" CACHE PATH "Path to After Effects SDK root")
set(FFMPEG_PATH "" CACHE PATH "Path to FFmpeg dev installation (include/ and lib/)")

if(NOT AE_SDK_PATH)
    message(FATAL_ERROR "AE_SDK_PATH not set.")
endif()

if(NOT FFMPEG_PATH)
    message(FATAL_ERROR "FFMPEG_PATH not set.")
endif()

# ── Validate SDK path ────────────────────────────────────────────────────────

if(NOT EXISTS "${AE_SDK_PATH}/Headers/AE_Effect.h")
    message(FATAL_ERROR "AE_Effect.h not found at ${AE_SDK_PATH}/Headers/")
endif()

if(NOT EXISTS "${FFMPEG_PATH}/include/libavformat/avformat.h")
    message(FATAL_ERROR "FFmpeg headers not found at ${FFMPEG_PATH}/include/")
endif()

# ── AE SDK include directories ───────────────────────────────────────────────
# Try both standard SDK layouts (flat and Examples-based)

set(AE_SDK_INCLUDES
    "${AE_SDK_PATH}/Headers"
    "${AE_SDK_PATH}/Headers/SP"
    "${AE_SDK_PATH}/Headers/adobesdk"
    "${AE_SDK_PATH}/Util"
    "${AE_SDK_PATH}/Resources"
)

# Some SDKs put things under Examples/
if(EXISTS "${AE_SDK_PATH}/Examples/Headers")
    list(APPEND AE_SDK_INCLUDES
        "${AE_SDK_PATH}/Examples/Headers"
        "${AE_SDK_PATH}/Examples/Headers/SP"
        "${AE_SDK_PATH}/Examples/Resources"
        "${AE_SDK_PATH}/Examples/Util"
    )
endif()

# ── Source files ──────────────────────────────────────────────────────────────

set(SOURCES
    src/AlphaFix.cpp
    src/AlphaFixDecoder.cpp
)

set(HEADERS
    src/AlphaFix.h
    src/AlphaFixDecoder.h
)

# AE SDK utility sources
set(AE_UTIL_SOURCES "")
foreach(UTIL_FILE "AEGP_SuiteHandler.cpp" "MissingSuiteError.cpp")
    if(EXISTS "${AE_SDK_PATH}/Util/${UTIL_FILE}")
        list(APPEND AE_UTIL_SOURCES "${AE_SDK_PATH}/Util/${UTIL_FILE}")
    elseif(EXISTS "${AE_SDK_PATH}/Examples/Util/${UTIL_FILE}")
        list(APPEND AE_UTIL_SOURCES "${AE_SDK_PATH}/Examples/Util/${UTIL_FILE}")
    endif()
endforeach()

# ══════════════════════════════════════════════════════════════════════════════
# PiPL Resource Pipeline (Windows)
#
# The AE SDK uses a custom resource format called PiPL.
# Pipeline: .r → (cl /EP) → .rr → (PiPLtool) → .rc → (rc) → .res
#
# NOTE: PiPLtool may omit commas in the generated .rc for some properties.
# After generation, the build will proceed — if AE still has PiPL issues,
# manually fix the commas in build/AlphaFix_PiPL.rc (add comma after "4L"
# on lines containing eVER, eINF, and aeFL properties).
# ══════════════════════════════════════════════════════════════════════════════

set(PIPL_RC_FILE "")

if(WIN32)
    # Find PiPLtool.exe
    find_file(PIPL_TOOL_EXE
        NAMES "PiPLtool.exe" "PiPLTool.exe"
        PATHS
            "${AE_SDK_PATH}/Resources"
            "${AE_SDK_PATH}/Examples/Resources"
            "${AE_SDK_PATH}/Util"
        NO_DEFAULT_PATH
    )
    if(NOT PIPL_TOOL_EXE)
        file(GLOB_RECURSE PIPL_SEARCH "${AE_SDK_PATH}/*PiPLtool*")
        if(PIPL_SEARCH)
            list(GET PIPL_SEARCH 0 PIPL_TOOL_EXE)
        endif()
    endif()

    if(NOT PIPL_TOOL_EXE)
        message(FATAL_ERROR "PiPLtool.exe not found in AE SDK at ${AE_SDK_PATH}")
    endif()
    message(STATUS "PiPLtool: ${PIPL_TOOL_EXE}")

    # Build include paths for cl /EP
    set(PIPL_INCLUDE_ARGS "")
    foreach(INC_DIR ${AE_SDK_INCLUDES})
        if(EXISTS "${INC_DIR}")
            list(APPEND PIPL_INCLUDE_ARGS "/I" "${INC_DIR}")
        endif()
    endforeach()

    set(PIPL_R_INPUT   "${CMAKE_SOURCE_DIR}/src/AlphaFix_PiPL.r")
    set(PIPL_RR_FILE   "${CMAKE_BINARY_DIR}/AlphaFix_PiPL.rr")
    set(PIPL_RC_OUTPUT "${CMAKE_BINARY_DIR}/AlphaFix_PiPL.rc")

    # Step 1: Preprocess .r → .rr
    add_custom_command(
        OUTPUT "${PIPL_RR_FILE}"
        COMMAND cl /EP /DWIN32 /D_WIN64 /DAE_OS_WIN /DMSWINDOWS
            ${PIPL_INCLUDE_ARGS}
            "${PIPL_R_INPUT}"
            > "${PIPL_RR_FILE}"
        DEPENDS "${PIPL_R_INPUT}"
        COMMENT "[PiPL] Preprocessing .r -> .rr"
        VERBATIM
    )

    # Step 2: Convert .rr → .rc
    add_custom_command(
        OUTPUT "${PIPL_RC_OUTPUT}"
        COMMAND "${PIPL_TOOL_EXE}" "${PIPL_RR_FILE}" "${PIPL_RC_OUTPUT}"
        DEPENDS "${PIPL_RR_FILE}"
        COMMENT "[PiPL] Generating .rc from .rr"
        VERBATIM
    )

    set(PIPL_RC_FILE "${PIPL_RC_OUTPUT}")
endif()


# ══════════════════════════════════════════════════════════════════════════════
# Build Target
# ══════════════════════════════════════════════════════════════════════════════

if(WIN32)
    add_library(AlphaFix SHARED
        ${SOURCES}
        ${HEADERS}
        ${AE_UTIL_SOURCES}
        ${PIPL_RC_FILE}
        src/AlphaFix.def
    )

    set_target_properties(AlphaFix PROPERTIES
        SUFFIX ".aex"
        PREFIX ""
    )

    # Linker options: .def file + delay-load FFmpeg DLLs
    target_link_options(AlphaFix PRIVATE
        "/DEF:${CMAKE_SOURCE_DIR}/src/AlphaFix.def"
        "/DELAYLOAD:avformat-62.dll"
        "/DELAYLOAD:avcodec-62.dll"
        "/DELAYLOAD:avutil-60.dll"
        "/DELAYLOAD:swscale-9.dll"
        "/DELAYLOAD:swresample-6.dll"
    )

    target_compile_definitions(AlphaFix PRIVATE
        _WINDOWS
        _WIN64
        WIN32
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        MSWindows
    )

    target_link_libraries(AlphaFix PRIVATE
        "${FFMPEG_PATH}/lib/avformat.lib"
        "${FFMPEG_PATH}/lib/avcodec.lib"
        "${FFMPEG_PATH}/lib/avutil.lib"
        "${FFMPEG_PATH}/lib/swscale.lib"
        delayimp.lib
    )

    if(MSVC)
        target_compile_options(AlphaFix PRIVATE
            /W3 /MP /Zi /EHsc
        )
    endif()

elseif(APPLE)
    add_library(AlphaFix MODULE
        ${SOURCES}
        ${HEADERS}
        ${AE_UTIL_SOURCES}
    )

    set_target_properties(AlphaFix PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        PREFIX ""
    )

    target_compile_definitions(AlphaFix PRIVATE __MACH__)

    find_library(AVFORMAT_LIB avformat PATHS "${FFMPEG_PATH}/lib" NO_DEFAULT_PATH)
    find_library(AVCODEC_LIB avcodec PATHS "${FFMPEG_PATH}/lib" NO_DEFAULT_PATH)
    find_library(AVUTIL_LIB avutil PATHS "${FFMPEG_PATH}/lib" NO_DEFAULT_PATH)
    find_library(SWSCALE_LIB swscale PATHS "${FFMPEG_PATH}/lib" NO_DEFAULT_PATH)

    target_link_libraries(AlphaFix PRIVATE
        ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB}
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework VideoToolbox"
    )
endif()

# ── Include directories ───────────────────────────────────────────────────────

target_include_directories(AlphaFix PRIVATE
    ${AE_SDK_INCLUDES}
    "${FFMPEG_PATH}/include"
    "${CMAKE_SOURCE_DIR}/src"
)


# ── User-configurable AE app path ─────────────────────────────────────────────

if(WIN32)
    set(AE_BASE_DIR
        "C:/Program Files/Adobe/Adobe After Effects 2026"
        CACHE PATH "Base After Effects application directory"
    )
else()
    set(AE_BASE_DIR
        "/Applications/Adobe After Effects 2026"
        CACHE PATH "Base After Effects application directory"
    )
endif()

# ── Include directories ───────────────────────────────────────────────────────

target_include_directories(AlphaFix PRIVATE
    ${AE_SDK_INCLUDES}
    "${FFMPEG_PATH}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

# ══════════════════════════════════════════════════════════════════════════════
# Runtime layout / install destinations
# ══════════════════════════════════════════════════════════════════════════════

if(WIN32)
    set(REQUIRED_FFMPEG_DLLS
        "${FFMPEG_PATH}/bin/avutil-60.dll"
        "${FFMPEG_PATH}/bin/swresample-6.dll"
        "${FFMPEG_PATH}/bin/avcodec-62.dll"
        "${FFMPEG_PATH}/bin/avformat-62.dll"
        "${FFMPEG_PATH}/bin/swscale-9.dll"
    )

    set(AE_SUPPORT_FILES_DIR
        "${AE_BASE_DIR}/Support Files"
        CACHE PATH "After Effects Support Files directory"
    )
    set(AE_PLUGIN_INSTALL_DIR
        "${AE_SUPPORT_FILES_DIR}/Plug-ins/AlphaFix"
        CACHE PATH "AE-specific AlphaFix plugin install directory"
    )
    set(AE_DLL_INSTALL_DIR
        "${AE_SUPPORT_FILES_DIR}"
        CACHE PATH "Directory for required FFmpeg DLLs"
    )

    foreach(REQUIRED_DLL ${REQUIRED_FFMPEG_DLLS})
        if(NOT EXISTS "${REQUIRED_DLL}")
            message(FATAL_ERROR "Required FFmpeg DLL not found: ${REQUIRED_DLL}")
        endif()
    endforeach()

    set(STAGING_RELEASE_DIR
        "${CMAKE_SOURCE_DIR}/Release"
        CACHE PATH "Root-level staging folder for packaged plugin files"
    )

    set(STAGING_RELEASE_DIR
        "${CMAKE_SOURCE_DIR}/Release"
        CACHE PATH "Root-level staging folder"
    )

    set(STAGING_SUPPORT_FILES_DIR
        "${STAGING_RELEASE_DIR}/Support Files"
    )

    set(STAGING_PLUGIN_DIR
        "${STAGING_SUPPORT_FILES_DIR}/Plug-ins/AlphaFix"
    )

    add_custom_command(TARGET AlphaFix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGING_SUPPORT_FILES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGING_PLUGIN_DIR}"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:AlphaFix>"
            "${STAGING_PLUGIN_DIR}/"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${REQUIRED_FFMPEG_DLLS}
            "${STAGING_SUPPORT_FILES_DIR}/"

        COMMENT "Staging AE-style Support Files folder with AlphaFix.aex and FFmpeg DLLs"
        COMMAND_EXPAND_LISTS
    )

    add_custom_command(TARGET AlphaFix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_SOURCE_DIR}/Release_v${PROJECT_VERSION}.zip" --format=zip "."
        WORKING_DIRECTORY "${STAGING_RELEASE_DIR}"
        COMMENT "Creating Release_v${PROJECT_VERSION}.zip"
    )

elseif(APPLE)
    set(AE_PLUGIN_INSTALL_DIR
        "${AE_BASE_DIR}/Plug-ins/AlphaFix"
        CACHE PATH "AE-specific AlphaFix plugin install directory"
    )
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Install
# ══════════════════════════════════════════════════════════════════════════════

install(TARGETS AlphaFix
    LIBRARY DESTINATION "${AE_PLUGIN_INSTALL_DIR}"
    RUNTIME DESTINATION "${AE_PLUGIN_INSTALL_DIR}"
    BUNDLE  DESTINATION "${AE_PLUGIN_INSTALL_DIR}"
)

if(WIN32)
    install(FILES ${REQUIRED_FFMPEG_DLLS} DESTINATION "${AE_DLL_INSTALL_DIR}")
endif()


# ══════════════════════════════════════════════════════════════════════════════
# Status
# ══════════════════════════════════════════════════════════════════════════════

message(STATUS "")
message(STATUS "=== AlphaFix Plugin Config ===")
message(STATUS "  AE SDK:     ${AE_SDK_PATH}")
message(STATUS "  FFmpeg:     ${FFMPEG_PATH}")
message(STATUS "  Install:    ${AE_PLUGIN_INSTALL_DIR}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Generator:  ${CMAKE_GENERATOR}")
if(PIPL_TOOL_EXE)
    message(STATUS "  PiPLtool:   ${PIPL_TOOL_EXE}")
endif()
message(STATUS "==============================")
message(STATUS "")
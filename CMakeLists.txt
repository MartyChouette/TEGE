cmake_minimum_required(VERSION 3.20)
project(EnjinEngine VERSION 0.9.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use static MSVC runtime (/MT instead of /MD) so users don't need VC++ Redistributable
# This must be set before any add_subdirectory() or target definitions so it propagates
# to all targets including third-party libraries (imgui, Jolt, Box2D, assimp, etc.)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Release builds carry full debug info (PDBs) for OUR code. The editor writes a
# minidump on crash (enjin_crash.dmp) but without symbols every user crash report
# has been an unreadable RVA. /Zi + /DEBUG costs nothing at runtime; the PDB just
# has to ship alongside dev builds (it is NOT packaged into the distribution zip).
#
# This is opt-in per target rather than a blanket add_compile_options, which
# applied to every target added afterwards: the vendored libraries (whose PDBs
# nobody has ever symbolized) and all 113 test executables, each of which wrote
# a PDB spanning the whole engine archive. That was gigabytes of disk and link
# time per build for symbols no one reads. Apply ENJIN_PDB_* to a target when a
# user could be holding the binary when it crashes.
if(MSVC)
    set(ENJIN_PDB_COMPILE_OPTIONS "$<$<CONFIG:Release>:/Zi>" "$<$<CONFIG:Release>:/FS>")
    set(ENJIN_PDB_LINK_OPTIONS "$<$<CONFIG:Release>:/DEBUG>")
    # /OPT:REF and /OPT:ICF below can only strip at the granularity the compiler
    # gives them. Without /Gy (per-function COMDATs) and /Gw (per-datum COMDATs)
    # most unreferenced code and data is welded to whatever else shares its
    # section and survives the link. /Zc:inline discards unreferenced
    # internal-linkage COMDATs at compile time, which is exactly the shape of
    # the static shader-byte arrays that headers paste into every includer.
    add_compile_options("$<$<CONFIG:Release>:/Gy>" "$<$<CONFIG:Release>:/Gw>" /Zc:inline)
    add_link_options("$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT ENJIN_PLATFORM_WEB)
    # Same intent on GCC and Clang, which had no equivalent: -g so a Release
    # binary carries line information, and -rdynamic so the crash handler's
    # backtrace resolves to function names instead of bare addresses. Applied to
    # the same four targets as the MSVC pair, not at root scope.
    set(ENJIN_PDB_COMPILE_OPTIONS "$<$<CONFIG:Release>:-g>")
    set(ENJIN_PDB_LINK_OPTIONS "$<$<CONFIG:Release>:-rdynamic>")
endif()

# Two warnings, chosen because they catch bug classes this codebase has actually
# shipped, and because the tree is clean for both today so they stay meaningful.
#
#   -Wswitch / C4062: a switch over an enum with no default that misses an
#     enumerator. This found three dialogue node types absent from both
#     exporters, which silently truncated every tree containing one, and a
#     depth-cubemap binding translating to the wrong Vulkan descriptor type.
#   -Wunused-result: an ignored return that the callee marked as needing a
#     check. This found posix_memalign's result being dropped.
#
# NOT enabled: -Wswitch-enum, which fires on every switch that legitimately uses
# a default (1213 of them here), and -Wmissing-field-initializers (124). Both are
# noise at this size and would train people to ignore the output.
if(MSVC)
    add_compile_options(/w14062)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wswitch -Wunused-result)
endif()

# Sanitizer options (for development builds)
option(ENJIN_ENABLE_ASAN  "Enable AddressSanitizer"                        OFF)
option(ENJIN_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (GCC/Clang)"  OFF)
option(ENJIN_ENABLE_TSAN  "Enable ThreadSanitizer (GCC/Clang)"            OFF)

if(ENJIN_ENABLE_ASAN AND ENJIN_ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be used together — they are incompatible.")
endif()

if(ENJIN_ENABLE_ASAN OR ENJIN_ENABLE_UBSAN OR ENJIN_ENABLE_TSAN)
    if(MSVC)
        # MSVC only supports ASan; UBSan and TSan are not available
        if(ENJIN_ENABLE_ASAN)
            add_compile_options(/fsanitize=address)
            message(STATUS "ASan enabled (MSVC)")
        endif()
        if(ENJIN_ENABLE_UBSAN)
            message(WARNING "UBSan is not supported on MSVC — ignoring ENJIN_ENABLE_UBSAN")
        endif()
        if(ENJIN_ENABLE_TSAN)
            message(WARNING "TSan is not supported on MSVC — ignoring ENJIN_ENABLE_TSAN")
        endif()
    else()
        # GCC / Clang
        add_compile_options(-fno-omit-frame-pointer)
        if(ENJIN_ENABLE_ASAN)
            add_compile_options(-fsanitize=address)
            add_link_options(-fsanitize=address)
            message(STATUS "ASan enabled (GCC/Clang)")
        endif()
        if(ENJIN_ENABLE_UBSAN)
            add_compile_options(-fsanitize=undefined)
            add_link_options(-fsanitize=undefined)
            message(STATUS "UBSan enabled (GCC/Clang)")
        endif()
        if(ENJIN_ENABLE_TSAN)
            add_compile_options(-fsanitize=thread)
            add_link_options(-fsanitize=thread)
            message(STATUS "TSan enabled (GCC/Clang)")
        endif()
    endif()
endif()

# WebAssembly / Emscripten support
option(ENJIN_PLATFORM_WEB "Build for WebAssembly with WebGPU (requires Emscripten)" OFF)

if(ENJIN_PLATFORM_WEB OR EMSCRIPTEN)
    set(ENJIN_PLATFORM_WEB ON CACHE BOOL "" FORCE)
    include(${CMAKE_SOURCE_DIR}/cmake/EmscriptenToolchain.cmake)
endif()

# Platform detection
if(ENJIN_PLATFORM_WEB)
    message(STATUS "Building for Web (Emscripten + WebGPU)")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building for Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building for Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Building for macOS")
endif()

# Build options
option(ENJIN_BUILD_EDITOR "Build the Enjin Editor" ON)
option(ENJIN_BUILD_PLAYER "Build the standalone game player" ON)
option(ENJIN_BUILD_HUB "Build the Enjin Hub launcher" OFF)
option(ENJIN_BUILD_TESTS "Build unit tests" OFF)
option(ENJIN_BUILD_EXAMPLES "Build example projects" OFF)

# One-shot browser verification of the WebGPU GPU-compute path. When ON, the web
# player runs a compute dispatch at startup and logs "[compute-smoke] PASS/FAIL"
# to the console. Off by default so shipping builds stay clean.
option(ENJIN_WEBGPU_COMPUTE_SMOKETEST "Run the WebGPU compute smoke test at web startup" OFF)
if(ENJIN_WEBGPU_COMPUTE_SMOKETEST)
    add_compile_definitions(ENJIN_WEBGPU_COMPUTE_SMOKETEST)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/Engine/include
    ${CMAKE_SOURCE_DIR}/Core/include
)

# Add subdirectories
add_subdirectory(Core)
add_subdirectory(third_party/imgui)
add_subdirectory(third_party/imguizmo)
add_subdirectory(third_party/angelscript)
add_subdirectory(third_party/nanosvg)
add_subdirectory(Engine)

if(ENJIN_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()

if(ENJIN_BUILD_PLAYER)
    add_subdirectory(Player)
endif()

if(ENJIN_BUILD_HUB)
    add_subdirectory(Hub)
endif()

if(ENJIN_BUILD_EXAMPLES)
    # Examples output to bin/Examples/ to keep the main bin/ directory clean
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Examples)
    add_subdirectory(Examples)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

if(ENJIN_BUILD_TESTS)
    enable_testing()
    # Tests output to bin/Tests/ to keep the main bin/ directory clean
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Tests)
    add_subdirectory(Tests)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# =============================================================================
# Binary Distribution - Install Rules
# =============================================================================

# Install editor, player, and hub executables
if(ENJIN_BUILD_EDITOR)
    install(TARGETS EnjinEditor RUNTIME DESTINATION bin)
endif()
if(ENJIN_BUILD_PLAYER)
    install(TARGETS EnjinPlayer RUNTIME DESTINATION bin)
endif()
if(ENJIN_BUILD_HUB)
    install(TARGETS EnjinHub RUNTIME DESTINATION bin)
endif()

# Install shader files
install(DIRECTORY Engine/shaders/ DESTINATION share/enjin/shaders
    FILES_MATCHING PATTERN "*.vert" PATTERN "*.frag" PATTERN "*.comp" PATTERN "*.glsl"
    PATTERN "*.spv" PATTERN "*.rgen" PATTERN "*.rmiss" PATTERN "*.rchit")

# Install documentation
install(DIRECTORY docs/ DESTINATION share/doc/enjin
    FILES_MATCHING PATTERN "*.md")

# Install the shipped templates. These ARE the built-in templates now, not code:
# the editor reads them from disk and only falls back to a generator when the
# folder is missing, so an install without them has no templates at all.
#
# Deliberately not named "templates": that name beside the exe is the USER's
# template folder, which the editor scans separately.
if(EXISTS "${CMAKE_SOURCE_DIR}/builtin_templates")
    install(DIRECTORY builtin_templates/ DESTINATION share/enjin/builtin_templates)
endif()

# A user template folder, if one was shipped alongside.
if(EXISTS "${CMAKE_SOURCE_DIR}/templates")
    install(DIRECTORY templates/ DESTINATION share/enjin/templates)
endif()

# Install third-party font directory
if(EXISTS "${CMAKE_SOURCE_DIR}/fonts")
    install(DIRECTORY fonts/ DESTINATION share/enjin/fonts)
endif()

# =============================================================================
# CPack Configuration
# =============================================================================

set(CPACK_PACKAGE_NAME "Enjin")
set(CPACK_PACKAGE_VENDOR "Enjin Engine")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Enjin Game Engine - C++20 Vulkan-powered game engine with editor")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Enjin")

# Platform-specific generators
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_PACKAGE_FILE_NAME "enjin-${PROJECT_VERSION}-win64")

    # --- NSIS Installer Configuration ---
    set(CPACK_NSIS_DISPLAY_NAME "Enjin Engine ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "Enjin Engine")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://enjin.dev")
    set(CPACK_NSIS_HELP_LINK "https://enjin.dev/docs")
    set(CPACK_NSIS_CONTACT "support@enjin.dev")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    # Install to Program Files/Enjin by default
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")

    # Start Menu shortcuts
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Enjin Editor.lnk' '$INSTDIR\\\\bin\\\\EnjinEditor.exe'
         CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Enjin Player.lnk' '$INSTDIR\\\\bin\\\\EnjinPlayer.exe'
         CreateShortCut '$DESKTOP\\\\Enjin Editor.lnk' '$INSTDIR\\\\bin\\\\EnjinEditor.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Enjin Editor.lnk'
         Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Enjin Player.lnk'
         Delete '$DESKTOP\\\\Enjin Editor.lnk'")

    # File associations (.enjin project files, .enjpak asset packs)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.enjin' '' 'EnjinScene'
        WriteRegStr HKCR 'EnjinScene' '' 'Enjin Scene File'
        WriteRegStr HKCR 'EnjinScene\\\\shell\\\\open\\\\command' '' '$INSTDIR\\\\bin\\\\EnjinEditor.exe \\\"%1\\\"'
        WriteRegStr HKCR '.enjpak' '' 'EnjinAssetPack'
        WriteRegStr HKCR 'EnjinAssetPack' '' 'Enjin Asset Pack'
        WriteRegStr HKCR 'EnjinAssetPack\\\\shell\\\\open\\\\command' '' '$INSTDIR\\\\bin\\\\EnjinPlayer.exe \\\"%1\\\"'
        System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, p 0, p 0)'
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.enjin'
        DeleteRegKey HKCR 'EnjinScene'
        DeleteRegKey HKCR '.enjpak'
        DeleteRegKey HKCR 'EnjinAssetPack'
        System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, p 0, p 0)'
    ")

    # Use icon if available
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/enjin.ico")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/enjin.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/enjin.ico")
    endif()

    # Installer branding
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/installer_banner.bmp")
        set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/resources/installer_banner.bmp")
    endif()

elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "enjin-${PROJECT_VERSION}-macos")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_PACKAGE_FILE_NAME "enjin-${PROJECT_VERSION}-linux-x64")

    # DEB package metadata
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Enjin Engine")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libvulkan1, libglfw3 (>= 3.3)")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://enjin.dev")
endif()

# Resource directory for distribution assets (icons, banners)
if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()

include(CPack)

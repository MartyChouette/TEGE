cmake_minimum_required(VERSION 3.20)
project(EnjinEngine VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building for Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building for Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Building for macOS")
endif()

# Build options
option(ENJIN_BUILD_EDITOR "Build the Enjin Editor" ON)
option(ENJIN_BUILD_PLAYER "Build the standalone game player" ON)
option(ENJIN_BUILD_TESTS "Build unit tests" OFF)
option(ENJIN_BUILD_EXAMPLES "Build example projects" OFF)

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

if(ENJIN_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

if(ENJIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# =============================================================================
# Binary Distribution - Install Rules
# =============================================================================

# Install editor and player executables
if(ENJIN_BUILD_EDITOR)
    install(TARGETS EnjinEditor RUNTIME DESTINATION bin)
endif()
if(ENJIN_BUILD_PLAYER)
    install(TARGETS EnjinPlayer RUNTIME DESTINATION bin)
endif()

# Install shader files
install(DIRECTORY Engine/shaders/ DESTINATION share/enjin/shaders
    FILES_MATCHING PATTERN "*.vert" PATTERN "*.frag" PATTERN "*.comp" PATTERN "*.glsl"
    PATTERN "*.spv" PATTERN "*.rgen" PATTERN "*.rmiss" PATTERN "*.rchit")

# Install documentation
install(DIRECTORY docs/ DESTINATION share/doc/enjin
    FILES_MATCHING PATTERN "*.md")

# Install project templates directory
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
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_FILE_NAME "enjin-${PROJECT_VERSION}-win64")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "enjin-${PROJECT_VERSION}-macos")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME "enjin-${PROJECT_VERSION}-linux-x64")
endif()

include(CPack)

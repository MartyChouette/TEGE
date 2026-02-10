cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# Fetch nlohmann/json for scene serialization
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# Fetch Assimp for FBX and other model format support
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(assimp)

# Fetch miniaudio (header-only, public domain audio library)
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniaudio)

# Engine library - Rendering, ECS, Asset system, etc.
file(GLOB_RECURSE ENGINE_SOURCES
    "src/**/*.cpp"
    "include/**/*.h"
    "include/**/*.hpp"
)

add_library(EnjinEngine STATIC ${ENGINE_SOURCES})

target_include_directories(EnjinEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${miniaudio_SOURCE_DIR}>
)

target_link_libraries(EnjinEngine PUBLIC
    EnjinCore
    imgui
    imguizmo
    angelscript
    nanosvg
    nlohmann_json::nlohmann_json
    assimp::assimp
)

target_compile_features(EnjinEngine PUBLIC cxx_std_20)

# Link Winsock2 for networking on Windows
if(WIN32)
    target_link_libraries(EnjinEngine PUBLIC ws2_32)
endif()

# EditorLayer.cpp exceeds the default MSVC section limit due to heavy template use
if(MSVC)
    target_compile_options(EnjinEngine PRIVATE /bigobj)
endif()

# Find Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(EnjinEngine PUBLIC Vulkan::Vulkan)

# GLFW is already linked via EnjinCore, no need to link again
# But we can verify it's available
if(TARGET glfw)
    message(STATUS "GLFW found via EnjinCore")
elseif(TARGET glfw3)
    message(STATUS "GLFW3 found")
else()
    # GLFW should be found by Core, but check anyway
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        target_link_libraries(EnjinEngine PUBLIC glfw)
    endif()
endif()

# Optional: Find shaderc for shader compilation
# find_package(shaderc REQUIRED)
# target_link_libraries(EnjinEngine PUBLIC shaderc::shaderc)

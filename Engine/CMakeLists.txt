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

# Platform-specific networking and system libraries
if(WIN32)
    target_link_libraries(EnjinEngine PUBLIC ws2_32 winhttp)
elseif(UNIX)
    # Linux/macOS: POSIX sockets are in libc (no extra link needed),
    # but we need libcurl for HTTP client functionality
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(EnjinEngine PUBLIC CURL::libcurl)
        target_compile_definitions(EnjinEngine PUBLIC ENJIN_HAS_CURL)
        message(STATUS "Found libcurl for HTTP client")
    else()
        message(STATUS "libcurl not found — HTTP client features disabled")
    endif()
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

# Jolt Physics backend (3D) — ON by default
option(ENJIN_PHYSICS_JOLT "Enable Jolt Physics backend for 3D physics" ON)
if(ENJIN_PHYSICS_JOLT)
    set(JOLT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.2.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(JoltPhysics)
    target_link_libraries(EnjinEngine PUBLIC Jolt)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_PHYSICS_JOLT)
    message(STATUS "Jolt Physics enabled")
endif()

# Box2D backend (2D) — ON by default
option(ENJIN_PHYSICS_BOX2D "Enable Box2D v3 backend for 2D physics" ON)
if(ENJIN_PHYSICS_BOX2D)
    set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        box2d
        GIT_REPOSITORY https://github.com/erincatto/box2d.git
        GIT_TAG v3.0.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(box2d)
    target_link_libraries(EnjinEngine PUBLIC box2d)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_PHYSICS_BOX2D)
    message(STATUS "Box2D v3 enabled")
endif()


# Intel Open Image Denoise — optional alternative RT denoiser (OFF by default, requires OIDN install)
option(ENJIN_RAYTRACING_OIDN "Enable Intel OIDN denoiser backend for ray tracing" OFF)
if(ENJIN_RAYTRACING_OIDN)
    find_package(OpenImageDenoise QUIET)
    if(OpenImageDenoise_FOUND)
        target_link_libraries(EnjinEngine PUBLIC OpenImageDenoise)
        target_compile_definitions(EnjinEngine PUBLIC ENJIN_RAYTRACING_OIDN)
        message(STATUS "Intel OIDN denoiser enabled")
    else()
        message(WARNING "OIDN not found — install Intel OIDN and set OpenImageDenoise_DIR, or disable ENJIN_RAYTRACING_OIDN")
    endif()
endif()

# Optional: Steam integration (cloud saves, achievements)
option(ENJIN_STEAM "Enable Steam integration (requires Steamworks SDK)" OFF)
if(ENJIN_STEAM)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_STEAM)
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/steamworks/sdk/public/steam")
        target_include_directories(EnjinEngine PUBLIC
            "${CMAKE_SOURCE_DIR}/third_party/steamworks/sdk/public")
        if(WIN32)
            target_link_libraries(EnjinEngine PUBLIC
                "${CMAKE_SOURCE_DIR}/third_party/steamworks/sdk/redistributable_bin/win64/steam_api64.lib")
        elseif(APPLE)
            target_link_libraries(EnjinEngine PUBLIC
                "${CMAKE_SOURCE_DIR}/third_party/steamworks/sdk/redistributable_bin/osx/libsteam_api.dylib")
        else()
            target_link_libraries(EnjinEngine PUBLIC
                "${CMAKE_SOURCE_DIR}/third_party/steamworks/sdk/redistributable_bin/linux64/libsteam_api.so")
        endif()
    else()
        message(WARNING "ENJIN_STEAM enabled but Steamworks SDK not found at third_party/steamworks/")
    endif()
endif()

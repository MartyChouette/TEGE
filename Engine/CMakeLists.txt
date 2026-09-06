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
# Static, like Jolt below (JOLT_BUILD_SHARED_LIBS OFF). Assimp defaults to a
# SHARED library on Linux, and nothing installed or packaged the .so: the
# install rules copy executables only, and the Linux release tarball copies
# exactly two binaries while its README promises they are "self-contained apart
# from standard desktop system libraries". They were not. An installed or
# downloaded build died immediately with
#   ./EnjinEditor: error while loading shared libraries: libassimp.so.5
# unless the machine happened to have assimp 5 installed system-wide.
#
# Found by the install smoke on its first CI run (2026-09-06). It never showed
# on Windows, where the DLL sits beside the exe in the build tree, which is why
# a local check of the same install passed.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
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
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
if(MSVC)
    set(USE_STATIC_CRT ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(assimp)

# Suppress Clang warnings in third-party assimp code (memcpy on non-trivial
# types). Clang-only flag: GCC hard-errors on unknown -Wno-error= options.
if(TARGET assimp AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(assimp PRIVATE -Wno-nontrivial-memcall -Wno-error=nontrivial-memcall)
endif()

# Fetch miniaudio (header-only, public domain audio library)
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniaudio)

# Engine library - Rendering, ECS, Asset system, etc.
# See the note in Core/CMakeLists.txt: "src/**/*.cpp" requires an intervening
# directory and silently drops depth-1 files. CONFIGURE_DEPENDS re-globs when the
# directory changes — the two explicit appends further down this file are scars
# from exactly the staleness it prevents.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "include/*.h"
    "include/*.hpp"
)

# Exclude the wrong renderer backend's source files
if(ENJIN_PLATFORM_WEB)
    # Web build: exclude Vulkan-specific sources
    file(GLOB_RECURSE VULKAN_SOURCES "src/Renderer/Vulkan/*.cpp" "src/Renderer/RayTracing/*.cpp"
         "src/Renderer/ShadowMap.cpp" "src/Renderer/PointLightShadowMap.cpp"
         "src/Renderer/SpotLightShadowMap.cpp" "src/Renderer/PostProcessing.cpp"
         "src/Renderer/Skybox.cpp" "src/Renderer/GPUDriven/*.cpp"
         "src/Renderer/AsyncComputeScheduler.cpp" "src/Renderer/Upscaling/*.cpp"
         "src/Renderer/OITManager.cpp" "src/Renderer/SHLightProbe.cpp"
         "src/Renderer/ReflectionProbeSystem.cpp" "src/Renderer/SDFScene.cpp"
         "src/Renderer/VariableRateShading.cpp" "src/Renderer/VisibilityBuffer/*.cpp"
         "src/Renderer/VirtualTexture/*.cpp" "src/Renderer/VirtualTextureSystem.cpp"
         "src/Renderer/ClusteredLighting*.cpp"
         "src/Renderer/Texture.cpp" "src/Renderer/RenderTarget.cpp"
         "src/Renderer/SVGLoader.cpp" "src/Renderer/VectorTessellator.cpp" "src/Renderer/Materials/*.cpp"
         "src/Renderer/FrameGraph/*.cpp" "src/Renderer/RenderGraph/*.cpp"
         "src/Renderer/RenderPipeline/*.cpp" "src/Renderer/Techniques/*.cpp"
         "src/Renderer/VRS/*.cpp" "src/Renderer/HaltonSequence.cpp"
         "src/Renderer/RenderPass.cpp" "src/Renderer/Scripting/*.cpp" "src/Renderer/ShaderManager.cpp"
         "src/Renderer/VolumetricFog.cpp" "src/Renderer/ShadowAtlas.cpp"
         "src/Renderer/DDGIProbeSystem.cpp" "src/Effects/GPUParticleSystem.cpp"
         "src/Effects/WeatherRenderer.cpp" "src/Effects/ParticleRenderer.cpp"
         "src/Effects/FluidRenderer.cpp" "src/Effects/GrassRenderer.cpp"
         "src/Effects/ShrubRenderer.cpp" "src/Effects/TreeRenderer.cpp"
         "src/Effects/SpriteBatchRenderer.cpp" "src/Effects/SpriteTextureAtlas.cpp"
         "src/GUI/ImGuiLayer.cpp" "src/GUI/ShaderGUI.cpp" "src/GUI/EngineSplash.cpp"
         # ScriptBindings_Render/_AudioGraph compile on web ON PURPOSE: every
         # script-visible symbol must register on every platform or any .as
         # module mentioning one fails to compile (Render_SetRainActive bug).
         "src/Editor/*.cpp" "src/App/*.cpp"
         "src/Water/WaterRenderer.cpp")
    list(REMOVE_ITEM ENGINE_SOURCES ${VULKAN_SOURCES})

    # ScenePicker is a RUNTIME utility that happens to live under src/Editor/.
    # It is pure ECS + math ray-picking with no ImGui and no editor dependency,
    # and FlowerSystem (a gameplay system that ticks in the player) calls its
    # ScreenToRay. The blanket "src/Editor/*.cpp" exclusion above dropped it, so
    # the moment the web player started ticking FlowerSystem the link failed on
    # an undefined ScreenToRay. Put it back explicitly.
    #
    # The real fix is moving ScenePicker out of Editor/ -- the engine has ten
    # runtime headers reaching into Enjin/Editor/, and this is the first one to
    # actually break a build.
    list(APPEND ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Editor/ScenePicker.cpp")
    list(REMOVE_DUPLICATES ENGINE_SOURCES)
else()
    # Desktop build: exclude WebGPU-specific sources
    file(GLOB_RECURSE WEBGPU_SOURCES "src/Renderer/WebGPU/*.cpp")
    list(REMOVE_ITEM ENGINE_SOURCES ${WEBGPU_SOURCES})
    # Explicit append beats stale GLOB caches (same trick as the Phase 4-5 files
    # below). Web excludes this file: nanosvg's implementation lives in
    # SVGLoader.cpp, which is also web-excluded.
    list(APPEND ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer/VectorTessellator.cpp")
    list(REMOVE_DUPLICATES ENGINE_SOURCES)
endif()

# Phase 4-5 files may not be found by GLOB_RECURSE due to CMake caching — add explicitly
list(APPEND ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer/Vulkan/VulkanBindGroupManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer/Vulkan/VulkanRenderEncoder.cpp"
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
    zlibstatic
)

# Pak compression (W1): zlib comes from assimp's bundled contrib copy.
# zlib.h lives in the source tree, the generated zconf.h in the build tree.
target_include_directories(EnjinEngine PRIVATE
    ${assimp_SOURCE_DIR}/contrib/zlib
    ${assimp_BINARY_DIR}/contrib/zlib
)

target_compile_features(EnjinEngine PUBLIC cxx_std_20)

# Platform-specific networking and system libraries
if(ENJIN_PLATFORM_WEB)
    # Emscripten provides networking via fetch API and WebSocket — no native libs needed
elseif(WIN32)
    target_link_libraries(EnjinEngine PUBLIC ws2_32 winhttp ole32)
elseif(UNIX)
    # Linux/macOS: POSIX sockets are in libc (no extra link needed),
    # but we need libcurl for HTTP client functionality
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(EnjinEngine PUBLIC CURL::libcurl)
        target_compile_definitions(EnjinEngine PUBLIC ENJIN_HAS_CURL)
        message(STATUS "Found libcurl for HTTP client")
    elseif(ENJIN_PLATFORM_WEB)
        # The browser is the HTTP client here; libcurl would be meaningless.
        message(STATUS "HTTP client uses the browser's fetch (web build)")
    else()
        # A STATUS line scrolls past in a normal configure, and then every HTTP
        # call returns "HTTP not available on this platform" at runtime, taking
        # the marketplace, telemetry and feedback paths with it. Install
        # libcurl4-openssl-dev (Debian/Ubuntu) or libcurl-devel (Fedora).
        message(WARNING "libcurl not found — the HTTP client is compiled out, so "
                        "marketplace, telemetry and feedback will fail at runtime")
    endif()
endif()

# EditorLayer.cpp exceeds the default MSVC section limit due to heavy template use
# /utf-8 ensures source files with UTF-8 encoded characters compile correctly
if(MSVC)
    # /MP compiles this target's ~390 files in parallel. MSBuild's own --parallel
    # only spans projects, and the engine is one project, so without /MP the
    # largest target in the build is also the one that never uses more than a core.
    target_compile_options(EnjinEngine PRIVATE /bigobj /utf-8 /MP)
    target_compile_options(EnjinEngine PRIVATE ${ENJIN_PDB_COMPILE_OPTIONS})
endif()

# Precompiled header. The candidates are the headers nearly every translation
# unit already includes and that almost never change — Log.h alone is in 306
# files. See EnginePCH.h for what is deliberately kept OUT of it and why.
target_precompile_headers(EnjinEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Enjin/EnginePCH.h)

# --- Renderer backend selection (Vulkan vs WebGPU) ---
if(ENJIN_PLATFORM_WEB)
    # WebGPU backend — Emscripten provides <webgpu/webgpu.h> via -sUSE_WEBGPU=1
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_RENDERER_WEBGPU=1)
    message(STATUS "Renderer backend: WebGPU (Emscripten)")
else()
    # Vulkan backend (desktop)
    find_package(Vulkan REQUIRED)
    target_link_libraries(EnjinEngine PUBLIC Vulkan::Vulkan)
    message(STATUS "Renderer backend: Vulkan")

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
endif()

# Optional: Find shaderc for shader compilation
# find_package(shaderc REQUIRED)
# target_link_libraries(EnjinEngine PUBLIC shaderc::shaderc)

# Jolt Physics backend (3D) — ON by default
option(ENJIN_PHYSICS_JOLT "Enable Jolt Physics backend for 3D physics" ON)
if(ENJIN_PHYSICS_JOLT)
    set(JOLT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY ON CACHE BOOL "" FORCE)
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

# SimplePhysics legacy backend — REMOVED (source files deleted, Jolt + Box2D are production backends)

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

# NVIDIA OptiX AI Denoiser — optional GPU-based RT denoiser (OFF by default, requires OptiX SDK + CUDA)
option(ENJIN_RAYTRACING_OPTIX "Enable NVIDIA OptiX AI Denoiser for ray tracing" OFF)
if(ENJIN_RAYTRACING_OPTIX)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        find_path(OptiX_INCLUDE_DIR optix.h
            HINTS "$ENV{OptiX_INSTALL_DIR}/include" "$ENV{OPTIX_PATH}/include"
            PATH_SUFFIXES include)
        if(OptiX_INCLUDE_DIR)
            target_include_directories(EnjinEngine PUBLIC ${OptiX_INCLUDE_DIR})
            target_link_libraries(EnjinEngine PUBLIC CUDA::cudart)
            target_compile_definitions(EnjinEngine PUBLIC ENJIN_RAYTRACING_OPTIX)
            message(STATUS "NVIDIA OptiX AI Denoiser enabled (OptiX: ${OptiX_INCLUDE_DIR})")
        else()
            message(WARNING "OptiX headers not found — set OptiX_INSTALL_DIR or OPTIX_PATH, or disable ENJIN_RAYTRACING_OPTIX")
        endif()
    else()
        message(WARNING "CUDA not found — install CUDA Toolkit, or disable ENJIN_RAYTRACING_OPTIX")
    endif()
endif()

# Steam Audio HRTF binaural rendering — optional, OFF by default
option(ENJIN_AUDIO_STEAM_AUDIO "Enable Steam Audio HRTF binaural rendering" OFF)
if(ENJIN_AUDIO_STEAM_AUDIO)
    set(STEAM_AUDIO_DIR "${CMAKE_SOURCE_DIR}/third_party/steamaudio")
    if(EXISTS "${STEAM_AUDIO_DIR}/include/phonon.h")
        target_include_directories(EnjinEngine PUBLIC "${STEAM_AUDIO_DIR}/include")
        if(WIN32)
            target_link_libraries(EnjinEngine PUBLIC "${STEAM_AUDIO_DIR}/lib/windows-x64/phonon.lib")
        elseif(APPLE)
            target_link_libraries(EnjinEngine PUBLIC "${STEAM_AUDIO_DIR}/lib/osx/libphonon.dylib")
        else()
            target_link_libraries(EnjinEngine PUBLIC "${STEAM_AUDIO_DIR}/lib/linux-x64/libphonon.so")
        endif()
        target_compile_definitions(EnjinEngine PUBLIC ENJIN_AUDIO_STEAM_AUDIO)
        message(STATUS "Steam Audio HRTF enabled")
    else()
        message(WARNING "ENJIN_AUDIO_STEAM_AUDIO enabled but SDK not found at third_party/steamaudio/")
    endif()
endif()

# --- Performance Optimization Features ---

# Cluster light grid (ON by default).
#
# This builds the grid and dispatches the compute pass. It does NOT switch
# surface shading over to it: triangle.frag guards that path with the SHADER
# macro CLUSTERED_LIGHTING, and nothing defines it, so the one compiled
# triangle.frag.spv still uses the brute-force light loop.
#
# The grid is not wasted work: the volumetric fog compute shader reads the
# cluster light buffer, the grid and the index list. Turning this off turns
# off lit fog with it.
option(ENJIN_CLUSTERED_LIGHTING "Build the cluster light grid (consumed by volumetric fog)" ON)
if(ENJIN_CLUSTERED_LIGHTING)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_CLUSTERED_LIGHTING)
    message(STATUS "Cluster light grid enabled (fog consumer; surface shading still brute-force)")
endif()

# Variable Rate Shading — per-tile shading rate control via VK_KHR_fragment_shading_rate (OFF by default)
option(ENJIN_VRS "Enable Variable Rate Shading" OFF)
if(ENJIN_VRS)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_VRS)
    message(STATUS "Variable Rate Shading enabled")
endif()

# Virtual texturing — page-based streaming with feedback buffer (OFF by default)
option(ENJIN_VIRTUAL_TEXTURING "Enable virtual texturing (page-based streaming)" OFF)
if(ENJIN_VIRTUAL_TEXTURING)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_VIRTUAL_TEXTURING)
    message(STATUS "Virtual texturing enabled")
endif()

# Visibility buffer — alternative render path with deferred material evaluation (OFF by default)
option(ENJIN_VISIBILITY_BUFFER "Enable visibility buffer render path" OFF)
if(ENJIN_VISIBILITY_BUFFER)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_VISIBILITY_BUFFER)
    message(STATUS "Visibility buffer render path enabled")
endif()

# --- Temporal Upscaling ---

# FSR 2 built-in (Lanczos + CAS) — always available, no SDK dependency.
# The built-in implementation renders at lower resolution, applies TAA, then
# upscales via Lanczos-2 compute shader + CAS (Contrast Adaptive Sharpening).
# Requires upscale_lanczos.comp.spv and upscale_cas.comp.spv to be compiled.
message(STATUS "FSR 2 temporal upscaling (built-in Lanczos + CAS) — always available")

# NVIDIA DLSS — temporal upscaler, NVIDIA GPUs only (OFF by default, requires Streamline SDK)
option(ENJIN_UPSCALING_DLSS "Enable NVIDIA DLSS temporal upscaling" OFF)
if(ENJIN_UPSCALING_DLSS)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_UPSCALING_DLSS)

    # Locate the Streamline SDK: check third_party/streamline/ first, then $ENV{STREAMLINE_SDK}
    set(STREAMLINE_SDK_DIR "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/streamline/include/sl.h")
        set(STREAMLINE_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/streamline")
    elseif(DEFINED ENV{STREAMLINE_SDK} AND EXISTS "$ENV{STREAMLINE_SDK}/include/sl.h")
        set(STREAMLINE_SDK_DIR "$ENV{STREAMLINE_SDK}")
    endif()

    if(STREAMLINE_SDK_DIR)
        target_compile_definitions(EnjinEngine PUBLIC ENJIN_HAS_DLSS_SDK)
        target_include_directories(EnjinEngine PUBLIC "${STREAMLINE_SDK_DIR}/include")
        if(WIN32)
            if(CMAKE_SIZEOF_VOID_POINTER EQUAL 8)
                set(STREAMLINE_LIB_DIR "${STREAMLINE_SDK_DIR}/lib/x64")
            else()
                set(STREAMLINE_LIB_DIR "${STREAMLINE_SDK_DIR}/lib/x86")
            endif()
        else()
            set(STREAMLINE_LIB_DIR "${STREAMLINE_SDK_DIR}/lib")
        endif()
        target_link_libraries(EnjinEngine PUBLIC
            "${STREAMLINE_LIB_DIR}/sl.dlss.lib"
            "${STREAMLINE_LIB_DIR}/sl.interposer.lib"
        )
        message(STATUS "NVIDIA DLSS: Streamline SDK found at ${STREAMLINE_SDK_DIR}")
    else()
        message(STATUS "NVIDIA DLSS: SDK not found — using built-in Lanczos + CAS fallback")
        message(STATUS "  To link the real SDK, place Streamline SDK at third_party/streamline/")
        message(STATUS "  or set STREAMLINE_SDK environment variable")
    endif()
endif()

# Intel XeSS — temporal upscaler, all GPUs via DP4a (OFF by default, requires XeSS SDK)
option(ENJIN_UPSCALING_XESS "Enable Intel XeSS temporal upscaling" OFF)
if(ENJIN_UPSCALING_XESS)
    target_compile_definitions(EnjinEngine PUBLIC ENJIN_UPSCALING_XESS)

    # Locate the XeSS SDK: check third_party/xess/ first, then $ENV{XESS_SDK}
    set(XESS_SDK_DIR "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/xess/inc/xess/xess.h")
        set(XESS_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/xess")
    elseif(DEFINED ENV{XESS_SDK} AND EXISTS "$ENV{XESS_SDK}/inc/xess/xess.h")
        set(XESS_SDK_DIR "$ENV{XESS_SDK}")
    endif()

    if(XESS_SDK_DIR)
        target_compile_definitions(EnjinEngine PUBLIC ENJIN_HAS_XESS_SDK)
        target_include_directories(EnjinEngine PUBLIC "${XESS_SDK_DIR}/inc")
        if(WIN32)
            if(CMAKE_SIZEOF_VOID_POINTER EQUAL 8)
                set(XESS_LIB_DIR "${XESS_SDK_DIR}/lib")
            else()
                set(XESS_LIB_DIR "${XESS_SDK_DIR}/lib/x86")
            endif()
        else()
            set(XESS_LIB_DIR "${XESS_SDK_DIR}/lib")
        endif()
        target_link_libraries(EnjinEngine PUBLIC
            "${XESS_LIB_DIR}/libxess.lib"
        )
        message(STATUS "Intel XeSS: SDK found at ${XESS_SDK_DIR}")
    else()
        message(STATUS "Intel XeSS: SDK not found — using built-in Lanczos + CAS fallback")
        message(STATUS "  To link the real SDK, place XeSS SDK at third_party/xess/")
        message(STATUS "  or set XESS_SDK environment variable")
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

# ============================================================================
# Shader codegen: GLSL -> SPIR-V -> ShaderData.h/.cpp
# ============================================================================
# This was a two-step manual ritual - glslangValidator by hand, then
# `python _gen_all.py` by hand - and skipping either step leaves the engine
# running SPIR-V that does not match its source. The failure is silent: the GPU
# reads the old struct offsets and produces dark scenes, wrong colours or a
# crash, with a shader file on disk that looks correct. Making it a build step
# means it cannot be forgotten.
#
# The Vulkan-only shaders build here. RT shaders still go through _gen_rt.py:
# they #include rt_common.glsl, which glslangValidator rejects, so they need
# glslc with an include path.
#
# If no compiler is on PATH the checked-in generated files are used as-is, so a
# machine without the Vulkan SDK still builds (CI containers, contributors).
if(NOT ENJIN_PLATFORM_WEB)
    find_program(ENJIN_GLSLANG NAMES glslangValidator glslangValidator.exe)
    find_program(ENJIN_PYTHON NAMES python python3)

    if(ENJIN_GLSLANG AND ENJIN_PYTHON)
        file(GLOB ENJIN_SHADER_SRCS
             "${CMAKE_SOURCE_DIR}/Engine/shaders/*.vert"
             "${CMAKE_SOURCE_DIR}/Engine/shaders/*.frag"
             "${CMAKE_SOURCE_DIR}/Engine/shaders/*.comp")

        # RT shaders include rt_common.glsl and are handled by _gen_rt.py.
        list(FILTER ENJIN_SHADER_SRCS EXCLUDE REGEX "/rt_")

        set(ENJIN_SPV_OUTPUTS "")
        foreach(SHADER ${ENJIN_SHADER_SRCS})
            set(SPV "${SHADER}.spv")
            add_custom_command(
                OUTPUT "${SPV}"
                COMMAND "${ENJIN_GLSLANG}" -V "${SHADER}" -o "${SPV}"
                DEPENDS "${SHADER}"
                COMMENT "SPIR-V: ${SHADER}"
                VERBATIM)
            list(APPEND ENJIN_SPV_OUTPUTS "${SPV}")
        endforeach()

        set(ENJIN_SHADERDATA_H "${CMAKE_SOURCE_DIR}/Engine/include/Enjin/Renderer/Vulkan/ShaderData.h")
        set(ENJIN_SHADERDATA_CPP "${CMAKE_SOURCE_DIR}/Engine/src/Renderer/Vulkan/ShaderDataGenerated.cpp")

        add_custom_command(
            OUTPUT "${ENJIN_SHADERDATA_H}" "${ENJIN_SHADERDATA_CPP}"
            COMMAND "${ENJIN_PYTHON}" "${CMAKE_SOURCE_DIR}/_gen_all.py"
            DEPENDS ${ENJIN_SPV_OUTPUTS} "${CMAKE_SOURCE_DIR}/_gen_all.py"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Packing SPIR-V into ShaderData"
            VERBATIM)

        add_custom_target(EnjinShaderCodegen
                          DEPENDS "${ENJIN_SHADERDATA_H}" "${ENJIN_SHADERDATA_CPP}")
        add_dependencies(EnjinEngine EnjinShaderCodegen)
        message(STATUS "Shader codegen wired: ${ENJIN_GLSLANG}")
    else()
        message(STATUS "Shader codegen SKIPPED (glslangValidator or python not found) "
                       "- building against the checked-in ShaderData")
    endif()
endif()

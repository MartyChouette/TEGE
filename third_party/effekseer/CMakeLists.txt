# Effekseer runtime and its official renderers
# Vendored from https://github.com/effekseer/Effekseer (MIT license), release
# tag 1807. See ../VERSIONS.md. The folders keep upstream's Dev/Cpp layout,
# because the sources include each other by relative path.
#
#   Effekseer/                  simulation core
#   EffekseerRendererCommon/    shared renderer code
#   EffekseerRendererLLGI/      renderer written against LLGI
#   EffekseerRendererVulkan/    desktop backend   (built when NOT web)
#   EffekseerRendererWebGPU/    web backend       (built when web)
#   EffekseerMaterialCompiler/  shader generators for .efkmat materials
#   3rdParty/LLGI/              upstream's graphics layer (zlib-style licence)
#   3rdParty/stb_effekseer/     their stb_image copy, for effect textures
#
# This is the simulation core: it loads .efkefc/.efk effects and steps them. It
# draws nothing by itself. The network client/server (the live link to the
# Effekseer editor) is compiled out: every file under Effekseer/Network is
# guarded by __EFFEKSEER_NETWORK_ENABLED__, which is deliberately not defined,
# so the library opens no sockets.

file(GLOB_RECURSE EFFEKSEER_SOURCES CONFIGURE_DEPENDS
    "Effekseer/*.cpp"
)

add_library(Effekseer STATIC ${EFFEKSEER_SOURCES})

# <Effekseer.h> and <Effekseer/...> both resolve from here, as upstream's do.
target_include_directories(Effekseer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Effekseer
)

target_compile_features(Effekseer PUBLIC cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(Effekseer PUBLIC Threads::Threads)

# Suppress warnings in third-party code
if(MSVC)
    target_compile_options(Effekseer PRIVATE /W0 /MP /utf-8)
else()
    target_compile_options(Effekseer PRIVATE -w)
endif()

# --- Shared renderer code ---------------------------------------------------
file(GLOB EFK_RENDERER_COMMON_SOURCES CONFIGURE_DEPENDS
    "EffekseerRendererCommon/EffekseerRendererCommon/*.cpp"
)
add_library(EffekseerRendererCommon STATIC ${EFK_RENDERER_COMMON_SOURCES})
target_include_directories(EffekseerRendererCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/EffekseerRendererCommon
)
target_link_libraries(EffekseerRendererCommon PUBLIC Effekseer)

# --- LLGI ---------------------------------------------------------------------
# Only the backend this platform draws with. Upstream also builds DX12 under
# MSVC, a window per platform, PC/LLGI.CreatePC.cpp and
# Vulkan/LLGI.PlatformVulkan.cpp, which between them create a device, a
# swapchain and a window of their own. We hand LLGI the engine's device, so
# none of that is compiled and the Win, Linux and PC folders are not vendored.
# PlatformVulkan is the only file that includes the window headers, and the
# Linux one needs an X11 package (libx11-xcb-dev) nothing else here does.
file(GLOB EFK_LLGI_SOURCES CONFIGURE_DEPENDS "3rdParty/LLGI/src/*.cpp")
if(ENJIN_PLATFORM_WEB)
    file(GLOB EFK_LLGI_BACKEND CONFIGURE_DEPENDS "3rdParty/LLGI/src/WebGPU/*.cpp")
else()
    file(GLOB EFK_LLGI_BACKEND CONFIGURE_DEPENDS "3rdParty/LLGI/src/Vulkan/*.cpp")
    list(FILTER EFK_LLGI_BACKEND EXCLUDE REGEX "LLGI[.]PlatformVulkan[.]cpp$")
endif()
add_library(LLGI STATIC ${EFK_LLGI_SOURCES} ${EFK_LLGI_BACKEND})
target_include_directories(LLGI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/LLGI/src)
if(ENJIN_PLATFORM_WEB)
    target_compile_definitions(LLGI PUBLIC ENABLE_WEBGPU)
    target_compile_features(LLGI PUBLIC cxx_std_20)
    target_compile_options(LLGI PUBLIC --use-port=emdawnwebgpu)
    target_link_options(LLGI PUBLIC --use-port=emdawnwebgpu)
else()
    # ENABLE_VULKAN_COMPILER is deliberately NOT defined: it links glslang to
    # compile .efkmat materials at load. Until that is wired, an effect that
    # uses a custom material does not draw on desktop. Built-in shaders ship
    # precompiled in EffekseerRendererVulkan/ShaderHeader.
    find_package(Vulkan REQUIRED)
    target_compile_definitions(LLGI PUBLIC ENABLE_VULKAN)
    target_compile_features(LLGI PUBLIC cxx_std_17)
    target_link_libraries(LLGI PUBLIC Vulkan::Vulkan)
    if(WIN32)
        # LLGI calls std::min/std::max with <windows.h> in scope
        target_compile_definitions(LLGI PRIVATE NOMINMAX)
    endif()
endif()

# --- Renderer on LLGI, and the backend on top of it ----------------------------
file(GLOB EFK_RENDERER_LLGI_SOURCES CONFIGURE_DEPENDS
    "EffekseerRendererLLGI/EffekseerRendererLLGI/*.cpp"
)
add_library(EffekseerRendererLLGI STATIC ${EFK_RENDERER_LLGI_SOURCES})
target_include_directories(EffekseerRendererLLGI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/EffekseerRendererLLGI
)
target_link_libraries(EffekseerRendererLLGI PUBLIC Effekseer EffekseerRendererCommon LLGI)

if(ENJIN_PLATFORM_WEB)
    file(GLOB EFK_BACKEND_SOURCES CONFIGURE_DEPENDS
        "EffekseerRendererWebGPU/EffekseerRendererWebGPU/*.cpp"
        "EffekseerMaterialCompiler/WGSLGenerator/*.cpp"
    )
    add_library(EffekseerRendererBackend STATIC ${EFK_BACKEND_SOURCES})
    target_include_directories(EffekseerRendererBackend PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/EffekseerRendererWebGPU
    )
    # Upstream's WebGPU renderer includes one header from the Vulkan one
    target_include_directories(EffekseerRendererBackend PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/EffekseerRendererVulkan
    )
else()
    file(GLOB EFK_BACKEND_SOURCES CONFIGURE_DEPENDS
        "EffekseerRendererVulkan/EffekseerRendererVulkan/*.cpp"
        "EffekseerMaterialCompiler/GLSLGenerator/*.cpp"
        "EffekseerMaterialCompiler/Common/*.cpp"
    )
    add_library(EffekseerRendererBackend STATIC ${EFK_BACKEND_SOURCES})
    target_include_directories(EffekseerRendererBackend PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/EffekseerRendererVulkan
    )
endif()
target_link_libraries(EffekseerRendererBackend PUBLIC
    Effekseer EffekseerRendererCommon EffekseerRendererLLGI LLGI)
if(UNIX AND NOT APPLE AND NOT ENJIN_PLATFORM_WEB)
    # LLGI's Vulkan header defines VK_USE_PLATFORM_XCB_KHR and then includes
    # vulkan.hpp. The Vulkan renderer includes plain vulkan.h first, without
    # it, so vulkan.hpp finds the XCB types missing. Defining it for the whole
    # target makes the first include agree with the second.
    target_compile_definitions(EffekseerRendererBackend PRIVATE VK_USE_PLATFORM_XCB_KHR)
endif()

foreach(efk_target EffekseerRendererCommon LLGI EffekseerRendererLLGI EffekseerRendererBackend)
    if(MSVC)
        target_compile_options(${efk_target} PRIVATE /W0 /MP /utf-8)
    else()
        target_compile_options(${efk_target} PRIVATE -w)
    endif()
endforeach()

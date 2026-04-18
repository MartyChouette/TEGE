# EmscriptenToolchain.cmake — Emscripten-specific CMake configuration for Enjin Engine
#
# Usage (after sourcing emsdk):
#   emcmake cmake -B build-web -DENJIN_PLATFORM_WEB=ON
#   emmake cmake --build build-web
#
# Or explicitly:
#   cmake -B build-web -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
#         -DENJIN_PLATFORM_WEB=ON
#   cmake --build build-web

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "EmscriptenToolchain.cmake included but not building with Emscripten.\n"
                        "Use: emcmake cmake .. -DENJIN_PLATFORM_WEB=ON")
    return()
endif()

message(STATUS "Configuring Enjin for WebAssembly (Emscripten ${EMSCRIPTEN_VERSION})")

# --- Compiler flags ---
# --use-port=emdawnwebgpu : Enable WebGPU bindings (Emscripten 5.0+, replaces -sUSE_WEBGPU)
# -pthread              : Enable pthreads (SharedArrayBuffer). Omit if targeting no-thread browsers.
# -O2                   : Optimize for size/speed balance

set(ENJIN_EM_COMPILE_FLAGS
    --use-port=emdawnwebgpu
    -O2
    -fexceptions
    -DENJIN_PLATFORM_WEB=1
    -DENJIN_RENDERER_WEBGPU=1
)

# --- Linker flags ---
# -sALLOW_MEMORY_GROWTH=1  : Dynamic heap growth (required — game memory is unpredictable)
# -sMAXIMUM_MEMORY=512MB   : Cap to prevent runaway allocation
# -sSTACK_SIZE=1048576      : 1 MB stack (matches typical native stack)
# -sEXPORTED_FUNCTIONS      : Export main + memory management
# -sEXPORTED_RUNTIME_METHODS : ccall/cwrap for JS interop
# --use-port=emdawnwebgpu    : Link WebGPU (Dawn port)
# --preload-file             : Added at build time per-project

set(ENJIN_EM_LINK_FLAGS
    --use-port=emdawnwebgpu
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=536870912
    -sSTACK_SIZE=1048576
    -sEXPORTED_FUNCTIONS=['_main']
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPF32']
    -sENVIRONMENT=web
    -sASSERTIONS=1
    -sNO_DISABLE_EXCEPTION_CATCHING
    -fexceptions
    -sASYNCIFY
    -O2
    --post-js=${CMAKE_SOURCE_DIR}/cmake/miniaudio_shim.js
)

# --- Apply flags ---
add_compile_options(${ENJIN_EM_COMPILE_FLAGS})
add_link_options(${ENJIN_EM_LINK_FLAGS})

# --- Disable features unavailable on web ---
set(ENJIN_BUILD_EDITOR OFF CACHE BOOL "Editor not supported on web" FORCE)
set(ENJIN_BUILD_HUB    OFF CACHE BOOL "Hub not supported on web" FORCE)
set(ENJIN_BUILD_TESTS  OFF CACHE BOOL "Tests not supported on web" FORCE)

# AngelScript portability mode for Emscripten (no JIT, generic calling conventions)
add_compile_definitions(AS_MAX_PORTABILITY)

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
    -sEXPORTED_FUNCTIONS=['_main','_onCanvasResize','_getDrawCallCount','_getEntityCount','_enjin_enable_touch_controls','_getStreamingChunkCount','_getStreamingLoadedCount','_getStreamingResidentMB','_getStreamingBudgetMB','_getStreamingEvictions','_setStreamingBudgetMB','_getLazyFSResidentMB','_getLazyFSEvictions','_getHeapMB','_enjin_lazyfs_materialize','_enjin_lazyfs_buffer','_enjin_lazyfs_release','_enjin_lazyfs_budget','_enjin_lazyfs_on_evict']
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPF32']
    -sENVIRONMENT=web
    # The Fetch API, for one thing: game.enjpak has to be requested with
    # Cache-Control: no-cache so the browser revalidates it instead of serving
    # a stale copy. emscripten_async_wget_data cannot set a request header, and
    # the pak URL carries no cache-buster, so a redeployed pak was being paired
    # with a fresh engine - the stale-pairing bug one level below the one
    # tools/check_demoroom.py watches for.
    -sFETCH
    -sNO_DISABLE_EXCEPTION_CATCHING
    -fexceptions
    -O2
    # emmalloc is a much smaller allocator than dlmalloc and fragments less,
    # which is what matters with ALLOW_MEMORY_GROWTH on: the heap only ever
    # grows, so fragmentation is permanent.
    -sMALLOC=emmalloc
    # Decode strings with the browser's TextDecoder instead of the JS fallback.
    -sTEXTDECODER=2
    -lidbfs.js   # IndexedDB-backed FS for persistent saves (web_main mounts /saves)
    --post-js=${CMAKE_SOURCE_DIR}/cmake/miniaudio_shim.js
)
# Runtime assertions cost size and speed — keep them for Debug configures only.
# (They were unconditionally on until 2026-07; release WASM ships without.)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND ENJIN_EM_LINK_FLAGS -sASSERTIONS=1)
endif()
# NOTE: -sASYNCIFY was removed 2026-07 — boot is fully callback-driven now
# (emscripten_async_wget_data + WebGPURenderer::InitializeAsync). Reintroducing
# any emscripten_sleep/emscripten_wget_data call will abort at runtime.

# --- Apply flags ---
add_compile_options(${ENJIN_EM_COMPILE_FLAGS})
add_link_options(${ENJIN_EM_LINK_FLAGS})

# --- Disable features unavailable on web ---
set(ENJIN_BUILD_EDITOR OFF CACHE BOOL "Editor not supported on web" FORCE)
set(ENJIN_BUILD_HUB    OFF CACHE BOOL "Hub not supported on web" FORCE)
set(ENJIN_BUILD_TESTS  OFF CACHE BOOL "Tests not supported on web" FORCE)

# AngelScript portability mode for Emscripten (no JIT, generic calling conventions)
add_compile_definitions(AS_MAX_PORTABILITY)

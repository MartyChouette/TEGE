#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #ifndef ENJIN_PLATFORM_WINDOWS
        #define ENJIN_PLATFORM_WINDOWS
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#elif defined(__linux__)
    #ifndef ENJIN_PLATFORM_LINUX
        #define ENJIN_PLATFORM_LINUX
    #endif
#elif defined(__APPLE__)
    #define ENJIN_PLATFORM_MACOS
    #define ENJIN_PLATFORM_POSIX
#elif defined(__EMSCRIPTEN__)
    #define ENJIN_PLATFORM_WEB 1
    #include <emscripten.h>
    #include <emscripten/html5.h>
#elif defined(__SWITCH__) || defined(ENJIN_PLATFORM_SWITCH)
    // Nintendo Switch (NVN graphics API)
    // __SWITCH__ is defined by the official Nintendo SDK toolchain.
    // ENJIN_PLATFORM_SWITCH can also be set manually via CMake.
    #ifndef ENJIN_PLATFORM_SWITCH
        #define ENJIN_PLATFORM_SWITCH
    #endif
#else
    #error "Unsupported platform"
#endif

// Compiler detection
#if defined(_MSC_VER)
    #define ENJIN_COMPILER_MSVC
    #define ENJIN_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define ENJIN_COMPILER_GCC
    #define ENJIN_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define ENJIN_FORCE_INLINE inline
#endif

// Debug/Release detection
#if !defined(ENJIN_BUILD_RELEASE) && !defined(ENJIN_BUILD_DEBUG)
    #if defined(NDEBUG) || defined(_RELEASE)
        #define ENJIN_BUILD_RELEASE
    #else
        #define ENJIN_BUILD_DEBUG
    #endif
#endif

// Export macros for DLL
#ifdef ENJIN_BUILD_SHARED
    #ifdef ENJIN_PLATFORM_WINDOWS
        #ifdef ENJIN_BUILD_CORE
            #define ENJIN_API __declspec(dllexport)
        #else
            #define ENJIN_API __declspec(dllimport)
        #endif
    #else
        #define ENJIN_API __attribute__((visibility("default")))
    #endif
#else
    #define ENJIN_API
#endif

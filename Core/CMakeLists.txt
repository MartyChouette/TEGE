cmake_minimum_required(VERSION 3.20)

# Core library - Foundation components
file(GLOB_RECURSE CORE_SOURCES
    "src/**/*.cpp"
    "include/*.h"
    "include/*.hpp"
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Enjin/Core/Version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/Enjin/Core/Version.h
)

add_library(EnjinCore STATIC ${CORE_SOURCES})

target_include_directories(EnjinCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(EnjinCore PUBLIC cxx_std_20)

# Find GLFW for windowing
# Try multiple methods to find GLFW
set(GLFW_FOUND FALSE)

# Method 1: Check if vcpkg is being used and GLFW is available
if(CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    # vcpkg toolchain is being used
    # Try to find GLFW via vcpkg's installed packages
    find_path(GLFW_VCPKG_INCLUDE_DIR
        NAMES GLFW/glfw3.h
        PATHS
            ${VCPKG_ROOT}/installed/x64-windows/include
            ${VCPKG_ROOT}/installed/x86-windows/include
            C:/vcpkg/installed/x64-windows/include
            C:/vcpkg/installed/x86-windows/include
        NO_DEFAULT_PATH
    )
    
    if(GLFW_VCPKG_INCLUDE_DIR)
        find_library(GLFW_VCPKG_LIBRARY
            NAMES glfw3 glfw
            PATHS
                ${VCPKG_ROOT}/installed/x64-windows/lib
                ${VCPKG_ROOT}/installed/x86-windows/lib
                C:/vcpkg/installed/x64-windows/lib
                C:/vcpkg/installed/x86-windows/lib
            NO_DEFAULT_PATH
        )
        
        if(GLFW_VCPKG_LIBRARY)
            set(GLFW_FOUND TRUE)
            target_include_directories(EnjinCore PUBLIC ${GLFW_VCPKG_INCLUDE_DIR})
            target_link_libraries(EnjinCore PUBLIC ${GLFW_VCPKG_LIBRARY})
            message(STATUS "Found GLFW via vcpkg: ${GLFW_VCPKG_LIBRARY}")
        endif()
    endif()
endif()

# Method 2: Try pkg-config (Linux)
if(NOT GLFW_FOUND AND UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GLFW3 QUIET glfw3)
        if(GLFW3_FOUND)
            set(GLFW_FOUND TRUE)
            target_include_directories(EnjinCore PUBLIC ${GLFW3_INCLUDE_DIRS})
            target_link_libraries(EnjinCore PUBLIC ${GLFW3_LIBRARIES})
            target_compile_options(EnjinCore PUBLIC ${GLFW3_CFLAGS_OTHER})
            message(STATUS "Found GLFW via pkg-config")
        endif()
    endif()
endif()

# Method 3: Try manual find (Windows/Linux fallback)
if(NOT GLFW_FOUND)
    find_path(GLFW_INCLUDE_DIR
        NAMES GLFW/glfw3.h
        PATHS
            ${CMAKE_SOURCE_DIR}/third_party/glfw/include
            ${CMAKE_SOURCE_DIR}/../third_party/glfw/include
            /usr/include
            /usr/local/include
            C:/glfw/include
            ${CMAKE_PREFIX_PATH}/include
    )
    
    find_library(GLFW_LIBRARY
        NAMES glfw3 glfw
        PATHS
            ${CMAKE_SOURCE_DIR}/third_party/glfw/lib
            ${CMAKE_SOURCE_DIR}/../third_party/glfw/lib
            /usr/lib
            /usr/local/lib
            C:/glfw/lib
            ${CMAKE_PREFIX_PATH}/lib
    )
    
    if(GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
        set(GLFW_FOUND TRUE)
        target_include_directories(EnjinCore PUBLIC ${GLFW_INCLUDE_DIR})
        target_link_libraries(EnjinCore PUBLIC ${GLFW_LIBRARY})
        message(STATUS "Found GLFW manually: ${GLFW_LIBRARY}")
    endif()
endif()

# Method 4: Use FetchContent as last resort (downloads GLFW)
if(NOT GLFW_FOUND)
    message(STATUS "GLFW not found in system, using FetchContent to download and build...")
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
    set(GLFW_FOUND TRUE)
    target_link_libraries(EnjinCore PUBLIC glfw)
    message(STATUS "GLFW downloaded and built via FetchContent")
endif()

if(NOT GLFW_FOUND)
    message(FATAL_ERROR "Could not find or build GLFW. Please install GLFW3 or check your vcpkg installation.")
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(EnjinCore PUBLIC ENJIN_PLATFORM_WINDOWS)
    target_link_libraries(EnjinCore PUBLIC winmm)  # timeBeginPeriod/timeEndPeriod for precise frame pacing
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(EnjinCore PUBLIC ENJIN_PLATFORM_LINUX)
elseif(APPLE)
    target_compile_definitions(EnjinCore PUBLIC ENJIN_PLATFORM_MACOS)
endif()

cmake_minimum_required(VERSION 3.20)

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
)

target_link_libraries(EnjinEngine PUBLIC
    EnjinCore
    imgui
)

target_compile_features(EnjinEngine PUBLIC cxx_std_20)

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

cmake_minimum_required(VERSION 3.20)

# Editor executable
file(GLOB_RECURSE EDITOR_SOURCES
    "src/*.cpp"
    "include/*.h"
    "include/*.hpp"
)

add_executable(EnjinEditor ${EDITOR_SOURCES})

target_include_directories(EnjinEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(EnjinEditor PRIVATE
    EnjinEngine
    EnjinCore
)

target_compile_features(EnjinEditor PUBLIC cxx_std_20)

# Find Dear ImGui (MIT licensed)
# Note: You'll need to add ImGui as a submodule or download it
# find_package(imgui REQUIRED)
# target_link_libraries(EnjinEditor PRIVATE imgui::imgui)

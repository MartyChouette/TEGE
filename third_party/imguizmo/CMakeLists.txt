cmake_minimum_required(VERSION 3.20)

add_library(imguizmo STATIC
    ImGuizmo.cpp
    ImGuizmo.h
)

target_include_directories(imguizmo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ImGuizmo depends on ImGui
target_link_libraries(imguizmo PUBLIC imgui)

cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Enjin Hub - Standalone project launcher & engine version manager
# Uses GLFW + ImGui (OpenGL backend) — no Vulkan dependency
# =============================================================================

# Hub executable
add_executable(EnjinHub
    src/main.cpp
    src/HubApplication.cpp
)

target_include_directories(EnjinHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

target_compile_features(EnjinHub PRIVATE cxx_std_20)

# --- GLFW ---
# Reuse GLFW from the Core library (already resolved by Core/CMakeLists.txt)
if(TARGET glfw)
    target_link_libraries(EnjinHub PRIVATE glfw)
elseif(TARGET glfw3)
    target_link_libraries(EnjinHub PRIVATE glfw3)
else()
    find_package(glfw3 REQUIRED)
    target_link_libraries(EnjinHub PRIVATE glfw)
endif()

# --- OpenGL ---
find_package(OpenGL REQUIRED)
target_link_libraries(EnjinHub PRIVATE OpenGL::GL)

# --- ImGui (OpenGL3 + GLFW backend) ---
# Build ImGui separately for the Hub with OpenGL instead of Vulkan
add_library(imgui_hub STATIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_hub PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

# Link GLFW (needed by imgui_impl_glfw)
if(TARGET glfw)
    target_link_libraries(imgui_hub PUBLIC glfw)
elseif(TARGET glfw3)
    target_link_libraries(imgui_hub PUBLIC glfw3)
endif()

target_link_libraries(imgui_hub PUBLIC OpenGL::GL)
target_compile_features(imgui_hub PRIVATE cxx_std_20)

target_link_libraries(EnjinHub PRIVATE imgui_hub)

# Platform-specific
if(WIN32)
    target_compile_definitions(EnjinHub PRIVATE NOMINMAX)
endif()

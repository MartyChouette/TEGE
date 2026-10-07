cmake_minimum_required(VERSION 3.20)

if(NOT ENJIN_PLATFORM_WEB)
    find_package(Vulkan REQUIRED)
endif()

add_library(imgui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
)

if(NOT ENJIN_PLATFORM_WEB)
    target_sources(imgui PRIVATE
        backends/imgui_impl_glfw.cpp
        backends/imgui_impl_vulkan.cpp
    )
else()
    # Web: WebGPU render backend (Emscripten's webgpu.h). No platform backend —
    # the web player feeds ImGuiIO manually from its own HTML5 input callbacks.
    target_sources(imgui PRIVATE
        backends/imgui_impl_wgpu.cpp
    )
endif()

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# Compile imgui without the functions and names it has marked obsolete, for
# everything that includes it. A call to one is then a compile error on the day
# it is written, not a surprise on the imgui update that finally removes it.
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# Link dependencies
if(ENJIN_PLATFORM_WEB)
    target_link_libraries(imgui PUBLIC glfw)
else()
    target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
endif()

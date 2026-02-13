cmake_minimum_required(VERSION 3.16)
project(ExamplePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Find the Enjin SDK headers
# Adjust this path based on your Enjin installation
set(ENJIN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/include")

add_library(ExamplePlugin SHARED ExamplePlugin.cpp)
target_include_directories(ExamplePlugin PRIVATE
    ${ENJIN_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui"
)
target_compile_definitions(ExamplePlugin PRIVATE ENJIN_PLUGIN_BUILD)

# Copy output to plugin directory
set_target_properties(ExamplePlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

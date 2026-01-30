# Shader Compilation Guide

## Current Status

The engine includes pre-compiled SPIR-V shaders for the triangle example. However, for production use, you'll want to compile GLSL shaders to SPIR-V.

## Option 1: Use glslc (Recommended)

`glslc` is part of the Vulkan SDK and can compile GLSL to SPIR-V:

```bash
# Compile vertex shader
glslc shader.vert -o shader.vert.spv

# Compile fragment shader
glslc shader.frag -o shader.frag.spv
```

Then load them in code:
```cpp
VulkanShader shader(context);
shader.LoadFromFile("shader.vert.spv");
```

## Option 2: Integrate shaderc

The `shaderc` library (Google's shader compiler) can be integrated for runtime compilation:

1. Add shaderc to CMakeLists.txt:
```cmake
find_package(shaderc REQUIRED)
target_link_libraries(EnjinEngine PUBLIC shaderc::shaderc)
```

2. Implement `ShaderCompiler::CompileGLSL()` in `VulkanShader.cpp`:
```cpp
#include <shaderc/shaderc.hpp>

bool CompileGLSL(const std::string& source, VkShaderStageFlagBits stage, std::vector<u32>& spirv) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    
    shaderc_shader_kind kind = (stage == VK_SHADER_STAGE_VERTEX_BIT) 
        ? shaderc_vertex_shader 
        : shaderc_fragment_shader;
    
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        source, kind, "shader", options);
    
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        ENJIN_LOG_ERROR(Renderer, "Shader compilation failed: %s", 
            result.GetErrorMessage().c_str());
        return false;
    }
    
    spirv = {result.cbegin(), result.cend()};
    return true;
}
```

## Option 3: Pre-compile at Build Time

Use CMake to compile shaders during build:

```cmake
# Find glslc
find_program(GLSLC glslc)

# Compile shaders
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
    COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/shaders/triangle.vert -o ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/triangle.vert
)

# Copy to output directory
file(COPY ${CMAKE_BINARY_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR}/bin)
```

## Shader Location

Place your GLSL shaders in:
```
Engine/shaders/
    triangle.vert
    triangle.frag
```

## Current Vertex Input Layout

The vertex shader accepts the following inputs (96 bytes per vertex):

| Location | Type | Attribute |
|----------|------|-----------|
| 0 | vec3 | Position |
| 1 | vec3 | Normal |
| 2 | vec2 | UV |
| 3 | vec4 | Vertex Color |
| 4 | vec4 | Tangent (xyz=dir, w=handedness) |
| 5 | vec4 | Bone Weights |
| 6 | uvec4 | Bone Indices |

## Descriptor Bindings

| Binding | Type | Stage | Purpose |
|---------|------|-------|---------|
| 0 | Uniform Buffer | Vertex | View/Projection matrices |
| 1 | Uniform Buffer | Vertex+Fragment | Lighting data |
| 2 | Uniform Buffer | Fragment | Material data |
| 3 | Combined Image Sampler | Fragment | Base color texture |
| 4 | Combined Image Sampler | Fragment | Shadow map |
| 5 | Combined Image Sampler | Fragment | Height map |
| 6 | Combined Image Sampler | Fragment | Normal map |
| 7 | Storage Buffer | Vertex | Bone matrices (skeletal animation) |

## Note on Current Implementation

The `ShaderData.h` file contains compiled SPIR-V bytecode embedded as C++ arrays. After modifying any shader, recompile with `glslangValidator` and regenerate the header. The engine loads shaders from these embedded arrays at startup.

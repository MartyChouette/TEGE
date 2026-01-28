# CLAUDE.md - Enjin Engine Project Context

## Overview

Enjin is a proprietary, licensable game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering capabilities.

## Build Commands

```bash
# Build (from project root)
cd build && make -j$(nproc)

# Clean rebuild
cd build && make clean && make -j$(nproc)

# Reconfigure CMake (needed after adding new source files)
cd build && cmake ..

# Compile shaders (GLSL to SPIR-V)
glslangValidator -V Engine/shaders/triangle.vert -o Engine/shaders/triangle.vert.spv
glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv

# Run the editor
./build/bin/EnjinEditor
```

## Project Architecture

```
enjin/
├── Core/                    # Foundation layer (no engine dependencies)
│   ├── include/Enjin/
│   │   ├── Core/           # Application, Window, Input
│   │   ├── Logging/        # Thread-safe categorized logging
│   │   ├── Math/           # Vector, Matrix, Quaternion
│   │   ├── Memory/         # Custom allocators (Stack, Pool, Linear)
│   │   └── Platform/       # Platform abstraction, types
│   └── src/
│
├── Engine/                  # Engine layer
│   ├── include/Enjin/
│   │   ├── Assets/         # Asset loading (GLTFLoader, SceneImporter)
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # Transform, Mesh, Light, Material, Name
│   │   │   └── Systems/    # RenderSystem
│   │   ├── Editor/         # EditorLayer, ScenePicker
│   │   ├── GUI/            # ImGui integration
│   │   └── Renderer/       # Vulkan renderer
│   │       └── Vulkan/     # VulkanContext, Pipeline, Buffer, etc.
│   ├── shaders/            # GLSL shaders (triangle.vert/frag)
│   └── src/
│
├── Editor/                  # Editor application (main.cpp entry point)
│
├── third_party/            # External dependencies
│   ├── imgui/              # Dear ImGui (UI)
│   └── imguizmo/           # Transform gizmos
│
└── build/                  # Build output (bin/, lib/)
```

## Key Classes and Concepts

### Core Layer

- **`Math::Vector3/4`** - 3D/4D vectors with SIMD-friendly layout
- **`Math::Matrix4`** - 4x4 matrix with LookAt, Perspective, Orthographic, Inverse
- **`Math::Quaternion`** - Rotation representation
- **`Log`** - Categorized logging: `ENJIN_LOG_INFO(Category, "msg", ...)`

### ECS (Entity-Component-System)

- **`ECS::World`** - Manages entities and components
- **`ECS::Entity`** - Just a u64 ID
- **Components:**
  - `TransformComponent` - position, rotation (Euler), scale
  - `MeshComponent` - vertices (position, normal, UV), indices
  - `MaterialComponent` - PBR properties (baseColor, metallic, roughness, emissive)
  - `LightComponent` - Light data (direction, color, intensity)
  - `NameComponent` - Entity name string

### Renderer

- **`VulkanContext`** - Vulkan instance, device, queues
- **`VulkanRenderer`** - Main renderer, swapchain management
- **`VulkanPipeline`** - Graphics pipeline with descriptor sets
- **`VulkanBuffer`** - GPU buffers (vertex, index, uniform)
- **`RenderSystem`** - ECS system that renders all entities with Mesh+Transform

### Uniform Buffer Objects (Shader Bindings)

```cpp
// Binding 0: MVP matrices (vertex shader)
struct UniformBufferObject {
    Matrix4 model, view, proj;
};

// Binding 1: Lighting (fragment shader)
struct LightingUBO {
    Vector3 ambientColor; f32 ambientIntensity;
    Vector3 cameraPos;    f32 _pad0;
    Vector3 lightDir;     f32 lightIntensity;
    Vector3 lightColor;   f32 shadowBias;
    Matrix4 lightSpaceMatrix;
    i32 shadowEnabled;    f32 _pad1[3];
};

// Binding 2: Material (fragment shader)
struct MaterialGPU {
    Vector3 baseColor;    f32 metallic;
    Vector3 emissiveColor; f32 roughness;
    f32 emissiveStrength, opacity, alphaCutoff;
    i32 flags;
};
```

### Editor

- **`EditorLayer`** - Main editor class with ImGui panels
- **`ScenePicker`** - Ray casting for entity selection (click-to-select)
- **Keyboard shortcuts:**
  - `W` - Translate gizmo
  - `E` - Rotate gizmo
  - `R` - Scale gizmo
  - `Q` - Toggle local/world space
  - `WASD` + mouse drag - Fly camera

### Assets

- **`GLTFLoader`** - Loads .gltf/.glb files into GLTFScene
- **`SceneImporter`** - Converts GLTFScene to ECS entities

## Shader Workflow

Shaders are in `Engine/shaders/` as GLSL, compiled to SPIR-V, then embedded in `ShaderData.h`:

1. Edit `triangle.vert` or `triangle.frag`
2. Compile: `glslangValidator -V triangle.frag -o triangle.frag.spv`
3. Convert to C++ array and update `ShaderData.h`
4. Rebuild engine

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- **API export:** `ENJIN_API` macro for DLL export

## Current Feature Status

**Completed:**
- Vulkan renderer with Blinn-Phong lighting
- ECS architecture
- glTF model import
- ImGui editor (hierarchy, inspector, viewport panels)
- Transform gizmos (ImGuizmo)
- Entity selection via ray casting
- PBR material system
- Fly camera controller

**In Progress:**
- Shadow mapping (infrastructure created, shader integration pending)

**Planned:**
- Scene serialization (save/load)
- Texture support
- Multiple light sources
- Post-processing effects

## Common Tasks

### Adding a new ECS Component

1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform

1. Update the UBO struct in `VulkanPipeline.h`
2. Update the GLSL shader
3. Recompile shader to SPIR-V
4. Update `ShaderData.h` with new bytecode
5. Update the system that uploads the uniform (e.g., `RenderSystem::UpdateUniformBuffer`)

### Importing a 3D model at runtime

```cpp
// In EditorLayer or game code:
SceneImporter::ImportOptions options;
options.scale = 1.0f;
auto result = SceneImporter::ImportGLTF("path/to/model.gltf", m_World, options);
if (result.success) {
    // result.entities contains all created entities
    // result.rootEntity is the root of the hierarchy
}
```

# CLAUDE.md - Enjin Engine Project Context

## Git Commit Rules

- **NEVER include a Co-Authored-By line in commits.** No byline, no attribution footer. Just the commit message.

## Overview

Enjin is a proprietary, licensable game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering capabilities.

## Build Commands

```bash
# Build on Windows (Visual Studio)
cd build && cmake .. && cmake --build . --config Release

# Build on Linux/Mac
cd build && cmake .. && make -j$(nproc)

# Clean rebuild (Linux/Mac)
cd build && make clean && make -j$(nproc)

# Reconfigure CMake (needed after adding new source files)
cd build && cmake ..

# Compile shaders (GLSL to SPIR-V)
glslangValidator -V Engine/shaders/triangle.vert -o Engine/shaders/triangle.vert.spv
glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv

# Run the editor
./build/bin/Release/EnjinEditor.exe  # Windows
./build/bin/EnjinEditor              # Linux/Mac
```

## Project Architecture

```
enjin/
├── Core/                    # Foundation layer (no engine dependencies)
│   ├── include/Enjin/
│   │   ├── Core/           # Application, Window, Input
│   │   ├── Logging/        # Thread-safe categorized logging
│   │   ├── Math/           # Vector, Matrix, Quaternion, Spline
│   │   ├── Memory/         # Custom allocators (Stack, Pool, Linear)
│   │   └── Platform/       # Platform abstraction, types
│   └── src/
│
├── Engine/                  # Engine layer
│   ├── include/Enjin/
│   │   ├── AI/             # AIBehaviors, Navmesh (stubs)
│   │   ├── Animation/      # Animation system (stub)
│   │   ├── Assets/         # GLTFLoader, SceneImporter, Prefab
│   │   ├── Audio/          # AudioSystem, SimpleAudio (stubs)
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # Transform, Mesh, Light, Material, Camera, Controllers
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Editor/         # EditorLayer, PlayMode, UndoRedo
│   │   ├── Effects/        # Weather, Water, RetroEffects
│   │   ├── GUI/            # ImGui integration
│   │   ├── Physics/        # SimplePhysics
│   │   ├── Platform/       # FileDialog
│   │   ├── Procedural/     # LevelGenerator (stub)
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
  - `MeshComponent` - vertices (position, normal, UV, color, tangent), indices
  - `MaterialComponent` - PBR properties, textures (base color, normal, height), retro flags
  - `LightComponent` - Light data (direction, color, intensity)
  - `NameComponent` - Entity name string
  - `CameraComponent` - In-game cameras with projection, weather/water settings
  - `NotesComponent` - Text annotations for entities
  - `CharacterController` - Various movement controllers (Platformer2D, TopDown2D/3D, FPS, TPS)

### Renderer

- **`VulkanContext`** - Vulkan instance, device, queues
- **`VulkanRenderer`** - Main renderer, swapchain management
- **`VulkanPipeline`** - Graphics pipeline with descriptor sets
- **`VulkanBuffer`** - GPU buffers (vertex, index, uniform)
- **`RenderSystem`** - ECS system that renders all entities with Mesh+Transform

### Descriptor Bindings

```
Binding 0: View/Projection UBO (vertex shader)
Binding 1: Lighting UBO with multi-light arrays (vertex + fragment)
Binding 2: Material UBO (fragment shader)
Binding 3: Base color texture sampler (fragment shader)
Binding 4: Shadow map sampler (fragment shader)
Binding 5: Height map for parallax mapping (fragment shader)
Binding 6: Normal map (fragment shader)
```

### Push Constants (128 bytes, per-object)

```cpp
struct PushConstants {
    Matrix4 model;          // 64 bytes
    Vector3 baseColor;      // + metallic = 16 bytes
    Vector3 emissiveColor;  // + roughness = 16 bytes
    f32 emissiveStrength, opacity, alphaCutoff;
    i32 flags;              // bit field: render/alpha/texture/retro flags
    f32 parallaxScale;      // + padding = 16 bytes
};
// flags layout: bits 0-2 render, 8-9 alpha mode, 10 height tex,
//   16-19 texture flags, 20-23 retro flags, 24-31 snap resolution
```

### Editor

- **`EditorLayer`** - Main editor class with ImGui panels
- **`ScenePicker`** - Ray casting for entity selection (click-to-select)
- **`PlayMode`** - Play/Pause/Stop game preview controls
- **Keyboard shortcuts:**
  - `W` - Translate gizmo
  - `E` - Rotate gizmo
  - `R` - Scale gizmo
  - `Q` - Toggle local/world space
  - `WASD` + mouse drag - Fly camera

### Effects Systems

- **`WeatherSystem`** - Rain, snow, fog, storm with lightning (global scene effect)
- **`Water3D`** - 3D water plane with waves (global scene effect)
- **`RetroEffects`** - CRT, pixelation, dithering post-processing
- Effects are configured globally and rendered only in Game View (not editor camera)

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
- glTF/FBX model import
- ImGui editor (hierarchy, inspector, viewport, effects panels)
- Transform gizmos (ImGuizmo)
- Entity selection via ray casting
- PBR material system
- Fly camera controller
- Scene serialization (save/load JSON)
- Post-processing effects (bloom, vignette, color grading, FXAA, film grain)
- Weather effects (rain, snow, fog, storm with toggleable lightning)
- Water effects (3D water plane with Gerstner waves)
- Camera component for in-game cameras
- Camera frustum visualization in editor
- Play mode (play/pause/stop)
- Native file dialogs (Windows, macOS, Linux)
- Multiple light sources support (directional, point, spot)
- Shadow mapping with PCF filtering
- Render-to-texture for Game View (offscreen rendering)
- Retro rendering effects (per-material flat shading, affine texturing, vertex snapping, stipple transparency)
- Retro post-processing (dithering, color quantization, resolution downscaling, CRT scanlines)
- Vertex colors for baked lighting/shadows
- Normal mapping (tangent-space, per-material)
- Parallax occlusion mapping (height map ray marching)
- Physics collision detection (sphere-sphere, AABB-AABB, sphere-AABB)
- Render graph with topological sorting
- Deferred rendering framework
- GPU-driven frustum culling
- Material system with file watching and hot-reload
- Cross-platform file dialogs (Win32, macOS osascript, Linux zenity/kdialog)
- Render scripting system (command-based DSL)
- GLSL runtime shader compilation

**Planned:**
- AI/Navmesh integration (framework exists, needs gameplay logic)
- Audio system integration (SimpleAudio works on Windows, FMOD/Wwise need SDKs)
- Skeletal animation (framework exists, needs sampling/skinning)

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

# Enjin Engine

A proprietary, licensable game engine built from scratch using Vulkan API.

## Features

- **Vulkan Renderer** - Modern graphics with Blinn-Phong lighting and PBR materials
- **Entity-Component System** - Flexible ECS architecture for game objects
- **glTF Model Import** - Load 3D models directly into the editor
- **Editor UI** - Full ImGui-based editor with hierarchy, inspector, and viewport panels
- **Transform Gizmos** - Visual manipulation tools (translate, rotate, scale) via ImGuizmo
- **Entity Selection** - Click-to-select with ray casting
- **Camera Controller** - Fly camera with keyboard/mouse controls
- **Material System** - PBR properties (baseColor, metallic, roughness, emissive)

## Project Structure

```
EnjinEngine/
├── Core/           # Foundation layer (Memory, Math, Logging, Platform)
├── Engine/         # Engine layer (Renderer, ECS, Assets, GUI)
├── Editor/         # Editor application
├── third_party/    # External dependencies (GLFW, ImGui, ImGuizmo)
└── Tests/          # Unit tests
```

## Roadmap

### Phase 1: Foundation ✅
- [x] Memory Management (Stack, Pool, Linear allocators)
- [x] Math Library (Vectors, Matrices, Quaternions, Matrix inversion)
- [x] Logging System (Thread-safe, categorized)
- [x] Platform Abstraction Layer
- [x] Entry Point Abstraction

### Phase 2: Vulkan Renderer ✅
- [x] Vulkan Context Initialization
- [x] Swapchain Management
- [x] Command Buffer System
- [x] SPIR-V Shader Pipeline
- [x] Depth Buffer / Z-testing
- [x] Blinn-Phong Lighting
- [x] Uniform Buffer Objects (MVP, Lighting, Material)

### Phase 3: Engine Core ✅
- [x] ECS (Entity Component System)
- [x] glTF Asset Loading (.gltf/.glb)
- [x] Scene Importer (glTF to ECS conversion)
- [x] Input System (Keyboard/Mouse)
- [x] Camera System (Fly camera with WASD + mouse)

### Phase 4: Editor Tooling ✅
- [x] Editor GUI (Dear ImGui integration)
- [x] Scene Hierarchy Panel
- [x] Entity Inspector Panel
- [x] Transform Gizmos (ImGuizmo - translate/rotate/scale)
- [x] Entity Selection via Ray Casting
- [x] Viewport Panel with camera controls
- [x] Settings Panel (gizmo options, render settings)
- [x] Stats Overlay (FPS, frame time)

### Phase 5: Advanced Rendering 🚧
- [x] PBR Material System (baseColor, metallic, roughness, emissive)
- [x] Alpha cutoff / transparency support
- [x] Multiple Light Sources (point, spot, directional)
- [x] Post-Processing Effects (bloom, tone mapping, vignette, color grading)
- [x] Retro Effects (PSX, CRT, dithering, vertex jitter)
- [x] Weather System (rain, snow, fog)
- [x] Water Rendering (waves, reflections, caustics)
- [ ] Shadow Mapping (infrastructure created)
- [ ] Texture Support (albedo, normal, roughness maps)
- [x] Environment Mapping / Skybox

### Phase 6: Production Features 🚧
- [x] Scene Serialization (JSON save/load)
- [ ] Undo/Redo System
- [ ] Asset Hot-Reloading
- [ ] Prefab System

### Phase 7: Animation & Audio ✅
- [x] 2D Sprite Animation (frame-based, flipbook)
- [x] 3D Skeletal Animation (bone hierarchy, skinning)
- [x] Animation Blending & State Machines
- [x] Audio System (basic)
- [x] FMOD Integration Hooks
- [x] Wwise Integration Hooks

### Phase 8: AI & Procedural Generation ✅
- [x] Spline System (Linear, Bezier, Catmull-Rom, B-Spline)
- [x] Enemy AI Behaviors (patrol, chase, flee, attack patterns)
- [x] AI State Machines (FSM with transitions)
- [x] 2D Procedural Level Generation (prefab-based with marked openings)
- [x] 3D Procedural Level Generation (room/corridor system)
- [x] Navmesh Generation & Pathfinding (A* pathfinding, path following)

### Phase 6 (continued): Production Features 🚧
- [x] Undo/Redo System (command pattern)
- [x] Prefab System (save/load entity templates)
- [ ] Asset Hot-Reloading

### Phase 9: Licensable Features 🚧
- [ ] Scripting Language (Lua or C# binding)
- [x] Physics Integration (simple built-in)
- [ ] Networking (client-server, peer-to-peer)

## Editor Controls

| Action | Control |
|--------|---------|
| Move Camera | `W/A/S/D` |
| Look Around | Right-click + drag |
| Camera Up/Down | `E` / `Q` (while moving) |
| Select Entity | Left-click in viewport |
| Translate Gizmo | `W` |
| Rotate Gizmo | `E` |
| Scale Gizmo | `R` |
| Toggle Local/World | `Q` |
| Import Model | File > Import Model |

## Skybox

The engine includes a dedicated Skybox panel (View > Skybox) for configuring the scene background.

**Supported types:**
- **None** - No skybox rendered
- **Procedural** - Gradient sky with configurable top, horizon, and bottom colors plus sun direction
- **Solid Color** - Single flat color fill
- **Cubemap** - Six-face cubemap with individual texture paths (Right, Left, Top, Bottom, Front, Back)

**Procedural presets:**
Quick-apply presets that configure colors and sun direction in one click:
- **Midday** - Bright blue sky with overhead sun
- **Sunset** - Warm orange horizon with low sun
- **Dawn** - Soft pinks and purples with rising sun
- **Night** - Deep dark sky with sun below horizon
- **Overcast** - Flat grey tones with diffused light

All non-None types support a rotation slider (0-360 degrees) around the Y axis. Skybox configuration is persisted with scene save/load, including sun direction.

## Building

### Prerequisites
- CMake 3.20+
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Vulkan SDK
- GLFW3

### Build Instructions

**Linux:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

**Windows:**
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

**See [WINDOWS_BUILD.md](docs/WINDOWS_BUILD.md) for detailed Windows instructions.**

### Build Options
- `ENJIN_BUILD_EDITOR=ON` - Build the editor (default: ON)
- `ENJIN_BUILD_TESTS=OFF` - Build unit tests (default: OFF)
- `ENJIN_BUILD_EXAMPLES=OFF` - Build example projects (default: OFF)

## License

Proprietary - All rights reserved.

## Technology Stack

- **Language**: C++20
- **Graphics API**: Vulkan 1.3
- **Windowing**: GLFW3 (zlib/libpng license - permissive)
- **Build System**: CMake

## License Compatibility

All dependencies use permissive licenses compatible with proprietary licensing:
- GLFW3: zlib/libpng (permissive)
- Vulkan SDK: Apache 2.0 (permissive)
- Dear ImGui: MIT (permissive)
- ImGuizmo: MIT (permissive)

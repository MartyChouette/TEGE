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
- [ ] Shadow Mapping (infrastructure created)
- [ ] Texture Support (albedo, normal, roughness maps)
- [ ] Multiple Light Sources (point, spot, directional)
- [ ] Environment Mapping / Skybox
- [ ] Post-Processing Effects (bloom, tone mapping)

### Phase 6: Production Features
- [ ] Scene Serialization (JSON save/load)
- [ ] Undo/Redo System
- [ ] Asset Hot-Reloading
- [ ] Prefab System

### Phase 7: Licensable Features
- [ ] Scripting Language (Lua or C# binding)
- [ ] Physics Integration (Jolt or custom)
- [ ] Audio System
- [ ] Animation System
- [ ] Networking

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

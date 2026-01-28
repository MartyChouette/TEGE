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
- [x] Math Library (Vectors, Matrices, Quaternions)
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

### Phase 3: Engine Core ✅
- [x] ECS (Entity Component System)
- [x] glTF Asset Loading
- [x] Input System (Keyboard/Mouse)
- [x] Camera System

### Phase 4: Tooling ✅
- [x] Editor GUI (Dear ImGui)
- [x] Scene Hierarchy Panel
- [x] Entity Inspector Panel
- [x] Transform Gizmos (ImGuizmo)
- [x] Entity Selection / Picking

### Phase 5: Advanced Rendering 🚧
- [x] PBR Material System
- [ ] Shadow Mapping (in progress)
- [ ] Texture Support
- [ ] Multiple Light Sources
- [ ] Post-Processing Effects

### Phase 6: Licensable Features
- [ ] Scene Serialization (Save/Load)
- [ ] Scripting Language (C#/Lua)
- [ ] Physics Integration
- [ ] Audio System

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

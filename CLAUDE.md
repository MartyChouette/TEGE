# CLAUDE.md - Enjin Engine Project Context

## Git Commit Rules

- **NEVER include a Co-Authored-By line in commits.** No byline, no attribution footer. Just the commit message.

## Overview

Enjin is an open-source (BSL 1.1) game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering capabilities.

## Build Commands

```bash
# Build on Windows (Visual Studio)
cd build && cmake .. && cmake --build . --config Release

# Build on Linux/Mac
cd build && cmake .. && make -j$(nproc)

# Reconfigure CMake (needed after adding new source files)
cd build && cmake ..

# Compile shaders (GLSL to SPIR-V)
glslangValidator -V Engine/shaders/triangle.vert -o Engine/shaders/triangle.vert.spv

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
│   │   ├── Math/           # Vector, Matrix, Quaternion, Spline, Noise
│   │   ├── Memory/         # Custom allocators (Stack, Pool, Linear)
│   │   └── Platform/       # Platform abstraction, types
│   └── src/
├── Engine/                  # Engine layer
│   ├── include/Enjin/
│   │   ├── AI/             # AIBehaviors, Navmesh, A* Pathfinding
│   │   ├── Animation/      # Sprite + skeletal animation, Timeline/Sequencer
│   │   ├── Assets/         # GLTFLoader, SceneImporter, Prefab
│   │   ├── Audio/          # SimpleAudio (miniaudio), SteamAudioProcessor (HRTF)
│   │   ├── ECS/            # Entity-Component-System (70+ component types)
│   │   │   ├── Components/ # Transform, Mesh, Material, Light, Camera, etc.
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Editor/         # EditorLayer, PlayMode, Settings, Tools
│   │   ├── Effects/        # Weather, Water, RetroEffects, Particles, WorldTime
│   │   ├── GUI/            # UICanvas, UISystem, DialogueTree
│   │   ├── Build/          # BuildPipeline, AssetPacker, AssetReader
│   │   ├── Gameplay/       # SaveSystem, QuestSystem, HUD, Cinematics
│   │   ├── Networking/     # LAN Multiplayer, HTTPClient, NewgroundsAPI
│   │   ├── Physics/        # IPhysicsBackend (Jolt/Box2D/Simple)
│   │   ├── Renderer/       # Vulkan renderer + RayTracing pipeline
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   ├── Scripting/      # AngelScript engine, ScriptBindings
│   │   └── VisualScript/   # Blueprint-style visual scripting
│   ├── shaders/            # GLSL shaders
│   └── src/
├── Editor/                  # Editor application (main.cpp entry point)
├── Player/                  # Standalone game player (no editor/ImGui)
├── third_party/            # External dependencies (imgui, imguizmo)
└── build/                  # Build output (bin/, lib/)
```

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Effects`, `Enjin::Accessibility`, `Enjin::InputSystem`, `Enjin::Build`, `Enjin::Gameplay`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)` — categories: Build, Player, Network, Script, etc.
- **API export:** `ENJIN_API` macro for DLL export
- **Important:** `InputSystem` namespace (not `Input`) to avoid collision with the existing `Enjin::Input` class.

## Key Classes

### ECS

- **`ECS::World`** - Entity/component manager. Thread-safe structural ops. `DestroyEntity()` is deferred (flushed at `Update()` start). `IsValid()` returns false for pending-destruction entities.
- **`ECS::Entity`** - u64 ID
- **Key Components:** `TransformComponent` (position, rotation, scale, visible), `MeshComponent`, `MaterialComponent` (PBR + textures + transmission/IOR/thickness/sssIntensity/sssRadius/sssColor; MaterialGPU = 80 bytes), `LightComponent` (no direction field — extract from TransformComponent rotation), `NameComponent`, `CameraComponent`, `NotesComponent` (field: `.notes` not `.text`), `AnimatorComponent`, colliders (`Box/Sphere/Capsule` with `categoryBits`/`collisionMask` bitmask filtering), `PostProcessVolumeComponent`

### Collision Filtering

Bilateral bitmask: `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`. Defaults: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF`. Up to 32 named groups in `SceneManager::m_CollisionGroupNames`.

### Physics

`IPhysicsBackend`/`IPhysicsBackend2D` interfaces. Backends: Jolt v5.2.0 (3D), Box2D v3.0.0 (2D), SimplePhysics (legacy, compile-guarded). `PhysicsBackendFactory` creates via type enum (`Auto`/`Jolt`/`Box2D`/`Simple`). CMake: `ENJIN_PHYSICS_JOLT` (ON), `ENJIN_PHYSICS_BOX2D` (ON), `ENJIN_PHYSICS_SIMPLE` (ON). **Sensor bodies** (`Body2DComponent::isSensor = true`): Box2D syncs positions from ECS (not to ECS), enabling collision callbacks for controller/AI/tween-driven entities without Box2D overwriting their positions.

### Renderer

- **Descriptor Bindings:** 0=ViewProj UBO, 1=Lighting UBO, 2=Material UBO, 3=Base color tex, 4=Shadow map array, 5=Height map, 6=Normal map, 7=Bone SSBO, 8=Metallic-roughness tex, 9=Emissive tex, 10=Point shadow cubemaps, 11=Spot shadow maps, 12=Shadow data SSBO, 13=Object data SSBO, 14=Cluster grid SSBO (clustered lighting), 15=Cluster light index SSBO (clustered lighting), 16=VT indirection tex, 17=VT physical atlas
- **Push Constants (128 bytes):** model matrix (64B), baseColor+metallic, emissiveColor+roughness, emissiveStrength, opacity, alphaCutoff, flags (bitfield), parallaxScale
- **Flags layout:** bits 0-2 render, 3 skinned, 4 wind, 5-7 water, 8-9 alpha mode, 10 height tex, 11 ocean, 12 UV quantize, 13 gouraud, 14-15 shadow dither, 16-19 texture flags, 20-23 retro flags, 24-28 snap resolution (/8), 29-31 shadow dither pattern
- **Scene classification:** `Scene2D` (sprites only, shadows skipped), `Scene2_5D` (sprites+lights), `Scene3D` (full pipeline)
- **Ray tracing:** Full RT pipeline (shadows/reflections/AO/GI/translucency/caustics/path tracing, SVGF+OIDN+OptiX denoisers). RT descriptor set: 16 bindings (0-13 base, 14=translucency, 15=caustics). RTCompositor enable flags: bits 0-5 (shadows/reflections/AO/GI/translucency/caustics). Auto-activates on RT-capable hardware. CMake: `ENJIN_RAYTRACING_OIDN`, `ENJIN_RAYTRACING_OPTIX`.
- **Performance optimizations:** Clustered forward lighting (16x9x24 grid, bindings 14-15), Variable Rate Shading (`VK_KHR_fragment_shading_rate`), Virtual Texturing (page-based streaming, bindings 16-17), Visibility Buffer (deferred material resolve). GPU two-phase HiZ occlusion culling, async compute overlap, per-frame linear allocator, 64-bit material sort keys, LOD hysteresis. CMake: `ENJIN_CLUSTERED_LIGHTING` (ON), `ENJIN_VRS` (OFF), `ENJIN_VIRTUAL_TEXTURING` (OFF), `ENJIN_VISIBILITY_BUFFER` (OFF).

### Editor

- **Settings:** Unified 3-tab window (System/Project/Scene). `OpenSettings(tab)` for programmatic tab selection. Bit 5 = canonical visibility.
- **PlayMode:** Play/Pause/Stop. Scene changes persist on Stop. `PlayModeDiff` shows what changed.
- **Shortcuts:** `1/2/3` gizmo modes, `WASD` fly cam, `Delete` delete, `Ctrl+D` duplicate, `F` focus, `Ctrl+P` command palette

## Shader Workflow

1. Edit GLSL in `Engine/shaders/`
2. Compile: `glslangValidator -V shader.frag -o shader.frag.spv`
3. Convert to C++ array → update `ShaderData.h`
4. Rebuild engine

Hot-reload in editor: `RenderSystem` watches `Engine/shaders/` via `FileWatcher`, auto-recompiles on change.

## Common Tasks

### Adding a new ECS Component
1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform
1. Update UBO struct (`Light.h` for LightingUBO, `VulkanPipeline.h` for ViewProjectionUBO)
2. Update GLSL shader → recompile → update `ShaderData.h`
3. Update the system that uploads the uniform

### Building a game for export
1. Configure `BuildConfig` with project path, output directory, window settings
2. Run `BuildPipeline::Execute(config)` — packs into `.enjpak`
3. Player loads `game.enjpak` from its own directory at startup

## Security

- **Trust zones** documented in `.enjin-boundaries.json` — security-critical (networking, scripts, assets), trust-boundary (serializers, bindings), user-api (components, script API), editor-internal, renderer-internals, gameplay-runtime, foundation
- **Scene files:** Validate array sizes, check `.contains()` before access
- **Scripts:** AngelScript sandboxed, 1M instruction limit
- **Asset packs:** XOR obfuscation (not crypto-secure), CRC32 integrity only
- **General:** Validate enum casts, sanitize file paths, cap allocation sizes

## Further Reading

- `docs/ARCHITECTURE.md` - System architecture and diagrams
- `docs/CLAUDE_REFERENCE.md` - Detailed subsystem documentation (RT pipeline, physics backends, Steam Audio, editor features, scripting bindings, feature list, perf history)
- `docs/SCRIPTING_API.md` - Complete AngelScript API reference
- `docs/USER_MANUAL.md` - Component details and user guide
- `docs/ROADMAP.md` - Planned work and progress tracking
- `docs/BUILD.md` - Build guide with dependencies

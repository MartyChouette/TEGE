# CLAUDE.md - Enjin Engine Project Context

## Git Commit Rules

- **NEVER include a Co-Authored-By line in commits.** No byline, no attribution footer. Just the commit message.

## Overview

Enjin is an open-source (BSL 1.1) game engine built from scratch using C++20. It features a complete editor with ImGui, an Entity-Component-System architecture, and modern rendering with multi-backend support (Vulkan, WebGPU, Metal planned).

## Traps & Rules

These are hard-won lessons. Violating any of these will cause bugs.

### Naming & API
- **`InputSystem` namespace**, not `Input` — `Enjin::Input` is an existing class
- **`NotesComponent` field is `.notes`**, not `.text`
- **`LightComponent` has no direction field** — extract direction from `TransformComponent` rotation
- **Log categories:** `Network` (not `Networking`), `Script` (not `Scripting`), `Build`, `Player`
- **Math headers:** `Enjin/Math/Vector.h` (not `Vector3.h`). `Matrix4` uses flat `f32 m[16]`
- **`SceneSerializer`** requires `World*` in constructor

### ECS
- **`DestroyEntity()` is deferred** — flushed at `World::Update()` start. `IsValid()` returns false for pending-destruction entities
- **`MeshComponent.subMeshes`** array for multi-material meshes. `MaterialSlotsComponent` holds per-sub-mesh materials

### Physics
- **STRICT 2D/3D separation:** Box2D for 2D scenes only, Jolt for 3D only. Never mix. 2D controllers use `CheckGround2D`/`CheckWall2D`, 3D use `CheckGround`/`CharacterVirtual`
- **Collider sizes are WORLD SPACE:** Jolt/Box2D do NOT multiply by transform scale. `BoxColliderComponent.size = (50, 0.1, 50)` means 50 world units regardless of entity scale
- **Capsule height convention:** `height` = cylinder only (between hemispheres). Total = `height + 2*radius`. For `CharacterVirtual`: `totalHalfH = height/2 + radius`
- **Box2D kinematic bodies:** Use `b2Body_SetLinearVelocity` (not `SetTransform`) — teleporting doesn't trigger sensor events
- **Box2D sensor events:** `enableSensorEvents` must be `true` even on static shapes. Formula: `enableSensorEvents = isSensor || !isStatic`
- **2D raycasts skip sensors:** `Box2DBackend::Raycast` filters out sensor bodies by design
- **Hazard sensors** must be `isKinematic = true` (not `isStatic`). Box2D v3 doesn't fire events between static sensors and kinematic visitors. Use `gravityScale = 0`
- **`CheckHazardOverlaps`** is a manual AABB check each frame — workaround for Box2D v3 kinematic-kinematic sensor limitation
- **3D character controllers:** Use `JPH::CharacterVirtual` (not manual raycasts). Self-excluded from own raycasts via `EnjinBodyFilter.ignoreBodyID`

### Renderer
- **Descriptor set binding 2** is `STORAGE_BUFFER_DYNAMIC` — ALL `vkCmdBindDescriptorSets` calls MUST pass `dynamicOffsetCount=1` with a `u32` offset for the material SSBO
- **WGSL rule:** `textureSample` must be called from uniform control flow — never inside `if` branches that depend on per-vertex/per-fragment data
- **Render pass formats:** Swapchain = `B8G8R8A8_SRGB` with MRT. Offscreen `RenderTarget`s = `B8G8R8A8_UNORM`, single color + depth, `colorAttachmentCount=1`, `SAMPLE_COUNT_1_BIT` (no MSAA)
- **WebGPU depth-only pass:** `Depth32Float`, no stencil — stencil ops MUST be `Undefined`. Shadow pipeline has no fragment shader
- **WebGPU tangent fallback:** PBR shader checks `dot(tangent,tangent) > 0.001` — without this, `normalize(vec3(0))` produces NaN and kills all lighting
- **`MaterialGPU` = 80 bytes** — struct alignment matters for SSBO offsets

### Shaders (CRITICAL)
- After ANY change to `LightingUBO`, `UniformBufferObject`, `MaterialGPU`, or other UBO/SSBO structs: **recompile ALL affected shaders AND regenerate `ShaderData.h`**. Stale SPIR-V = GPU reading wrong offsets (dark scenes, wrong colors, crashes)
- Shader edit workflow: edit GLSL → `glslangValidator -V` → `python _gen_all.py` → rebuild. No shortcuts

### Windows C++ Gotchas
- `near` and `far` are reserved macros (`windef.h`) — don't use as variable names

## Build Commands

```bash
# Build on Windows (Visual Studio)
cd build && cmake .. && cmake --build . --config Release

# Build on Linux/Mac
cd build && cmake .. && make -j$(nproc)

# Build for Web (WebAssembly + WebGPU)
# Requires Emscripten SDK at D:/emsdk
export EMSDK=/d/emsdk && export EMSDK_PYTHON="$EMSDK/python/3.13.3_64bit/python.exe"
"$EMSDK_PYTHON" "$EMSDK/upstream/emscripten/emcmake.py" cmake -B build-web -S . -DENJIN_PLATFORM_WEB=ON
"$EMSDK_PYTHON" "$EMSDK/upstream/emscripten/emmake.py" cmake --build build-web
# Output: build-web/bin/EnjinPlayer.{js,wasm}

# Reconfigure CMake (needed after adding new source files)
cd build && cmake ..

# Run the editor
./build/bin/Release/EnjinEditor.exe  # Windows
./build/bin/EnjinEditor              # Linux/Mac

# Serve web demo
cd web-demo && python serve.py  # http://localhost:9090
```

## Testing

- **Framework:** Custom — `ENJIN_TEST(Suite, Name)`, `ENJIN_EXPECT_*`, `ENJIN_ASSERT_*`
- **No `ENJIN_ASSERT_GT`** — use `ENJIN_ASSERT_TRUE(x > y)` instead
- **Run all:** `cd build && ctest --output-on-failure`
- **Run one suite:** `cd build && ctest -R TestPhysics --output-on-failure`
- **~80 CTest targets, ~1100+ test cases** across 18 subdirectories
- **4 tests require environment:** TestAudio, TestAudioTypes, TestAssetPack, TestAssetLoaders (may show "Not Run")

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Effects`, `Enjin::Accessibility`, `Enjin::InputSystem`, `Enjin::Build`, `Enjin::Gameplay`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- **API export:** `ENJIN_API` macro for DLL export

## Common Tasks

### Adding a new ECS Component
1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform
1. Update C++ UBO struct (`Light.h` for LightingUBO, `VulkanPipeline.h` for ViewProjectionUBO)
2. Update the matching GLSL struct in the shader
3. Compile: `glslangValidator -V Engine/shaders/triangle.frag -o Engine/shaders/triangle.frag.spv` (and all affected shaders)
4. Regenerate: `python _gen_all.py`
5. Update the system that uploads the uniform
6. Rebuild engine

### Building a game for export
1. Configure `BuildConfig` with project path, output directory, window settings
2. Run `BuildPipeline::Execute(config)` — packs into `.enjpak`
3. Player loads `game.enjpak` from its own directory at startup

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
│   │   ├── Animation/      # Sprite + skeletal animation, BlendTree, Retargeting
│   │   ├── Assets/         # GLTFLoader, AssimpLoader, SceneImporter, Prefab
│   │   ├── Audio/          # SimpleAudio (miniaudio), SteamAudioProcessor
│   │   ├── ECS/            # Entity-Component-System (80+ component types)
│   │   │   ├── Components/ # Transform, Mesh, Material, Light, Camera, etc.
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Editor/         # EditorLayer, PlayMode, Settings, Tools
│   │   ├── Effects/        # Weather, Water, RetroEffects, Particles
│   │   ├── GUI/            # UICanvas, UISystem, DialogueTree
│   │   ├── Build/          # BuildPipeline, AssetPacker, AssetReader
│   │   ├── Gameplay/       # SaveSystem, QuestSystem, HUD, Cinematics
│   │   ├── Networking/     # LAN Multiplayer, HTTPClient, NewgroundsAPI
│   │   ├── Physics/        # IPhysicsBackend (Jolt/Box2D)
│   │   ├── Renderer/       # Multi-backend renderer (Vulkan/WebGPU/Metal)
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   ├── Scripting/      # AngelScript engine, ScriptBindings
│   │   └── VisualScript/   # Blueprint-style visual scripting
│   ├── shaders/            # GLSL + WGSL shaders
│   └── src/
├── Editor/                  # Editor application (main.cpp entry point)
├── Player/                  # Standalone game player (no editor/ImGui)
├── third_party/            # External dependencies (imgui, imguizmo)
└── build/                  # Build output (bin/, lib/)
```

## Key Subsystem Notes

### Renderer
- **Multi-backend:** `IRenderBackend` with sub-interfaces (`IGPUBufferManager`, `IGPUTextureManager`, `IGPUPipelineManager`, etc.). Typed opaque handles in `GPUTypes.h`. Feature detection in `GPUCapabilities.h`
- **Backends:** Vulkan (Windows/Linux, full features), WebGPU (browser via Emscripten, PBR + shadows working), Metal (stubs)
- **Scene classification:** `Scene2D` (sprites only, shadows skipped), `Scene2_5D` (sprites+lights), `Scene3D` (full pipeline)
- **Ray tracing:** Full RT pipeline (shadows/reflections/AO/GI/path tracing, denoisers). Auto-activates on capable hardware

### Physics
- `IPhysicsBackend`/`IPhysicsBackend2D` interfaces. Jolt v5.2.0 (3D), Box2D v3.0.0 (2D)
- `PhysicsBackendFactory` creates via `PhysicsBackendType` enum (`Auto`/`Jolt`/`Box2D`)
- CMake: `ENJIN_PHYSICS_JOLT` (ON), `ENJIN_PHYSICS_BOX2D` (ON)
- Collision filtering: bilateral bitmask `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`. Defaults: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF`

### Editor
- Settings: 3-tab window (System/Project/Scene). `OpenSettings(tab)` for programmatic tab selection
- PlayMode: Play/Pause/Stop. Scene changes persist on Stop. `PlayModeDiff` shows what changed
- Shortcuts: `1/2/3` gizmo modes, `WASD` fly cam, `Delete` delete, `Ctrl+D` duplicate, `F` focus, `Ctrl+P` command palette, `F1` Game Debug, `F2` Debug Workstation, `` ` `` console

## Security

- **Scene files:** Validate array sizes, `.contains()` before access. Vertex/index caps (10M), strings via `SafeStr()`
- **Scripts:** AngelScript sandboxed, 1M instruction limit. `#include` paths not yet restricted
- **Asset packs:** XOR obfuscation (not crypto-secure), CRC32 integrity. Path traversal rejected
- **Process execution:** No `std::system()` — use `ShellExecuteA` (Win), `fork`/`execlp` (Linux/macOS)
- **Thread safety:** See `docs/AUDIT_2026_04_12.md` for open issues
- **Trust zones:** Documented in `.enjin-boundaries.json`

## Further Reading

- `docs/CLAUDE_REFERENCE.md` - Detailed subsystem docs (component catalog, binding tables, RT pipeline, physics, audio, scripting, editor details, perf history)
- `docs/ARCHITECTURE.md` - System architecture and diagrams
- `docs/SCRIPTING_API.md` - Complete AngelScript API reference
- `docs/USER_MANUAL.md` - Component details and user guide
- `docs/ROADMAP.md` - Planned work and progress tracking
- `docs/BUILD.md` - Build guide with dependencies
- `docs/AUDIT_2026_04_12.md` - Full engine audit (55 findings)

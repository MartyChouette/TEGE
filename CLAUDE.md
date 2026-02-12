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

See `docs/BUILD.md` for full build guide with dependencies and troubleshooting.

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
│
├── Engine/                  # Engine layer
│   ├── include/Enjin/
│   │   ├── AI/             # AIBehaviors, Navmesh, A* Pathfinding
│   │   ├── Animation/      # Sprite + skeletal animation, Timeline/Sequencer
│   │   ├── Assets/         # GLTFLoader, SceneImporter, Prefab
│   │   ├── Audio/          # AudioSystem, SimpleAudio (miniaudio backend)
│   │   ├── Debug/          # Profiler, ScopeTimer, FrameData
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # 70+ component types (incl. joints, ragdoll, script, LOD)
│   │   │   │   ├── Controllers/  # 5 character controller types + Vehicle
│   │   │   │   └── ...
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Accessibility/  # ColorblindFilter, SubtitleSystem, ContentWarning
│   │   ├── Editor/         # EditorLayer, PlayMode, PlayModeDiff, EditorSettings, FeedbackSystem, VectorDrawingEditor, PerformanceStats
│   │   ├── Effects/        # Weather, Water, RetroEffects, WorldTime, Particles
│   │   ├── Input/          # InputAction (remappable input action map)
│   │   ├── GUI/            # ImGui integration, UICanvas, UISystem, DialogueTree
│   │   ├── Build/          # BuildPipeline, AssetPacker, AssetReader
│   │   ├── Gameplay/       # TieredSaveSystem, SaveBackend, SaveLoadMenu, HUDSystem, QuestSystem, ObjectPool, CinematicSystem
│   │   ├── Networking/     # HTTPClient, LANMultiplayer, NewgroundsAPI, NewgroundsSaveBackend, SteamSaveBackend
│   │   ├── Physics/        # IPhysicsBackend, SimplePhysics, PhysicsWorld, ConstraintSolver, Physics2D
│   │   ├── Plugin/         # PluginSystem, HotReload
│   │   ├── Procedural/     # LevelGenerator
│   │   ├── Renderer/       # Vulkan renderer, RenderBackend abstraction
│   │   │   ├── Vulkan/     # VulkanContext, Pipeline, Buffer, etc.
│   │   │   └── RayTracing/ # RT pipeline, acceleration structures, denoiser
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   ├── Scripting/      # ScriptEngine, ScriptBindings, TegeBehavior
│   │   └── VisualScript/   # NodeDefinition, NodeRegistry, VisualScriptExecutor
│   ├── shaders/            # GLSL shaders (triangle.vert/frag)
│   └── src/
│
├── Editor/                  # Editor application (main.cpp entry point)
├── Player/                  # Standalone game player (no editor/ImGui)
├── third_party/            # External dependencies (imgui, imguizmo)
└── build/                  # Build output (bin/, lib/)
```

See `docs/ARCHITECTURE.md` for detailed system architecture.

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Effects`, `Enjin::Accessibility`, `Enjin::InputSystem`, `Enjin::Build`, `Enjin::Gameplay`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)` — categories include Build, Player
- **API export:** `ENJIN_API` macro for DLL export
- **Important:** The `InputSystem` namespace was chosen to avoid collision with the existing `Enjin::Input` class. Always use `InputSystem::` not `Input::` for action-map types.

## Key Classes and Concepts

### Core Layer

- **`Math::Vector3/4`** - 3D/4D vectors with SIMD-friendly layout
- **`Math::Matrix4`** - 4x4 matrix with LookAt, Perspective, Orthographic, Inverse
- **`Math::Quaternion`** - Rotation representation
- **`Log`** - Categorized logging: `ENJIN_LOG_INFO(Category, "msg", ...)`

### ECS (Entity-Component-System)

- **`ECS::World`** - Manages entities and components
- **`ECS::Entity`** - Just a u64 ID
- **Key Components:**
  - `TransformComponent` - position, rotation (Euler), scale, `visible` bool
  - `MeshComponent` - vertices (position, normal, UV, color, tangent, boneWeights, boneIndices), indices
  - `MaterialComponent` - PBR properties, textures (base color, normal, height), retro flags
  - `LightComponent` - Light data (direction, color, intensity)
  - `NameComponent` - Entity name string
  - `CameraComponent` - In-game cameras with projection
  - `NotesComponent` - Text annotations (field: `.notes`, not `.text`)
  - `AnimatorComponent` - Skeletal animation playback
  - `CharacterController` - Movement controllers (Platformer2D, TopDown2D/3D, FPS, TPS)
  - `BoxColliderComponent` / `SphereColliderComponent` / `CapsuleColliderComponent` - Colliders with `categoryBits`/`collisionMask` bitmask filtering
  - `SaveDataComponent` - Persistence marker with `PersistenceTier` (SceneState/RunState/MetaProgression), custom tags, and key-value data
  - `SaveLoadMenuComponent` - In-game save/load grid overlay with configurable columns, mode (Save/Load), and slot display

### Collision Filtering

Bitmask system: `categoryBits` (which groups it belongs to) and `collisionMask` (which groups it collides with). Bilateral rule: `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`. Defaults: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF`. Up to 32 named groups stored in `SceneManager::m_CollisionGroupNames`. Old `layer` field migrated to `categoryBits` in deserialization.

### Physics Backend Abstraction

`IPhysicsBackend` (3D) and `IPhysicsBackend2D` (2D) are abstract interfaces for physics implementations. `SimplePhysicsBackend` and `SimplePhysicsBackend2D` wrap the existing engines. `JoltBackend` wraps Jolt Physics v5.2.0 with full ECS↔Jolt synchronization, thread-safe contact events, bilateral collision filtering, 6 joint types, gravity zones, and CCD support. `Box2DBackend` wraps Box2D v3.0.0 (C API with handle-based IDs) for production-grade 2D physics — multi-threaded sub-stepping, robust constraint solving, 5 joint types (Revolute/Prismatic/Distance/Rope/Weld), contact+sensor event polling, raycasting, overlap queries, CCD, and bilateral collision filtering. `PhysicsBackendFactory` creates backends via `CreatePhysicsBackend(type, mode)` / `CreatePhysicsBackend2D(type, mode)`. `PhysicsBackendType` enum: `Auto`, `Jolt`, `Box2D`. When `ENJIN_PHYSICS_JOLT=ON`, Auto selects Jolt for 3D/Mixed modes. When `ENJIN_PHYSICS_BOX2D=ON`, Auto selects Box2D for 2D/Mixed modes. PlayMode and Player own physics via `unique_ptr<IPhysicsBackend>`. All consumers (ControllerSystem, ScriptBindings, VisualScriptExecutor, NodeRegistry) accept `IPhysicsBackend*`. CMake options: `ENJIN_PHYSICS_JOLT`, `ENJIN_PHYSICS_BOX2D` (both OFF by default, FetchContent for Jolt v5.2.0, Box2D v3.0.0).

### Project Mode (2D/3D)

`ProjectMode` enum: `Mode2D`, `Mode3D`, `Mixed`. Stored in `.enjinproject`. Components tagged with `DimensionTag` (Any/Only2D/Only3D) for Add Component filtering. Grid orientation: 2D = XY plane, 3D/Mixed = XZ plane.

### Scene Composition & 2D/3D Pipeline

`ClassifySceneComposition()` runs each frame and classifies the scene as `SceneRenderMode::Scene2D`, `Scene2_5D`, or `Scene3D` based on entity types present. This drives automatic pipeline optimizations:

- **Scene2D** (sprites/tilemaps only): Shadow passes (directional CSM, point, spot) are skipped entirely. `UpdateFrameUniforms()` early-returns after uploading a minimal LightingUBO (ambient/fog only, zero lights/shadows). Normal map descriptor writes (binding 6) are skipped in the unlit sprite texture bind callback.
- **Scene2_5D** (sprites + lights, no 3D meshes): Shadows skipped, but full lighting UBO is populated so lit sprites respond to lights. Normal map descriptor is bound.
- **Scene3D** (3D meshes present): Full pipeline — all shadow passes, lighting, normal maps.

A diagnostic warning logs every 300 frames if active cameras have mixed projection types (perspective + orthographic).

### Renderer

- **`VulkanContext`** - Vulkan instance, device, queues
- **`VulkanRenderer`** - Main renderer, swapchain management
- **`VulkanPipeline`** - Graphics pipeline with descriptor sets
- **`VulkanBuffer`** - GPU buffers (vertex, index, uniform, storage)
- **`RenderSystem`** - ECS system that renders all entities with Mesh+Transform

### Descriptor Bindings

```
Binding 0: View/Projection UBO (vertex shader)
Binding 1: Lighting UBO with multi-light arrays (vertex + fragment)
Binding 2: Material UBO (fragment shader)
Binding 3: Base color texture sampler (fragment shader)
Binding 4: Shadow map array sampler - 2D array for CSM cascades (fragment shader)
Binding 5: Height map for parallax mapping (fragment shader)
Binding 6: Normal map (fragment shader)
Binding 7: Bone matrix SSBO for skeletal animation (vertex shader)
```

### Ray Tracing Pipeline

Full Vulkan RT pipeline with hybrid raster+RT rendering. Code is complete; `RTShaderData.h` has placeholder SPIR-V stubs — once compiled, the system activates automatically.

- **`RTCapabilities`** - Extension detection (`Query(VkPhysicalDevice)`) and properties. VulkanContext adds +1000 score for RT-capable GPUs, enables RT extensions via VkPhysicalDeviceFeatures2 pNext chain
- **`AccelerationStructureManager`** - BLAS cache by mesh hash (`RegisterMesh()`), per-frame TLAS rebuild (`BuildTLAS()`), instance management. Uses `vkGetDeviceProcAddr` for all RT functions
- **`RTPipeline`** - RT pipeline + shader binding table (SBT) construction
- **RT Effects** - `RTShadows` (R16F), `RTReflections` (RGBA16F), `RTAmbientOcclusion` (R16F), `RTGlobalIllumination` (RGBA16F), `PathTracer` (progressive accumulation). Each has Initialize/Dispatch/Shutdown + config struct
- **`SVGFDenoiser`** - 3-pass compute: temporal accumulation (alpha=0.05), variance estimation (3x3 box), a-trous wavelet (5 iterations, ping-pong buffers)
- **`RTCompositor`** - Fullscreen compute shader composites RT layers into scene HDR
- **Integration** - `RenderSystem::InitializeRayTracing()` creates RT descriptor set (14 bindings), all subsystems. Runs after shadow pass, before main render pass. Only for Scene3D mode. Stub detection skips pipeline creation if SPIR-V is placeholder
- **Editor** - "Ray Tracing" TreeNode in Rendering settings: supported indicator, enable toggle, mode dropdown (Hybrid/Path Trace), per-effect config sliders, path tracer SPP progress, SVGF settings, BLAS/instance stats
- **`SceneRenderSettings`** - 24 RT config fields with full JSON serialize/deserialize and CaptureFromRuntime/ApplyToRuntime

**RT Descriptor Set (Set 1)**:
```
Binding 0:  ACCELERATION_STRUCTURE (TLAS)
Binding 1:  STORAGE_IMAGE (Scene HDR)
Binding 2:  COMBINED_IMAGE_SAMPLER (Depth)
Binding 3:  COMBINED_IMAGE_SAMPLER (World normals)
Binding 4:  COMBINED_IMAGE_SAMPLER (Motion vectors)
Binding 5:  STORAGE_IMAGE (RT Shadow output)
Binding 6:  STORAGE_IMAGE (RT Reflection output)
Binding 7:  STORAGE_IMAGE (RT AO output)
Binding 8:  STORAGE_IMAGE (RT GI output)
Binding 9:  STORAGE_BUFFER (Material data)
Binding 10: STORAGE_BUFFER (Vertex data)
Binding 11: STORAGE_BUFFER (Index data)
Binding 12: STORAGE_BUFFER (Per-instance transforms)
Binding 13: UNIFORM_BUFFER (Light data)
```

**20 GLSL Shaders** (`Engine/shaders/`): `rt_common.glsl`, `rt_shadow/reflect/ao/gi/pathtrace .rgen/.rmiss/.rchit`, `svgf_temporal/variance/atrous.comp`, `rt_composite.comp`

### Sprite Texture Atlas

`SpriteTextureAtlas` auto-packs small sprite textures (<=512px) into a single 4096x4096 GPU texture at runtime using shelf packing. Owned by `RenderSystem` (`m_SpriteAtlas`), wired into `SpriteBatchRenderer` via `SetAtlas()`. Sprites sharing the atlas render in one instanced draw call instead of one per unique texture.

- **Request/Build cycle:** `RenderSprites()` calls `RequestTexture()` for every sprite each frame (sets dirty flag on new paths), then `Build()` if dirty. Build loads pixels via stb_image, sorts by height descending, shelf-packs with 1px padding, uploads to GPU with `CLAMP_TO_EDGE` / no mipmaps.
- **Batch key:** Atlased sprites use `"__atlas__"` as the effective texture key in `SpriteBatchRenderer::Render()`, grouping them into one batch. The texture bind callback in `RenderSprites()` handles the sentinel by binding `m_SpriteAtlas->GetAtlasTexture()`.
- **UV remapping:** Per-instance UVs (including sprite sheet frame sub-rects) are linearly remapped into the atlas region: `uvOut = regionStart + uvIn * regionSize`.
- **Exclusions:** Textures >512px, failed loads, or atlas overflow go into `m_ExcludedPaths` and fall back to individual draw calls.
- **Hot-reload:** `m_SpriteAtlas->Invalidate()` is called in the texture FileWatcher callback, clearing regions/exclusions so the atlas rebuilds next frame with fresh pixel data.
- **Files:** `Engine/include/Enjin/Effects/SpriteTextureAtlas.h`, `Engine/src/Effects/SpriteTextureAtlas.cpp`

### Descriptor Set Caching

Per-entity texture (bindings 3/5/6/8/9) and bone buffer (binding 7) descriptor writes are cached via `m_LastBound` state in `RenderSystem`. `UpdateEntityTextureDescriptors()` and `UpdateBoneDescriptor()` compare resolved pointers against the last-written state and skip `vkUpdateDescriptorSets` when unchanged. The main render loop sorts entities by `MaterialComponent::cachedTextureKey` (a pointer-tuple struct) so identical materials draw consecutively, maximizing cache hits. `m_LastBound.Reset()` is called at each render pass boundary (main pass, splitscreen viewports, RenderToTarget, RenderSplitscreen).

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
// flags layout: bits 0-2 render, 3 skinned, 8-9 alpha mode, 10 height tex,
//   14-15 shadow dither mode, 16-19 texture flags, 20-23 retro flags,
//   24-28 snap resolution (/8), 29-31 shadow dither pattern
```

### Editor

- **`EditorLayer`** - Main editor class with ImGui panels
- **Default UI sizing:** Body font 17px, heading 23px, monospace 16px. Frame padding 8x5, item spacing 10x7, scrollbar 16px, menu bar height 28px, 4px panel gaps.
- **View menu:** Sub-menus for Panels, Settings (Editor Settings, Project Settings), Rendering (Rendering, Post Processing, Retro Effects), Tools. Game View and Scene List are top-level.
- **Panel names:** `EditorSettings` (bit 5), `PostProcessing` (bit 6), `RetroEffects` (bit 7), `Rendering` (bit 10 — skybox + shadows/ambient/cel/RT/display), `SaveDebug` (bit 31 — View > Tools > Save Debug). All templates default to minimal 5-panel layout (Hierarchy, Inspector, Viewport, Console, AssetBrowser).
- **Project Settings panel:** Project mode, Window icon, Physics, Frame rate, Collision Groups, Build Config, Environment (weather/wind/world time/curvature).
- **`ScenePicker`** - Ray casting for entity selection (click-to-select, marquee rect-pick)
- **`PlayMode`** - Play/Pause/Stop game preview controls. Integrates `TieredSaveSystem` for save/load during play. On Stop, computes a `PlayModeDiff` showing entity changes with cherry-pick apply dialog
- **Multi-select:** `m_SelectedEntities` (unordered_set), `m_PrimarySelected` for inspector/gizmo. Methods: `SelectEntity()`, `DeselectEntity()`, `ClearSelection()`, `SelectRange()`, `SelectEntitiesInRect()`
- **Keyboard shortcuts:** `1/2/3` gizmo modes, `4` local/world, `WASD` fly cam, `Space/E` up, `Q/Ctrl` down, `Shift` sprint, RMB+mouse look, `Delete` delete, `Ctrl+D` duplicate, `F` focus, `Ctrl+click` toggle select, `Shift+click` range select, viewport drag for marquee
- **Entity icons:** `GetEntityIcon()` prefixes hierarchy labels with bracket-tags by primary component type (`[C]` Camera, `[L]` Light, `[M]` Mesh, `[S]` Sprite, `[T]` Tilemap, `[P]` Particle, `[A]` Audio, `[R]` Rigidbody, `[D]` Dialogue, `[V]` Visual Script, `[U]` UI Canvas, `[AI]` AI, `[BT]` Behavior Tree)
- **Component icons:** `GetComponentIcon()` adds bracket-tags to inspector `CollapsingHeader` labels (Transform, Mesh, Material, Light, Camera, Sprite 2D, Audio, Rigidbody, Health)
- **Empty states:** `DrawEmptyState()` helper renders centered icon (heading font, 40% opacity), heading, body text, and optional CTA button. Applied to Hierarchy (no world/no entities), Inspector (no selection), Asset Browser (dir not found), Dialogue, Plugin Browser, and all "No world loaded" panels

### Skybox

`SkyboxConfig` with type (`None`/`Cubemap`/`Procedural`/`SolidColor`), colors, sun direction, cubemap paths, rotation. API: `RenderSystem::SetSkybox(config)` / `GetSkyboxConfig()`.

### UI System

- **`UICanvasComponent`** — ECS component (namespace `Enjin::GUI`). Holds element tree, design resolution, scale mode, theme
- **`UIElement`** — Single UI element with `UIAnchor` layout, `UIStyleOverride`, `UIWidgetData`
- **`UIWidgetType`**: Panel, Button, Label, Image, ProgressBar, Slider, Checkbox, Toggle
- **`NineSliceConfig`** — 9-slice sprite config: `texturePath`, `borderLeft/Right/Top/Bottom` (texels)
- **`UISystem`** — Layout + render + input + focus navigation. `SetTextureResolver()` for Vulkan texture loading. `RenderImage()` resolves textures via the resolver (SVG files auto-route through `SVGLoader`). Focus navigation: Tab/Shift+Tab, DPad/Arrow keys with repeat, Enter/Space/Gamepad-A activation, Left/Right slider adjustment. Focus indicator rendered as outset rounded-rect border using theme `inputFocused` color or per-element `focusColor` override
- **`DialogueBoxComponent`** — Auto-builds UICanvas elements for dialogue display: panel, speaker label, text label, portrait image, continue indicator, 6 choice buttons. `BuildDialogueBoxUI()` creates the element tree; `SyncDialogueBoxUI()` updates from `DialogueComponent` state each frame. Inspector with box layout, text style, portrait, choice, and continue settings
- **UI Editor** — Viewport WYSIWYG: click-select, drag-move, resize handles, right-click context menu

### Effects Systems

- **`WeatherSystem`** - Rain, snow, fog, storm with lightning
- **`Water3D`** - 3D water plane with Gerstner waves
- **`RetroEffects`** - CRT, pixelation, dithering post-processing
- **`ParticleSystem`** - CPU simulation (5 shapes, size/speed curves, gravity/drag) + GPU instanced billboard renderer (up to 16384 particles, Billboard and VelocityStretch modes)
- **Particle Editor** - 12 presets (7 standard + 5 liquid), color gradient, curve visualization, playback controls

### Assets & Build

- **`GLTFLoader`** / **`AssimpLoader`** - Loads glTF/GLB natively, FBX/OBJ/DAE/3DS via Assimp v5.4.3
- **`SceneImporter`** - Converts to ECS entities. `Import()` auto-detects format. `ImportOptions` controls scale, materials, animations, colliders. `ImportResult` includes stats.
- **`AssetMetadata`** - `.enjinasset` JSON sidecar files for re-import settings
- **`PrefabManager`** - Create/instantiate/save/load `.enjprefab` files with per-instance overrides
- **`BuildPipeline`** - Full game export: scan → validate → pack `.enjpak` → copy player → manifest
- **Pack format:** `.enjpak` with magic `ENJPAK10`, per-file CRC32, XOR obfuscation (key: `enjin_default_pack_key_2025`)
- **Player app** (`Player/src/main.cpp`) - Standalone executable, loads `game.enjpak`

### Scripting

- **AngelScript** via `TegeBehavior` base class with hot-reload
- ~350 bound functions across math, entity, scene, input, physics, physics 2D, audio, components (incl. full animator: play/stop/pause/resume/isPlaying/getAnimation/speed), coroutines, events, tweening, noise, rendering, post-processing, dialogue, save/load, weather, particles, quests, cinematics, object pool, destructibles, UI canvas (including focus management), localization, prefabs, networking, AI/behavior trees (15 AI controller + 13 BT blackboard + 2 navmesh), accessibility (8 subtitle + 5 announcer + 4 colorblind + 3 general), Newgrounds
- See `docs/SCRIPTING_API.md` for the complete API reference
- **Visual scripting** (Blueprint-style) with 90+ built-in nodes (including 25 Gameplay nodes: save/load/checkpoint/meta, weather, quests, cinematics, particles, destructibles, prefabs, UI focus, localization; 5 Physics 2D nodes; 6 Networking nodes; 4 AI + 4 BT + 1 Navmesh; 2 StateMachine; 3 Accessibility; 4 Tween; 3 Dialogue; 2 Animator), debugger with breakpoints/step-through, execution timeline profiler

### Window Icon

Place `icon.png` (32x32 or 64x64) next to the executable. Loaded via stb_image + `glfwSetWindowIcon()`. Missing file uses OS default silently. Editor also supports a **Window Icon Picker** in Project Settings: `windowIconPath` on `EditorSettings` with JSON persistence, browse/apply/clear UI, and `Window::SetIcon()` virtual method (GLFW `stbi_load` + `glfwSetWindowIcon` implementation). Auto-applies on settings load.

## Shader Workflow

Shaders are in `Engine/shaders/` as GLSL, compiled to SPIR-V, then embedded in `ShaderData.h`:

1. Edit `triangle.vert` or `triangle.frag`
2. Compile: `glslangValidator -V triangle.frag -o triangle.frag.spv`
3. Convert to C++ array and update `ShaderData.h`
4. Rebuild engine

### Shader Hot-Reload (Editor-Only)

RenderSystem watches `Engine/shaders/` via `FileWatcher`, polling every 30 frames. On change: compile to SPIR-V via `glslangValidator`/`glslc`, swap shader module if successful, keep old shader on failure.

Watched shaders: `triangle.vert/frag`, `shadow.vert`, `skybox.vert/frag`, `grass.vert/frag`, `shrub.vert/frag`, `tree.vert/frag`, `particle.vert/frag`, `sprite.vert/frag`, `sprite_lit.vert/frag`.

Controlled by `m_ShaderHotReloadEnabled` (default `true`). Requires compiler in PATH. Silently disabled if `Engine/shaders/` not found.

## Common Tasks

### Adding a new ECS Component

1. Create header in `Engine/include/Enjin/ECS/Components/`
2. Include in relevant systems
3. Optionally add inspector UI in `EditorLayer::DrawInspectorPanel()`

### Adding a new shader uniform

1. Update the UBO struct (`Light.h` for LightingUBO, `VulkanPipeline.h` for ViewProjectionUBO)
2. Update the GLSL shader
3. Recompile shader to SPIR-V
4. Update `ShaderData.h` with new bytecode
5. Update the system that uploads the uniform (e.g., `RenderSystem::UpdateUniformBuffer`)

### Importing a 3D model at runtime

```cpp
SceneImporter::ImportOptions options;
options.scale = 1.0f;
options.importMaterials = true;
options.importAnimations = true;
options.generateColliders = true;
auto result = SceneImporter::Import("path/to/model.gltf", m_World, options);
if (result.success) {
    // result.entities, result.rootEntity, result.meshCount, etc.
}
```

### Building a game for export

1. Configure `BuildConfig` with project path, output directory, and window settings
2. Run `BuildPipeline::Execute(config)` — scans scenes, validates assets, packs into `.enjpak`
3. The pipeline copies `EnjinPlayer` executable alongside the pack
4. Player loads `game.enjpak` from its own directory at startup

## Security Considerations

### Input Validation
- **Scene files (JSON):** Validate array sizes, check `.contains()` before accessing keys. Vectors return safe defaults for malformed arrays.
- **glTF/GLB import:** Clamp loop bounds to allocated buffer size. Validate animation keyframe arrays.
- **Asset pack (.enjpak):** Bounds-check all sizes/offsets against file size, cap to reasonable maximums. Check `file.good()` after every seek/read.

### Script Execution
- AngelScript sandboxed from filesystem/network (no bindings exposed).
- 1M instruction limit via `SetLineCallback()` prevents infinite loop DoS.
- Script `#include` paths resolved via `lexically_normal()` but not yet restricted to script directory.

### Asset Pack Obfuscation
- XOR obfuscation is **not cryptographically secure** — trivially broken via known-plaintext.
- CRC32 is for **integrity detection**, not authentication.
- For commercial releases, replace XOR with authenticated encryption (e.g., AES-GCM).

### General Guidelines
- Validate enum casts from deserialized integers against valid ranges.
- Sanitize file paths — prevent `..` traversal.
- Cap allocation sizes from untrusted input.

## Current Feature Status

The engine has 120+ completed features across these categories. See `docs/USER_MANUAL.md` for component details.

- **Rendering:** Vulkan with Blinn-Phong, PBR materials, normal/parallax mapping, 4-cascade CSM shadows, post-processing (bloom, vignette, FXAA, film grain, color grading, full-screen stipple/dither), retro effects, wireframe, deferred framework, GPU frustum culling, per-scene render settings, ray tracing pipeline (RT shadows, reflections, AO, GI, path tracing, SVGF denoiser — awaiting compiled SPIR-V shaders)
- **ECS & Editor:** 70+ component types, ImGui editor with hierarchy/inspector/viewport, transform gizmos, multi-select, undo/redo, component search with fuzzy matching, 23 startup templates, entity visibility toggle, bug reporting & feedback system (auto-diagnostics, JSON persistence, remote submission), vector drawing editor (7 shapes, layers, SVG export), splash screen with animated mantra
- **2D:** Sprite rendering, sprite texture atlas (auto-packing for batched draw calls), tilemap rendering/editing, sprite animation, 2D camera (follow, bounds, shake, dead zones, look-ahead), 2D/3D project mode separation
- **3D:** glTF/FBX/OBJ/DAE/PLY/VOX import, skeletal animation, LOD, terrain sculpting, vegetation (grass/shrub/tree with custom assets), cubemap skybox
- **Physics:** `IPhysicsBackend` abstraction layer with Jolt Physics v5.2.0 backend (multi-threaded solver, rotational dynamics, CCD, island-based sleep) and Box2D v3.0.0 backend (production-grade 2D: sub-stepping, contact/sensor events, raycasting, overlap queries, 5 joint types, CCD), SimplePhysics fallback, collision detection (sphere/AABB), constraint solver (6 joint types), ragdoll, gravity/temperature zones, collision filtering (32-group bitmask), 2D physics (circle/box/polygon, 5 joint types, CCD, physics materials)
- **Audio:** miniaudio backend, 3D spatialization, multi-channel mixing
- **Scripting:** AngelScript (~350 bindings incl. AI/BT/navmesh + accessibility), visual scripting (90+ nodes, debugger), state machines with script callbacks, coroutines, event system, DataAsset system (schemas + instances, JSON I/O, AS + VS bindings), documentation generator, plugin DLL repositories
- **Gameplay:** Tiered save system (20 slots, 3-tier persistence: SceneState/RunState/MetaProgression, auto-save, checkpoints, pluggable backends — Local/Newgrounds/Steam), play mode diff dialog (cherry-pick entity changes on Stop), in-game save/load menu component, quest/objective system, HUD overlay, cinematic camera, dialogue trees (7 node types, .enjdlg files), tweening (25 easing functions), object pooling, damage/stamina systems, destructible environments (4 fracture patterns, chain destruction), localization system (string tables, CSV/JSON, LOC() macro), Newgrounds.io API (medals, scoreboards, cloud saves)
- **Networking:** LAN multiplayer (host-authoritative UDP, client-side prediction, 20Hz state sync, interpolation buffer, entity ownership, RPC system, lobby, reliable delivery, editor Network Panel)
- **Effects:** Weather, water, particles (12 presets, GPU instanced), world time/seasons, noise library (4 types, 2D+3D, fractal functions)
- **Procedural:** 9 generation algorithms (cellular automata, BSP, diamond-square, L-system, WFC, Voronoi, random walker, grammar, prefab assembler), editor panel with preview
- **Build & Export:** Asset pack pipeline (.enjpak), standalone player app, splitscreen (2P/4P), HTML5 export (canvas, preloader, responsive scaling, Newgrounds-compatible embed)
- **Tools:** Node graph editor framework, animation graph, dialogue editor, visual script editor, particle editor, profiler, plugin/hot-reload system, shader graph (skeleton), audio event graph (skeleton), particle graph (skeleton)
- **Accessibility:** 11 editor themes, 8 colorblind modes, remappable input, subtitles, content warnings, reduced motion, keyboard-only navigation (panel focus shortcuts, gizmo nudge), motor accessibility (dwell-click, sticky drag, adjustable thresholds), scene & entity locking (.enjinlock advisory locks), command palette (Ctrl+P, fuzzy search, 25+ commands), alternative input devices (switch access, eye tracking, sip-and-puff, head tracking), audio visual indicators, screen reader announcer, UICanvas focus navigation (Tab/DPad/Arrow keys with focus indicators)

## Known Performance Issues

Key bottlenecks identified in codebase audits — see `docs/ROADMAP.md` for remaining plans.

- **P0 (all resolved):** ~~`vkDeviceWaitIdle()` GPU stalls~~ (replaced with per-frame fences), ~~`GetAllEntities()` + filter~~ (replaced with `GetEntitiesWithComponent<T>()`), ~~shadow pass iteration~~ (shadow caster caching)
- **P1 (all resolved):** ~~Per-entity texture lookups~~ (cached texture pointers on MaterialComponent), ~~per-entity descriptor writes~~ (last-bound tracking + material sort)
- **P2 (all resolved):** ~~Redundant per-entity `GetComponent()` calls~~ (multi-component query + `GetColliderInfo()` helper + Has+Get merged to single Get+null-check), ~~string-based entity lookups in scripts~~ (name cache on World with lazy rebuild), ~~vector allocations without `reserve()` in FlowerSystem~~ (reserve before spawn loops), ~~`std::map` in DialogueTree/Gameplay~~ (switched to `unordered_map`)
- **P3 (all resolved):** ~~O(N²) collision detection~~ (spatial hash grid broad-phase), ~~`GetAllEntities()` in physics hot paths~~ (per-frame collider cache for ground check/raycast/overlap), ~~gravity zone query per rigidbody~~ (hoisted outside loop), ~~6 redundant ScriptComponent queries per frame~~ (single cached query shared by Update/FixedUpdate/LateUpdate), ~~player entity scan every frame~~ (cached with invalidation), ~~O(N) streaming queue duplicate checks~~ (hash set)
- **P4 (Phases 1-3 done, Phases 4-5 planned):** Jolt + Box2D integration (5-phase plan, replaces SimplePhysics entirely). Phase 1 complete: `IPhysicsBackend`/`IPhysicsBackend2D` interfaces, adapters, factory, all consumers rewired. Phase 2 complete: `JoltBackend` wrapping Jolt v5.2.0 (1273 lines) — full ECS↔Jolt sync, thread-safe contact events via `JoltContactListener`, bilateral collision filtering in `OnContactValidate`, 6 joint types via `BodyLockWrite`, gravity zones, raycasting, CCD. Phase 3 complete: `Box2DBackend` wrapping Box2D v3.0.0 (~755 lines) — C API with handle-based IDs, ECS↔Box2D sync, contact/sensor event polling, raycasting, overlap queries, 5 joint types, CCD, bilateral collision filtering. Phase 4 pending: migration. See `docs/ROADMAP.md`.

## Roadmap

See `docs/ROADMAP.md` for detailed technical plans, implementation priorities, and progress tracking.

**Key categories of planned work:**
- **Editor Tools:** ~~Accent color theming~~ (done), project hub, template rebuild, ~~extended model formats (PLY/VOX)~~ (done), drag-and-drop improvements, ~~micro-interactions~~ (done — spring easing, hover transitions)
- **Runtime Systems:** ~~Improved physics (2D, CCD)~~ (done — PhysicsWorld2D), networking, ~~destructible environments~~ (done — DestructibleSystem), fluid simulation, ~~SVG import~~ (done), ~~dialogue assets + localization~~ (done — .enjdlg files, LocalizationManager)
- **Rendering & Performance:** ~~Sprite batching~~ (done), ~~pipeline optimization~~ (done), ~~soft shadows~~ (done), ~~ray tracing pipeline~~ (done — RT shadows/reflections/AO/GI, path tracing, SVGF denoiser, awaiting compiled SPIR-V), ~~full-screen stipple/dither~~ (done — 8 combinable patterns via bitmask, 3 color modes, post-process chain), Jolt + Box2D physics integration (Phases 1-3 done — IPhysicsBackend abstraction + JoltBackend + Box2DBackend; Phases 4-5 pending — migration, SimplePhysics retirement)
- **Procedural Generation:** ~~All algorithms~~ (done — 9 algorithms + editor panel), ~~custom flora assets~~ (done)
- **Scripting:** ~~Plugin DLL repositories~~ (done), ~~documentation generator~~ (done), ~~ScriptableObject/DataAsset system~~ (done)
- **Graph Systems:** ~~Shader Graph, Audio Event Graph, Particle Graph~~ (done — skeleton node types + editor shells)
- **Platform:** Linux, Steam Deck (Steam Input + gyro), macOS (MoltenVK), Xbox Series X|S (GDK/D3D12), PS5 (PSDK/AGC), Switch 2 (Vulkan 1.3), Switch 1 (NVN), Mobile (Android/iOS), VR/XR (OpenXR), WebAssembly (WebGPU). Platform abstraction layer (`PlatformTarget`) with adaptive quality, input/save/achievement abstractions
- **Accessibility:** ~~Screen reader support~~ (done — Announcer), ~~keyboard-only navigation~~ (done — panel focus, gizmo nudge), ~~motor accessibility~~ (done — dwell-click, sticky drag, alternative input devices)
- **Collaboration:** Git integration, ~~scene/entity locking~~ (done — .enjinlock advisory locks), collaborative editing
- **Flash Game Revival:** ~~Vector drawing editor~~ (done), ~~Newgrounds.io API~~ (done), ~~HTML5 export~~ (done), SWF import, AS2/AS3 transpiler, Flash-style authoring

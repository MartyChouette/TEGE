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
│   │   ├── Audio/          # AudioSystem, SimpleAudio (miniaudio), SteamAudioProcessor (HRTF)
│   │   ├── Debug/          # Profiler, ScopeTimer, FrameData
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # 70+ component types (incl. joints, ragdoll, script, LOD)
│   │   │   │   ├── Controllers/  # 5 character controller types + Vehicle
│   │   │   │   └── ...
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Accessibility/  # ColorblindFilter, SubtitleSystem, ContentWarning
│   │   ├── Editor/         # EditorLayer, PlayMode, PlayModeDiff, EditorSettings, FeedbackSystem, VectorDrawingEditor, PerformanceStats, TemplateMarketplace
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

- **`ECS::World`** - Manages entities and components. Thread-safe: structural ops (Create/Destroy/Add/Remove/Clear) guarded by recursive mutex. `DestroyEntity()` is deferred — queued and flushed at `Update()` start. `DestroyEntityImmediate()` for rare cases needing instant removal. `IsValid()` returns false for pending-destruction entities. `Lock()`/`Unlock()` for external batch operations.
- **`ECS::Entity`** - Just a u64 ID
- **Key Components:**
  - `TransformComponent` - position, rotation (Euler), scale, `visible` bool
  - `MeshComponent` - vertices (position, normal, UV, color, tangent, boneWeights, boneIndices), indices
  - `MaterialComponent` - PBR properties, textures (base color, normal, height), retro flags, dithered gradient (`ditherGradient`, `ditherGradientBands` 2-8, `ditherGradientPattern` 6 patterns)
  - `LightComponent` - Light data (direction, color, intensity)
  - `NameComponent` - Entity name string
  - `CameraComponent` - In-game cameras with projection
  - `NotesComponent` - Text annotations (field: `.notes`, not `.text`)
  - `AnimatorComponent` - Skeletal animation playback
  - `CharacterController` - Movement controllers (Platformer2D, TopDown2D/3D, FPS, TPS)
  - `BoxColliderComponent` / `SphereColliderComponent` / `CapsuleColliderComponent` - Colliders with `categoryBits`/`collisionMask` bitmask filtering
  - `SaveDataComponent` - Persistence marker with `PersistenceTier` (SceneState/RunState/MetaProgression), custom tags, and key-value data
  - `SaveLoadMenuComponent` - In-game save/load grid overlay with configurable columns, mode (Save/Load), and slot display
  - `PostProcessVolumeComponent` - Spatial PP blending with Box/Sphere shapes, priority, smoothstep blend radius, selective override mask (24 effect groups incl. 5 screen-space effects at bits 19-23), global volumes

### Collision Filtering

Bitmask system: `categoryBits` (which groups it belongs to) and `collisionMask` (which groups it collides with). Bilateral rule: `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`. Defaults: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF`. Up to 32 named groups stored in `SceneManager::m_CollisionGroupNames`. Old `layer` field migrated to `categoryBits` in deserialization.

### Physics Backend Abstraction

`IPhysicsBackend` (3D) and `IPhysicsBackend2D` (2D) are abstract interfaces for physics implementations. Shared data types live in `PhysicsTypes.h` (AABB, Ray, RaycastHit, CollisionResult, CollisionEvent, ColliderInfo) and `PhysicsTypes2D.h` (Shape2DType, Body2DComponent, Joint2DComponent, Contact2D, RayHit2D) — always available regardless of which backends are compiled. `JoltBackend` wraps Jolt Physics v5.2.0 with full ECS↔Jolt synchronization, thread-safe contact events, bilateral collision filtering, 6 joint types, gravity zones, and CCD support. `Box2DBackend` wraps Box2D v3.0.0 (C API with handle-based IDs) for production-grade 2D physics — multi-threaded sub-stepping, robust constraint solving, 5 joint types (Revolute/Prismatic/Distance/Rope/Weld), contact+sensor event polling, raycasting, overlap queries, CCD, and bilateral collision filtering. `SimplePhysicsBackend` and `SimplePhysicsBackend2D` wrap the legacy engines (guarded by `ENJIN_PHYSICS_SIMPLE`). `PhysicsBackendFactory` creates backends via `CreatePhysicsBackend(type, mode)` / `CreatePhysicsBackend2D(type, mode)` with `IsJoltAvailable()` / `IsBox2DAvailable()` / `IsSimpleAvailable()` helpers and `ResolveBackendName()`. `PhysicsBackendType` enum: `Auto`, `Jolt`, `Box2D`, `Simple`. When `ENJIN_PHYSICS_JOLT=ON` (default), Auto selects Jolt for 3D/Mixed modes. When `ENJIN_PHYSICS_BOX2D=ON` (default), Auto selects Box2D for 2D/Mixed modes. PlayMode and Player own physics via `unique_ptr<IPhysicsBackend>`. All consumers accept `IPhysicsBackend*`. `ControllerSystem` accepts both `IPhysicsBackend*` (3D) and `IPhysicsBackend2D*` (2D) — Platformer2D uses 2D raycasts for ground detection when available, with 3D fallback. CMake options: `ENJIN_PHYSICS_JOLT` (ON), `ENJIN_PHYSICS_BOX2D` (ON), `ENJIN_PHYSICS_SIMPLE` (ON, can be disabled to retire legacy code).

### Project Mode (2D/3D)

`ProjectMode` enum: `Mode2D`, `Mode3D`, `Mixed`. Stored in `.enjinproject`. Components tagged with `DimensionTag` (Any/Only2D/Only3D) for Add Component filtering. Grid orientation: 2D = XY plane, 3D/Mixed = XZ plane.

### Scene Composition & 2D/3D Pipeline

`ClassifySceneComposition()` runs each frame and classifies the scene as `SceneRenderMode::Scene2D`, `Scene2_5D`, or `Scene3D` based on entity types present. This drives automatic pipeline optimizations:

- **Scene2D** (sprites/tilemaps only): Shadow passes (directional CSM, point, spot) are skipped entirely. `UpdateFrameUniforms()` early-returns after uploading a minimal LightingUBO (ambient/fog only, zero lights/shadows). Normal map descriptor writes (binding 6) are skipped in the unlit sprite texture bind callback.
- **Scene2_5D** (sprites + any lights, no 3D meshes): Shadows skipped, but full lighting UBO is populated so lit sprites respond to lights. Classification triggers on any light entity (point, spot, directional), not just shadow-casting directional. Normal map descriptor is bound.
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

Full Vulkan RT pipeline with hybrid raster+RT rendering, fully wired end-to-end. `CompositeRTResults()` called after SVGF denoising, real depth buffer bound to RT descriptor binding 2, camera change detection resets path tracer accumulation, `DenoiseRTOutputs()` executes real SVGF passes. All 19 RT shaders compiled to SPIR-V and embedded in `RTShaderData.h` (384–10,964 bytes each). RT pipeline activates automatically on supported hardware.

- **`RTCapabilities`** - Extension detection (`Query(VkPhysicalDevice)`) and properties. VulkanContext adds +1000 score for RT-capable GPUs, enables RT extensions via VkPhysicalDeviceFeatures2 pNext chain
- **`AccelerationStructureManager`** - BLAS cache by mesh hash (`RegisterMesh()`), per-frame TLAS rebuild (`BuildTLAS()`), instance management. Uses `vkGetDeviceProcAddr` for all RT functions
- **`RTPipeline`** - RT pipeline + shader binding table (SBT) construction
- **RT Effects** - `RTShadows` (R16F), `RTReflections` (RGBA16F), `RTAmbientOcclusion` (R16F), `RTGlobalIllumination` (RGBA16F), `PathTracer` (progressive accumulation). Each has Initialize/Dispatch/Shutdown + config struct
- **`SVGFDenoiser`** - 3-pass compute: temporal accumulation (alpha=0.05), variance estimation (3x3 box), a-trous wavelet (5 iterations, ping-pong buffers)
- **`OIDNDenoiser`** - Intel Open Image Denoise alternative to SVGF. CMake option `ENJIN_RAYTRACING_OIDN` (OFF by default). Editor UI: denoiser type selector (SVGF / OIDN) in Rendering panel RT section. Files: `OIDNDenoiser.h/cpp`
- **`RTCompositor`** - Fullscreen compute shader composites RT layers into scene HDR
- **Integration** - `RenderSystem::InitializeRayTracing()` creates RT descriptor set (14 bindings), all subsystems. Runs after shadow pass, before main render pass. Only for Scene3D mode. Activates automatically on RT-capable hardware
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
- **View menu:** Sub-menus for Panels, Settings (Editor Settings, Project Settings), Rendering (Rendering, Post Processing, Retro Effects), Tools. Game View and Scene List are top-level. "Show Colliders" toggle for physics debug wireframes.
- **Panel names:** `EditorSettings` (bit 5), `PostProcessing` (bit 6), `RetroEffects` (bit 7), `Rendering` (bit 10 — skybox + shadows/ambient/cel/RT/display), `SaveDebug` (bit 31 — View > Tools > Save Debug). All templates default to minimal 5-panel layout (Hierarchy, Inspector, Viewport, Console, AssetBrowser).
- **`TemplateMarketplace`** - Bundled catalog of 15 curated templates across 5 categories (Starter, Genre, Systems, Retro, Advanced). `MarketplaceEntry` struct with id, name, description, category, author, version, license, projectMode, tags, accentColor, downloadCount, rating, ratingCount, fileSizeBytes, quality. Search (multi-field fuzzy), filter by category, sort by name/rating/downloads. Install/uninstall to `templates/` directory. Uses `IsOpen()/SetOpen()` pattern. Full marketplace UI: search bar, category filter chips, sort dropdown, grid layout with accent-colored cards. Menu: View > Tools > Template Marketplace. Files: `TemplateMarketplace.h/cpp`
- **Notification toast system** - Stacked toast notifications in bottom-right corner. 4 types: Info (blue), Success (green), Warning (yellow), Error (red). Slide-in animation from right, fade-out on expiry (3s default, 5s for errors). `ShowNotification(message, type)` callable from EditorLayer. Wired into scene save/load, build complete/fail, template save/install/remove, model import, component remove (with undo hint)
- **Accent color picker** - 6 harmony presets in Editor Settings (Default Blue, Warm Orange, Forest Green, Royal Purple, Crimson Red, Teal). Click preset to auto-derive all 11 accent colors from single primary. Fine-tune individual colors in collapsible sub-tree. Reset to Theme Defaults button
- **Theme preview** - Miniature 250x160 live preview pane in Editor Settings showing current theme's window bg, title bar, header, buttons, slider, checkbox, status bar. Updates in real-time when theme/accent changes
- **Keyboard shortcuts help modal** - Ctrl+Shift+/ opens searchable modal listing all editor shortcuts grouped by category (General, Viewport, Selection, Play Mode, Editor). Fuzzy search across shortcut keys, descriptions, and categories
- **Hierarchy search** - Case-insensitive text filter in hierarchy panel; non-matching entities are hidden
- **Delete confirmation** - Modal dialog ("Delete N entities?") before entity deletion with Cancel/Delete buttons
- **Inspector tooltips** - 50+ tooltips across Transform, Material, Light, Camera, Rigidbody, and Collider component fields
- **Project Settings panel:** Project mode, Window icon, Physics, Frame rate, Audio (HRTF toggle + status), Collision Groups, Build Config, Environment (weather/wind/world time/curvature).
- **`ScenePicker`** - Ray casting for entity selection (click-to-select, marquee rect-pick)
- **Physics debug wireframes** — `m_ShowColliderWireframes` toggle draws Box (yellow), Sphere (green-yellow, 3 circles), Capsule (orange) collider outlines + joint lines (6 colors by type)
- **`PlayMode`** - Play/Pause/Stop game preview controls. Integrates `TieredSaveSystem` for save/load during play. On Stop, scene changes persist (objects are not restored to pre-play state); `PlayModeDiff` shows what changed for informational purposes. Camera position is restored on stop
- **Multi-select:** `m_SelectedEntities` (unordered_set), `m_PrimarySelected` for inspector/gizmo. Methods: `SelectEntity()`, `DeselectEntity()`, `ClearSelection()`, `SelectRange()`, `SelectEntitiesInRect()`
- **Keyboard shortcuts:** `1/2/3` gizmo modes, `4` local/world, `WASD` fly cam, `Space/E` up, `Q/Ctrl` down, `Shift` sprint, RMB+mouse look, `Delete` delete, `Ctrl+D` duplicate, `F` focus, `Ctrl+click` toggle select, `Shift+click` range select, viewport drag for marquee
- **Entity icons:** `GetEntityIcon()` prefixes hierarchy labels with bracket-tags by primary component type (`[C]` Camera, `[L]` Light, `[M]` Mesh, `[S]` Sprite, `[T]` Tilemap, `[P]` Particle, `[A]` Audio, `[R]` Rigidbody, `[D]` Dialogue, `[V]` Visual Script, `[U]` UI Canvas, `[AI]` AI, `[BT]` Behavior Tree)
- **Component icons:** `GetComponentIcon()` adds bracket-tags to inspector `CollapsingHeader` labels (Transform, Mesh, Material, Light, Camera, Sprite 2D, Audio, Rigidbody, Health)
- **Empty states:** `DrawEmptyState()` helper renders centered icon (heading font, 40% opacity), heading, body text, and optional CTA button. Applied to Hierarchy (no world/no entities), Inspector (no selection), Asset Browser (dir not found), Console, Network, Dialogue, Plugin Browser, and all "No world loaded" panels

### Skybox

`SkyboxConfig` with type (`None`/`Cubemap`/`Procedural`/`SolidColor`), colors, sun direction, cubemap paths, rotation. API: `RenderSystem::SetSkybox(config)` / `GetSkyboxConfig()`.

### UI System

- **`UICanvasComponent`** — ECS component (namespace `Enjin::GUI`). Holds element tree, design resolution, scale mode, theme
- **`UIElement`** — Single UI element with `UIAnchor` layout, `UIStyleOverride`, `UIWidgetData`, `accessibleLabel` (for screen reader, falls back to name)
- **`UIWidgetType`**: Panel, Button, Label, Image, ProgressBar, Slider, Checkbox, Toggle
- **`NineSliceConfig`** — 9-slice sprite config: `texturePath`, `borderLeft/Right/Top/Bottom` (texels)
- **`UISystem`** — Layout + render + input + focus navigation. `SetTextureResolver()` for Vulkan texture loading. `RenderImage()` resolves textures via the resolver (SVG files auto-route through `SVGLoader`). Focus navigation: Tab/Shift+Tab, DPad/Arrow keys with repeat, Enter/Space/Gamepad-A activation, Left/Right slider adjustment. Focus indicator rendered as outset rounded-rect border using theme `inputFocused` color or per-element `focusColor` override. `SetFontScale(f32)` multiplies all resolved font sizes for accessibility
- **`DialogueBoxComponent`** — Auto-builds UICanvas elements for dialogue display: panel, speaker label, text label, portrait image, continue indicator, 6 choice buttons. `BuildDialogueBoxUI()` creates the element tree; `SyncDialogueBoxUI()` updates from `DialogueComponent` state each frame. Inspector with box layout, text style, portrait, choice, and continue settings
- **UI Editor** — Viewport WYSIWYG: click-select, drag-move, resize handles, right-click context menu

### Effects Systems

- **`WeatherSystem`** - Rain, snow, fog, storm with lightning
- **`Water3D`** - 3D water plane with Gerstner waves
- **`RetroEffects`** - CRT, pixelation, dithering post-processing
- **`ParticleSystem`** - CPU simulation (5 shapes, size/speed curves, gravity/drag) + GPU instanced billboard renderer (up to 16384 particles, Billboard and VelocityStretch modes)
- **Particle Editor** - 12 presets (7 standard + 5 liquid), color gradient, curve visualization, playback controls
- **`FluidTerrainCoupling`** - Wires FluidSimulation density/velocity grids to TerrainComponent heightmap. Erosion mode (velocity erodes terrain) and accumulate mode (density builds terrain, e.g. lava). Bidirectional: terrain slope drives fluid flow. Files: `FluidTerrainCoupling.h/cpp`
- **`ReactionDiffusion`** - Gray-Scott model Turing pattern simulation on 2D grids. 9 presets, bake-to-RGBA8/heightmap. Files: `ReactionDiffusion.h/cpp`
- **`CellularAutomataGeometry`** - 7 CA rules with real-time mesh gen (Voxels/MarchingCubes/PointCloud). Files: `CellularAutomataGeometry.h/cpp`
- **`PhysarumSimulation`** - Agent-based slime mold sim (50K+ agents, trail diffusion/decay, 5 presets). Files: `PhysarumSimulation.h/cpp`
- **`TimelineEditor`** - Flash-style keyframe animation with layers, curves, auto-key, onion skinning. Files: `TimelineEditor.h/cpp`
- **`FourierMesh`** - DFT decomposition of 2D contours, progressive reconstruction animation, 3D extrusion. Files: `FourierMesh.h/cpp`
- **`Projection4D`** - 4D polytope visualization: 5 polytopes, 6 rotation planes, stereographic projection 4D→3D. Files: `Projection4D.h/cpp`

### Assets & Build

- **`GLTFLoader`** / **`AssimpLoader`** - Loads glTF/GLB natively, FBX/OBJ/DAE/3DS via Assimp v5.4.3
- **`SceneImporter`** - Converts to ECS entities. `Import()` auto-detects format. `ImportOptions` controls scale, materials, animations, colliders. `ImportResult` includes stats.
- **`AssetMetadata`** - `.enjinasset` JSON sidecar files for re-import settings
- **`PrefabManager`** - Create/instantiate/save/load `.enjprefab` files with per-instance overrides
- **`BuildPipeline`** - Full game export: scan → validate → pack `.enjpak` → copy player → manifest
- **Pack format:** `.enjpak` with magic `ENJPAK10`, per-file CRC32, XOR obfuscation (key: `enjin_default_pack_key_2025`)
- **`SWFConverter`** - Converts parsed SWFDocument to ECS entities (shapes→sprites, MovieClips→entity hierarchy with timeline). Files: `SWFConverter.h/cpp`
- **Player app** (`Player/src/main.cpp`) - Standalone executable, loads `game.enjpak`. Automatic title screen (MainMenu after splash) and pause menu (ESC during gameplay) via `GameMenuSystem` with fully wired callbacks (New Game, Continue, Options, How to Play, Resume, Quit to Menu, Quit). Runtime systems: ParticleSystem, SubtitleSystem, AlternativeInputManager, AccessibilityAnnouncer, AudioVisualIndicatorSystem, ContentWarningSystem, PostProcessing, FluidSimulation, WindSystem, WorldTime, SeasonalWeather, reduced motion wiring, font scaling. Applies per-scene render settings after loading.

### Asset Libraries

- **`FontLibrary`** - Curated catalog of 42 OFL/Apache-licensed fonts across 8 categories (Sans-Serif, Serif, Monospace, Display, Handwriting, Pixel, Fantasy, Sci-Fi). Editor browser in Editor Settings > Fonts with search, category filter, and install status. Files: `FontLibrary.h/cpp`
- **`AssetLibrary`** - Curated CC0 asset catalog: 16 3D model packs (Kenney/Quaternius) and 15 2D sprite/tileset/UI packs across 14 categories (Architecture, Nature, Props, Characters, Vehicles, Weapons, Dungeon, Sci-Fi, UI Kits, Tilesets, Sprites, VFX, Backgrounds, Textures). Editor browser with search and category filter. Files: `AssetLibrary.h/cpp`

### Steam Audio HRTF

`SteamAudioProcessor` provides physics-based HRTF binaural rendering for 3D sounds via headphones. CMake option `ENJIN_AUDIO_STEAM_AUDIO` (OFF by default, requires SDK at `third_party/steamaudio/`). Steam Audio is a **processing layer** — miniaudio still handles file I/O, mixing, and device output. For each 3D sound, a custom `ma_node` (BinauralNode) is inserted between the `ma_sound` source and the engine endpoint. The node routes mono audio through `IPLBinauralEffect`, producing binaural stereo. 2D sounds (Music, UI) are unaffected. When HRTF is active, miniaudio's built-in spatialization is disabled and distance attenuation is applied manually via `Calculate3DVolume()`. Coordinate conversion: Enjin LH (Z forward) → Steam Audio RH (Z backward) by negating Z in `ComputeListenerRelativeDirection()`. Thread safety: CreateSource/DestroySource are mutex-guarded; SetSourceDirection is lock-free (benign race); Process runs on the audio thread. Editor UI: "Audio" section in Project Settings with HRTF toggle + status indicator. `EditorSettings::enableHRTF` persisted in JSON. Files: `SteamAudioProcessor.h/cpp`

### Audio Mixer

Editor tool window (View > Tools > Audio Mixer) showing all AudioSourceComponent entities in a DAW-style mixer layout. Master volume slider at top, per-channel volume strips (SFX/Music/UI/Voice) with color-coded labels and mute buttons. Tab bar filters by channel (All/SFX/Music/UI/Voice). Source list: channel badge, entity name, clip filename, volume/pitch sliders (live during play mode), loop/3D flags, playing state. Click source to select entity. `m_ShowAudioMixer` toggle.

### Template Creator

`TemplateCreator` saves the current scene as a reusable startup template. `SaveTemplate()` / `LoadTemplate()` / `ScanTemplates()` / `DeleteTemplate()`. Editor UI at View > Tools > Template Creator with metadata editing (name, description, category), save/load/delete. Auto-captures game view as 280x180 PNG thumbnail on save (Vulkan framebuffer readback via `RenderTarget::CaptureToPixels()`, BGRA-to-RGBA swizzle, downscale, `stbi_write_png`). Custom templates stored in `templates/` directory. Files: `TemplateCreator.h/cpp`, `RenderTarget.h/cpp`

### Scripting

- **AngelScript** via `TegeBehavior` base class with hot-reload
- ~481 bound functions across math, entity, scene, input, physics, physics 2D, audio (incl. channel volume: SetChannelVolume/GetChannelVolume/StopChannel + 4 channel constants), components (incl. full animator: play/stop/pause/resume/isPlaying/getAnimation/speed), sprite 2D (texture/color/flip/sort/anim), coroutines, events, tweening, noise, rendering, post-processing, post-process volumes (10 bindings: SetActive/IsActive/SetWeight/GetWeight/SetBlendRadius/GetBlendRadius/SetPriority/GetPriority/SetGlobal/IsGlobal), screen-space effects (30 bindings: 6 per effect for GodRays/SSAO/ContactShadows/Caustics/FogShafts — enable/disable + parameter get/set), input actions (22 bindings: GameAction enum + query/sensitivity/toggle/rebinding/display/presets), dialogue, save/load, weather, particles, quests, cinematics, object pool, destructibles, UI canvas (including focus management), localization, prefabs, networking, AI/behavior trees (15 AI controller + 13 BT blackboard + 2 navmesh), accessibility (8 subtitle + 5 announcer + 4 colorblind + 3 general), procedural generation (~15 bindings for all 9 algorithms), camera presets, Newgrounds, audio event graph (4 bindings: TriggerEvent/SetParameter/GetParameter/StopAll), plugins (4 bindings: IsLoaded/GetVersion/Load/Unload), MIDI input (12 bindings: GetDeviceCount/GetDeviceName/OpenDevice/CloseDevice/IsDeviceOpen/IsNoteOn/IsNoteOff/GetNoteVelocity/GetCC/GetCCValue/GetEventCount), Flash API shim (~40 bindings: DisplayObject, MovieClip, Stage, Mouse, Keyboard, TextField, Math, Sound, Timer)
- See `docs/SCRIPTING_API.md` for the complete API reference
- **Visual scripting** (Blueprint-style) with 143+ built-in nodes (including 25 Gameplay nodes: save/load/checkpoint/meta, weather, quests, cinematics, particles, destructibles, prefabs, UI focus, localization; 4 PP Volume nodes; 5 Screen-Space Effect nodes (GodRays/SSAO/ContactShadows/Caustics/FogShafts toggles); 5 Physics 2D nodes; 6 Networking nodes; 4 AI + 4 BT + 1 Navmesh; 2 StateMachine; 3 Accessibility; 4 Tween; 3 Dialogue; 2 Animator; 4 Input; 2 Scene; 6 Noise; 6 Streaming; 9 Procedural; 3 AudioGraph; 3 Plugin; 6 Water; 2 HUD), debugger with breakpoints/step-through, execution timeline profiler

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

## Trust Zone Map

The codebase is divided into trust zones documented in `.enjin-boundaries.json`. AI models navigating the codebase should use these zones to gauge risk and enforce appropriate validation. Summary:

| Zone | Risk | What lives here | Key rule |
|------|------|----------------|----------|
| **security-critical** | HIGH | Networking, script engine, asset packer/reader, scene serializer, plugin loader, template I/O | Every parameter from outside the engine must be validated. Changes require security review. |
| **trust-boundary** | HIGH | ScriptBindings\_\*, SceneSerializer, AssetReader, NetworkSerializer, EditorSettings, VisualScriptExecutor | Where untrusted data crosses into trusted state. Validation MUST happen here. |
| **user-api** | MEDIUM | ECS component headers, ScriptBindings (686+ functions), VS NodeRegistry, InputAction | The scripting contract. Additions safe, removals/renames break user scripts. All script parameters are untrusted. |
| **editor-internal** | LOW-MED | EditorLayer, all \*Editor panels, CommandPalette, PlayMode, tools | Developer-facing UI. Still processes file paths and JSON — validate those. |
| **renderer-internals** | LOW | Vulkan/\*, RayTracing/\*, PostProcessing, shadow maps, shaders, GPU culling | No direct user input. Risk is GPU resource leaks and driver crashes. Always check VkResult. |
| **gameplay-runtime** | LOW-MED | Physics, audio, AI, save/load, dialogue, weather, particles, destructibles | Data from ECS components — moderate trust. Cap iterations, guard divide-by-zero, limit recursion. |
| **foundation** | LOW | Core math, memory allocators, logging, platform abstraction | Shared by everything. Widest blast radius. No untrusted input arrives directly. |

When modifying files, check which zone they belong to and follow the zone's rules from `.enjin-boundaries.json`.

## Current Feature Status

The engine has 150+ completed features across these categories. See `docs/USER_MANUAL.md` for component details.

- **Rendering:** Vulkan with Blinn-Phong, PBR materials, normal/parallax mapping (4 POM modes), 4-cascade CSM shadows, post-processing (bloom, vignette, FXAA, film grain, color grading, full-screen stipple/dither, depth of field, tilt-shift, post-process volumes with spatial blending), screen-space effects (SSAO, contact shadows, god rays, fake caustics, fog shafts — all in postprocess.frag using depth + invViewProj), retro effects, wireframe, deferred framework, GPU frustum culling, per-scene render settings, ray tracing pipeline (RT shadows, reflections, AO, GI, path tracing, SVGF denoiser + optional OIDN via `ENJIN_RAYTRACING_OIDN` — all 19 SPIR-V shaders compiled and embedded), SH light probes (L2, grid generation, baking, wired to renderer via LightingUBO, editor baking tool + viewport visualization), SDF scene (6 primitives, 6 boolean ops, CPU evaluation, GPU buffer packing), OIT (weighted blended, composite pipeline with fullscreen shader), camera presets (9 built-in), dithered gradient rendering (per-material flat-shaded banding, 2-8 bands, 6 dither patterns)
- **ECS & Editor:** 70+ component types, ImGui editor with hierarchy/inspector/viewport, transform gizmos, multi-select, undo/redo, component search with fuzzy matching, hierarchy search (case-insensitive filter), entity delete confirmation modal, 50+ inspector tooltips, 44 startup templates + template creator (save scene as reusable template with auto-captured 280x180 thumbnail) + template marketplace (15 curated templates, search/filter/sort, install/uninstall), entity visibility toggle, bug reporting & feedback system (auto-diagnostics, JSON persistence, remote submission), vector drawing editor (7 shapes, layers, SVG export), splash screen with animated mantra, source-app import presets (10 DCC tools with auto-detection), collaborative editing UI (OT protocol, peer cursors, conflict resolution, lock enforcement), symbol library (reusable graphic symbols as prefabs, nested editing, category browser, update propagation, Flash timeline sync), notification toast system (4 types, slide-in animation, wired to save/build/template/scene-load/model-import/component-remove events), accent color picker (6 harmony presets, auto-derived 11 accent colors), theme preview pane (250x160 live preview), keyboard shortcuts help modal (Ctrl+Shift+/, searchable, categorized)
- **2D:** Sprite rendering, sprite texture atlas (auto-packing for batched draw calls), tilemap rendering/editing, sprite animation, 2D camera (follow, bounds, shake, dead zones, look-ahead), 2D/3D project mode separation
- **3D:** glTF/FBX/OBJ/DAE/PLY/VOX import, skeletal animation, LOD, terrain sculpting, vegetation (grass/shrub/tree with custom assets), cubemap skybox
- **Physics:** `IPhysicsBackend` abstraction layer with Jolt Physics v5.2.0 backend (multi-threaded solver, rotational dynamics, CCD, island-based sleep) and Box2D v3.0.0 backend (production-grade 2D: sub-stepping, contact/sensor events, raycasting, overlap queries, 5 joint types, CCD), SimplePhysics fallback, collision detection (sphere/AABB), constraint solver (6 joint types), ragdoll, gravity/temperature zones, collision filtering (32-group bitmask), 2D physics (circle/box/polygon, 5 joint types, CCD, physics materials)
- **Audio:** miniaudio backend (WAV/MP3/FLAC/Vorbis), 3D spatialization, Steam Audio HRTF binaural rendering (optional, `ENJIN_AUDIO_STEAM_AUDIO` CMake option, custom ma_node pipeline), audio channels (SFX/Music/UI/Voice with independent volume — Music and UI force non-diegetic 2D playback), multi-channel mixing, MIDI input (platform-specific: WinMM on Windows, device enumeration, per-frame event polling, persistent CC state, 12 AS bindings)
- **Scripting:** AngelScript (~686 bindings incl. 22 gameplay component types, AI/BT/navmesh, accessibility, procedural gen, audio graph, audio channels 3+4 constants, plugins, MIDI input 12 bindings, Flash API shim ~40 bindings, Flash SharedObject persistence 5 bindings, screen-space effects 30 bindings, input actions 22 bindings), AS2/AS3→AngelScript transpiler, visual scripting (136+ nodes incl. Set Channel Volume + Stop Audio Channel + 5 screen-space effect toggles + TweenFloat + OnCollision + CustomEvent, debugger), state machines with script callbacks, coroutines, event system, DataAsset system (schemas + instances, JSON I/O, AS + VS bindings), documentation generator, plugin DLL repositories
- **Gameplay:** Tiered save system (20 slots, 3-tier persistence: SceneState/RunState/MetaProgression, auto-save, checkpoints, pluggable backends — Local/Newgrounds/Steam), play mode diff dialog (cherry-pick entity changes on Stop), in-game save/load menu component, quest/objective system, HUD overlay, cinematic camera, dialogue trees (7 node types, .enjdlg files), tweening (25 easing functions), object pooling, damage/stamina systems, destructible environments (4 fracture patterns, chain destruction), localization system (string tables, CSV/JSON, LOC() macro), Newgrounds.io API (medals, scoreboards, cloud saves), AISystem (patrol/chase/flee/wander/navmesh — fully wired in PlayMode and Player)
- **Networking:** LAN multiplayer (host-authoritative UDP, client-side prediction, 20Hz state sync, interpolation buffer, entity ownership, RPC system, lobby, reliable delivery, HMAC-SHA256 packet authentication with replay protection, per-packet RTT measurement, packet loss tracking, player ID recycling, delta-compressed snapshots, editor Network Panel), cross-platform HTTP client (WinHTTP + libcurl)
- **Effects:** Weather, water, particles (12 presets, GPU instanced), world time/seasons, noise library (4 types, 2D+3D, fractal functions), fluid-terrain coupling (erosion + accumulation modes), reaction-diffusion (Gray-Scott, 9 presets), cellular automata geometry (7 rules, 3 mesh modes), Physarum slime mold (50K+ agents, 5 presets), Fourier mesh decomposition (DFT/progressive reconstruction/3D extrusion), 4D stereographic projection (5 polytopes, 6 rotation planes), inverse rendering (gradient descent parameter optimization), non-Euclidean geometry (portal rendering with stencil recursion, hyperbolic/spherical/toroidal space warping), metaball/blob rendering (marching cubes isosurface, gradient normals, per-group color blending), voxel cone tracing VXGI (diffuse/specular GI, AO, god rays via mip pyramid), SDF rendering (mesh-to-SDF, sphere tracing, 8SSEDT text, volume blending), framebuffer feedback (8 presets, ping-pong compositing, 5 blend modes), screen-space distortion (7 types incl. shockwave/heat haze/underwater), IK-driven mesh deformation (FABRIK + Verlet physics, tube/ribbon meshes), interactive water (spring-damper waves, splashes/wakes/buoyancy, wired in PlayMode/Player/Editor/Serialization), audio-reactive mesh displacement (Cooley-Tukey FFT, bass/mid/treble bands, 4 mapping modes)
- **Procedural:** 9+ generation algorithms (cellular automata, BSP, diamond-square, L-system, WFC, Voronoi, random walker, grammar, prefab assembler, fractal terrain with fBm/ridged multifractal + hydraulic/thermal erosion, advanced 3D L-system with stochastic rules), editor panel with preview, one-click forest generation (4 presets: Mixed/Dense Conifer/Deciduous Park/Sparse Savanna)
- **Build & Export:** Asset pack pipeline (.enjpak), standalone player app (automatic title screen + pause menu with Options/How to Play/Quit), splitscreen (2P/4P), HTML5 export (canvas, preloader, responsive scaling, Newgrounds-compatible embed, Newgrounds game page template with medal sidebar/scoreboard/embed codes), binary distribution (CMake install + CPack, Windows ZIP + NSIS installer with Start Menu shortcuts and file associations, package scripts), Linux AppImage builder, SWF import/conversion (shapes→sprites, MovieClips→entity hierarchy)
- **Platform:** Linux (XDG paths, zenity dialogs, AppImage), Steam Deck (adaptive quality, gyro stubs, Steam Input), Nintendo Switch 1 stub (NVN backend, Joy-Con), Hub application (standalone launcher with project/version/template management)
- **Tools:** Node graph editor framework, animation graph (dual-mode: AnimatorComponent clip/speed/blend editing + StateMachineComponent game logic SM), dialogue editor, visual script editor, particle editor, profiler (P50/P95/P99 frame time percentiles, descriptor cache hit/miss tracking, CSV export), plugin/hot-reload system (PluginContext, PluginSDK.h, state save/restore), shader graph (full GLSL code generation, 58 node types incl. SceneColor/SceneNormal/SceneDepth/StaticSwitch, type mismatch validation, .enjshader), audio event graph (full runtime execution, trigger events, parameter thresholds, .enjaudiopkg), audio mixer (DAW-style mixer window — master/channel volume strips, per-source volume/pitch, channel filter tabs, live play mode control), particle graph (full compiler to ParticleEmitterComponent, .enjparticle), Flash-style timeline editor (layers, Bezier/CatmullRom curves, auto-key, onion skinning), template marketplace (15 curated templates, search/filter/sort, install/uninstall)
- **Accessibility:** 11 editor themes, 8 colorblind modes + colorblind-safe UI theme + 9 colorblind-safe palettes (patterns+icons), remappable input, subtitles, content warnings (wired in Player), reduced motion (UI + weather + editor UI animations), keyboard-only navigation (panel focus shortcuts, gizmo nudge), motor accessibility (dwell-click, sticky drag on UICanvas, adjustable thresholds, switch access/one-button scanning mode), scene & entity locking (.enjinlock advisory locks), command palette (Ctrl+P, fuzzy search, 25+ commands), alternative input devices (switch access, eye tracking, sip-and-puff, head tracking — wired in PlayMode + Player), audio visual indicators (wired in PlayMode + Player), screen reader announcer (wired into UISystem for game UI), UICanvas focus navigation (Tab/DPad/Arrow keys with focus indicators), accessible labels on UIElement, high contrast UI themes (HighContrastDark/Light, WCAG AAA 7:1+), runtime font scaling, dyslexia-friendly font/spacing (FontLibrary with OpenDyslexic option), gamepad editor navigation (4 radial dial menus), keyboard shortcuts help modal (Ctrl+Shift+/, searchable, categorized)

## Known Performance Issues

Key bottlenecks identified in codebase audits — see `docs/ROADMAP.md` for remaining plans.

- **P0 (all resolved):** ~~`vkDeviceWaitIdle()` GPU stalls~~ (replaced with `VulkanContext::WaitForGPU()` — fence-based wait registered by VulkanRenderer, 18 subsystems migrated), ~~`GetAllEntities()` + filter~~ (replaced with `GetEntitiesWithComponent<T>()`), ~~shadow pass iteration~~ (shadow caster caching), ~~play mode double-draw~~ (main pass skip + offscreen-only rendering), ~~frame jitter on Windows~~ (`timeBeginPeriod(1)` for 1ms sleep resolution + 2ms spin margin)
- **P1 (all resolved):** ~~Per-entity texture lookups~~ (cached texture pointers on MaterialComponent), ~~per-entity descriptor writes~~ (last-bound tracking + material sort), ~~offscreen path unsorted entity list~~ (sorted render list built before main pass skip)
- **P2 (all resolved):** ~~Redundant per-entity `GetComponent()` calls~~ (multi-component query + `GetColliderInfo()` helper + Has+Get merged to single Get+null-check), ~~string-based entity lookups in scripts~~ (name cache on World with lazy rebuild), ~~vector allocations without `reserve()` in FlowerSystem~~ (reserve before spawn loops), ~~`std::map` in DialogueTree/Gameplay~~ (switched to `unordered_map`)
- **P3 (all resolved):** ~~O(N²) collision detection~~ (spatial hash grid broad-phase), ~~`GetAllEntities()` in physics hot paths~~ (per-frame collider cache for ground check/raycast/overlap), ~~gravity zone query per rigidbody~~ (hoisted outside loop), ~~6 redundant ScriptComponent queries per frame~~ (single cached query shared by Update/FixedUpdate/LateUpdate), ~~player entity scan every frame~~ (cached with invalidation), ~~O(N) streaming queue duplicate checks~~ (hash set)
- **P4 (all 5 phases complete):** Jolt + Box2D integration complete. Phase 1: `IPhysicsBackend`/`IPhysicsBackend2D` interfaces, adapters, factory, all consumers rewired. Phase 2: `JoltBackend` wrapping Jolt v5.2.0. Phase 3: `Box2DBackend` wrapping Box2D v3.0.0. Phase 4: Jolt/Box2D ON by default, `PhysicsTypes.h`/`PhysicsTypes2D.h` extracted, `Simple = 3` in enum, factory with warnings/helpers, editor UI with 4 backends + availability. Phase 5: `ENJIN_PHYSICS_SIMPLE` compile guard on all legacy files, null-safety verified, builds clean with SIMPLE=ON and SIMPLE=OFF.
- **P6 (resolved):** ~~`World::IsValid()` O(N) pending destruction scan~~ (companion `unordered_set` for O(1) lookup, also optimizes `DestroyEntity()` duplicate check and `IsPendingDestruction()`)
- **P5 (all 19 findings resolved):** ~~`ToEuler()` per sprite for Z rotation~~ (`Quaternion::GetRotationZ()` single atan2, 12+ hot paths), ~~`ToMatrix()` for forward/right/up extraction~~ (`Quaternion::GetForward()/GetRight()/GetUp()` helpers, 7 sites), ~~double atlas region lookup per sprite~~ (cached region pointer + pre-hashed texture paths), ~~6+ optional GetComponent per entity~~ (pre-classified entity component flags), ~~per-frame vector allocations~~ (promoted to member variables), ~~editor GetAllEntities() calls~~ (component queries), ~~AISystem wired~~ (was 615+ lines of dead code, now active in PlayMode and Player)

## Roadmap

See `docs/ROADMAP.md` for detailed technical plans, implementation priorities, and progress tracking.

**Key categories of planned work:**
- **Editor Tools:** ~~Accent color theming~~ (done), project hub, ~~template rebuild~~ (done — 44 templates, all polished with ~50% more content), ~~extended model formats (PLY/VOX)~~ (done), drag-and-drop improvements, ~~micro-interactions~~ (done — spring easing, hover transitions), ~~template creator~~ (done — save scene as template, View > Tools), ~~source-app import presets~~ (done — 10 DCC tools with auto-detection), ~~font library~~ (done — 42 fonts, 8 categories), ~~2D/3D asset library~~ (done — 16 3D + 15 2D CC0 packs), ~~gamepad editor~~ (done — 4 radial dials, DPad hierarchy, stick camera), ~~template marketplace~~ (done — 15 curated templates, search/filter/sort, install/uninstall, accent-colored cards), ~~notification toasts~~ (done — 4 types, slide-in/fade-out, wired to save/build/template/scene-load/model-import/component-remove), ~~accent color presets~~ (done — 6 harmony presets, auto-derive 11 colors), ~~theme preview~~ (done — 250x160 live preview pane), ~~shortcuts help modal~~ (done — Ctrl+Shift+/, searchable, categorized), ~~hierarchy search~~ (done — case-insensitive filter), ~~delete confirmation~~ (done — modal dialog), ~~inspector tooltips~~ (done — 50+ across 6 component types)
- **Runtime Systems:** ~~Improved physics (2D, CCD)~~ (done — PhysicsWorld2D), ~~networking~~ (done — LAN multiplayer with HMAC-SHA256 auth, replay protection), ~~destructible environments~~ (done — DestructibleSystem), ~~fluid simulation~~ (done — Stable Fluids solver, FluidTerrainCoupling), ~~SVG import~~ (done), ~~dialogue assets + localization~~ (done — .enjdlg files, LocalizationManager)
- **Rendering & Performance:** ~~Sprite batching~~ (done), ~~pipeline optimization~~ (done), ~~soft shadows~~ (done), ~~ray tracing pipeline~~ (done — RT shadows/reflections/AO/GI, path tracing, SVGF + OIDN denoisers, all 19 SPIR-V shaders compiled), ~~full-screen stipple/dither~~ (done — 8 combinable patterns via bitmask, 3 color modes, post-process chain), ~~camera presets~~ (done — 9 presets), ~~depth of field + tilt-shift~~ (done — GPU Poisson disc DoF + tilt-shift blur in postprocess.frag, depth sampled from render pass), ~~SH light probes~~ (done — L2, grid, baking, wired to renderer via LightingUBO shProbeIrradiance, editor baking tool with per-probe list + viewport visualization), ~~SDF scene~~ (done — 6 primitives, boolean ops), ~~OIT~~ (done — weighted blended, composite pipeline with embedded SPIR-V), ~~POM advanced modes~~ (done — Steep/Occlusion/Relief), ~~Jolt + Box2D physics integration~~ (done — all 5 phases: abstraction, Jolt backend, Box2D backend, production defaults, SimplePhysics compile-guarded), ~~dithered gradient rendering~~ (done — per-material flat-shaded banding, 2-8 bands, 6 patterns), ~~binary distribution~~ (done — CMake install + CPack, ZIP + NSIS installer, package scripts), ~~screen-space effects~~ (done — SSAO, contact shadows, god rays, fake caustics, fog shafts in postprocess.frag, 30 AS bindings + 5 VS nodes)
- **Procedural Generation:** ~~All algorithms~~ (done — 9+ algorithms + editor panel, fractal terrain with erosion, advanced 3D L-system), ~~custom flora assets~~ (done), ~~one-click forest generation~~ (done — 4 presets, area/density controls)
- **Scripting:** ~~Plugin DLL repositories~~ (done), ~~documentation generator~~ (done), ~~ScriptableObject/DataAsset system~~ (done), ~~MIDI input~~ (done — WinMM backend, 12 AS bindings, wired in PlayMode + Player)
- **Graph Systems:** ~~Shader Graph~~ (done — 58 node types incl. SceneColor/SceneNormal/SceneDepth/StaticSwitch, type mismatch validation, full GLSL codegen with topological sort, .enjshader save/load), ~~Audio Event Graph~~ (done — runtime execution via SimpleAudio, trigger/parameter/delay, .enjaudiopkg, 4 AS + 3 VS bindings), ~~Particle Graph~~ (done — compiler to ParticleEmitterComponent, .enjparticle save/load)
- **Platform:** ~~Linux~~ (done — XDG paths, AppImage, CMake targets), ~~Steam Deck~~ (done — adaptive quality, Steam Input, gyro), macOS (MoltenVK), Xbox Series X|S (GDK/D3D12), PS5 (PSDK/AGC), Switch 2 (Vulkan 1.3), ~~Switch 1~~ (stub done — NVN backend, Joy-Con), Mobile (Android/iOS), VR/XR (OpenXR), WebAssembly (WebGPU). Platform abstraction layer (`PlatformTarget`) with adaptive quality, input/save/achievement abstractions
- **Accessibility:** ~~Screen reader support~~ (done — Announcer), ~~keyboard-only navigation~~ (done — panel focus, gizmo nudge), ~~motor accessibility~~ (done — dwell-click, sticky drag, alternative input devices), ~~dyslexia mode~~ (done — letter/word/line spacing), ~~colorblind-safe theme~~ (done — blue/orange palette), ~~switch access~~ (done — one-button auto-scan)
- **Collaboration:** Git integration, ~~scene/entity locking~~ (done — .enjinlock advisory locks), ~~collaborative editing~~ (done — OT protocol UI, peer cursors, conflict resolution, session management)
- **Flash Game Revival:** ~~Vector drawing editor~~ (done), ~~Newgrounds.io API~~ (done), ~~HTML5 export~~ (done), ~~SWF import~~ (done — SWFConverter), ~~AS2/AS3 transpiler~~ (done — AS3Transpiler + FlashAPIShim), ~~symbol library~~ (done — reusable symbols as prefabs, nested editing, Flash timeline sync), ~~Newgrounds game page~~ (done — themed HTML5 template with medals/scoreboard/embed), ~~SharedObject persistence~~ (done — mapped to TieredSaveSystem meta-progression, 5 AS bindings), Flash-style authoring

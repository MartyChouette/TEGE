# Enjin Engine Architecture Documentation

## Overview

Enjin Engine is a proprietary, licensable 3D game engine built from scratch using C++20 and the Vulkan graphics API. It features a complete ImGui-based editor, an Entity-Component-System architecture, and modern rendering capabilities.

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  (Editor, Game Player, Standalone Runtime)               │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                    Engine Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Renderer   │  │   Physics    │  │    Audio     │  │
│  │   System     │  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Scripting   │  │   Scene      │  │   Effects    │  │
│  │  (AngelScript)│  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │    ECS       │  │   Gameplay   │  │  Procedural  │  │
│  │   System     │  │   Systems    │  │  Generation  │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Editor     │  │   Assets     │  │   Plugin     │  │
│  │   System     │  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Debug /    │  │  Animation   │  │   Level      │  │
│  │   Profiler   │  │  Timeline    │  │  Streaming   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                    Core Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Memory     │  │     Math     │  │   Logging    │  │
│  │  Management  │  │   Library    │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Platform    │  │   Window     │  │    Input     │  │
│  │  Abstraction │  │   System     │  │   System     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                 Vulkan API Layer                         │
│  (Instance, Device, Queues, Commands, Swapchain)        │
└─────────────────────────────────────────────────────────┘
```

## File Structure

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
│   │   ├── AI/             # AIBehaviors, Navmesh, A* Pathfinding
│   │   ├── Animation/      # Sprite + skeletal animation, Timeline/Sequencer
│   │   ├── Assets/         # GLTFLoader, SceneImporter, Prefab
│   │   ├── Audio/          # AudioSystem, SimpleAudio (miniaudio backend)
│   │   ├── Debug/          # Profiler, ScopeTimer, FrameData tracking
│   │   ├── ECS/            # Entity-Component-System
│   │   │   ├── Components/ # 70+ component types (incl. joints, ragdoll, behavior trees, dialogue box)
│   │   │   │   ├── Controllers/  # 5 character controller types
│   │   │   │   └── ...
│   │   │   └── Systems/    # RenderSystem, ControllerSystem
│   │   ├── Editor/         # EditorLayer, PlayMode, PlayModeDiff, EditorSettings, FeedbackSystem, PerformanceStats, VectorDrawingEditor
│   │   ├── Effects/        # Weather, Water, Wind, RetroEffects, Destructible, SpriteTextureAtlas, SpriteContourTracer
│   │   ├── GUI/            # ImGui integration, Localization, DialogueTree, UICanvas, UISystem
│   │   ├── Gameplay/       # TieredSaveSystem, SaveBackend, SaveLoadMenu, HUDSystem, QuestSystem, FootstepSystem, ObjectPool, CinematicSystem, DialogueAsset
│   │   ├── Networking/     # LANMultiplayer, NetworkPanel, NewgroundsSaveBackend, SteamSaveBackend
│   │   ├── Physics/        # SimplePhysics, PhysicsWorld, ConstraintSolver, Physics2D
│   │   ├── Platform/       # FileDialog
│   │   ├── Plugin/         # PluginSystem, HotReload
│   │   ├── Procedural/     # LevelGenerator
│   │   ├── Renderer/       # Vulkan renderer, RenderBackend abstraction
│   │   │   ├── Vulkan/     # VulkanContext, Pipeline, Buffer, etc.
│   │   │   └── RayTracing/ # RT pipeline, acceleration structures, denoiser
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   ├── Scripting/      # AngelScript engine, bindings, coroutines, events
│   │   └── VisualScript/   # NodeDefinition, NodeRegistry, VisualScriptExecutor
│   ├── shaders/            # GLSL shaders (triangle.vert/frag, grass.vert/frag)
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

## Key Systems

### Rendering System

**Components**:
- `VulkanRenderer` - Main renderer, swapchain management
- `VulkanContext` - Vulkan instance, device, queues
- `VulkanPipeline` - Graphics pipeline with descriptor sets
- `VulkanBuffer` - GPU buffers (vertex, index, uniform, storage)
- `RenderSystem` - ECS system that renders entities with Mesh+Transform
- `PostProcessing` - Bloom, vignette, color grading, FXAA, film grain
- `RenderTarget` - Offscreen rendering for Game View

**Features**:
- Blinn-Phong lighting with multi-light support
- PBR material system with base color, normal, and height maps
- Shadow mapping with PCF filtering (directional CSM, point cubemap, spot 2D array)
- Skeletal animation with GPU skinning (bone SSBO)
- Instanced grass rendering
- Retro rendering effects (per-material)
- Wireframe rendering mode
- Text-to-texture rasterization (stb_truetype)
- Ray tracing pipeline (hybrid raster+RT, path tracing mode)
- SVGF compute denoiser (temporal, variance, a-trous wavelet)

### Ray Tracing System

**Components**:
- `RTCapabilities` - Extension detection and hardware properties query
- `AccelerationStructure` (BLAS/TLAS) - Low-level AS wrappers with vkGetDeviceProcAddr function pointers
- `AccelerationStructureManager` - BLAS cache by mesh hash, per-frame TLAS rebuild
- `RTPipeline` - RT pipeline wrapper with shader binding table (SBT) construction
- `RTShadows` - 1 SPP shadow ray dispatch with configurable distance/radius
- `RTReflections` - Single-bounce specular reflection dispatch
- `RTAmbientOcclusion` - Short-range AO hemisphere sampling
- `RTGlobalIllumination` - Multi-bounce diffuse GI
- `PathTracer` - Progressive path tracer with accumulation buffer and SPP tracking
- `SVGFDenoiser` - 3-pass compute denoiser (temporal accumulation, variance estimation, a-trous wavelet)
- `RTCompositor` - Fullscreen compute shader compositing RT layers into scene HDR

**Features**:
- Hybrid raster+RT pipeline (raster for primary visibility, RT for lighting effects)
- BLAS per unique mesh with hash-based deduplication
- TLAS rebuilt per frame from entity transforms (UPDATE mode for transform-only changes)
- Per-effect enable/disable with independent configuration
- Progressive path tracing mode with automatic reset on camera/scene changes
- SVGF denoising with configurable temporal alpha and a-trous iterations
- RT descriptor set (14 bindings, separate from main pipeline set 0)
- Graceful fallback: placeholder SPIR-V stubs detected and skipped, raster path unaffected
- Only active for Scene3D render mode (2D/2.5D scenes skip RT entirely)
- Editor panel with per-effect toggles, config sliders, BLAS/instance stats

**Files**: `Engine/include/Enjin/Renderer/RayTracing/`, `Engine/src/Renderer/RayTracing/`, `Engine/shaders/rt_*.glsl`, `Engine/shaders/svgf_*.comp`, `Engine/shaders/rt_composite.comp`

### ECS System

**Components**:
- `World` - Main ECS container managing entities and components
- `Entity` - ID-based entities (u64)
- 70+ component types across categories:
  - Core (Transform, Mesh, Material, Light, Camera, Name, Notes, Text)
  - Controllers (Platformer2D, TopDown2D, TopDown3D, ThirdPerson, FirstPerson, Vehicle, Possessable)
  - Terrain (TerrainComponent, Terrain2DComponent)
  - Physics (Rigidbody, BoxCollider, SphereCollider, CapsuleCollider, TriggerZone)
  - Joints (DistanceJoint, HingeJoint, BallSocketJoint, SpringJoint, FixedJoint, SliderJoint, Ragdoll)
  - Environment (WeatherZone, WaterVolume, GrassVolume, Vegetation, Temperature, Gravity, CameraTrigger)
  - Combat (Health, Damage, DamageResistance, Resource)
  - Gameplay (QuestState, HUDWidget, CinematicCamera, Footstep, Poolable, SaveData [with PersistenceTier + tags], SaveLoadMenu, Interactable, Pickup, Inventory, Timer, Audio, Tag, SpawnPoint, Script, LOD, DialogueBoxComponent, PerFrameColliderComponent, PolygonCollider2DComponent)
  - AI (AIController, FollowTarget, LookAtTarget, Waypoint, BehaviorTreeComponent)
  - Visual (Billboard, ParticleEmitter, Sprite2D, AnimatedSprite2D, Tilemap, Camera2DBounds)
  - Streaming (StreamingVolume, StreamingPortal)
  - Timeline (TimelineComponent)
  - Networking (NetworkIdentity, NetworkTransform)
  - Other (StateMachine, Dialogue, Skeleton, Animator)

### Editor System

**Components**:
- `EditorLayer` - Main editor with ImGui panels and menus
- `PlayMode` - Play/Pause/Stop game preview with state save/restore
- `SceneManager` - Multi-scene project management with transitions
- Template Selector - Startup project templates

**Features**:
- 20+ editor panels (Hierarchy, Inspector, Console, Asset Browser, Editor Settings, Project Settings, Post Processing, Retro Effects, Rendering, Game View, Scene List, Stats Overlay, Profiler, Pixel Editor, Vector Drawing, Behavior Tree, Procedural Generation, Sprite Sheet Importer, Bug Reports & Feedback, Network, Animation Timeline, Visual Script, Quest Flow)
- Transform gizmos (translate, rotate, scale) via ImGuizmo
- Entity selection via ray casting (click-to-select)
- Entity clipboard (Cut/Copy/Paste via JSON serialization)
- Scene management with project manifests and scene transitions
- Startup template selector with 38 built-in templates + custom templates
- Terrain sculpting brushes (raise, lower, flatten, smooth, paint) with viewport ray-heightmap intersection
- 2D terrain control point drag-to-edit in viewport
- Bug reporting and feedback system with auto-captured diagnostics
- Command palette (Ctrl+P) with fuzzy search and 25+ commands

### Scene System

**Components**:
- `SceneSerializer` - Save/load scenes as JSON (.enjin files)
- `SceneManager` - Project manifests, scene lists, runtime loading, transitions

**Features**:
- Full serialization of all 70+ component types (including joints, ragdoll, and networking)
- Project manifest format (.enjinproject)
- Scene build indices and start scene designation
- Scene transitions (Instant, Fade Black, Fade White, Cross Fade)
- Additive scene loading
- Save/load to string (for clipboard operations)

### Effects System

**Components**:
- `WindSystem` - Global wind affecting weather, vegetation, and grass
- `WeatherSystem` - Rain, snow, fog, storm with lightning
- `Water3D` - 3D water plane with Gerstner wave simulation
- `RetroEffects` - CRT, pixelation, dithering post-processing
- `GrassRenderer` - Instanced grass blades with wind sway
- `WeatherRenderer` - 3D weather particle rendering

### Physics System

**SimplePhysics** (collision queries and character movement):
- Collision detection (sphere-sphere, AABB-AABB, sphere-AABB)
- **Spatial hash grid** broad-phase for collision events — O(N) typical vs O(N²) brute-force. FNV-1a cell hashing, 4m default cell size
- **Per-frame collider cache** — all entities with Box/Sphere/Capsule colliders queried once per `Update()`, reused by ground check, raycasts, MoveAndSlide, overlap queries, and collision event detection
- Raycasting (single hit and multi-hit) against cached collider list
- Move-and-slide for character controllers
- Ground detection (raycast downward)
- Configurable gravity (zone query hoisted outside rigidbody loop)

**PhysicsWorld** (impulse-based rigid body dynamics):
- RigidBody objects with mass, velocity, restitution, friction
- Sphere and Box collision shapes
- Impulse-based collision response with friction
- Positional correction with Baumgarte stabilization
- ECS integration bridge (SyncFromECS / SyncToECS)

**ConstraintSolver** (joint system):
- Sequential impulse solver with configurable iterations (default 8)
- Warm starting for improved convergence
- Baumgarte stabilization for position drift correction
- 6 joint types:
  - `DistanceJoint` - Fixed distance between entities with stiffness
  - `HingeJoint` - Rotation around one axis with angle limits and motor
  - `BallSocketJoint` - Free rotation with cone and twist limits
  - `SpringJoint` - Hooke's law spring with damping
  - `FixedJoint` - Rigid connection (breakable under force)
  - `SliderJoint` - Translation along one axis with limits and motor
- All joints support breakable mode with force threshold and stress tracking

**RagdollComponent**:
- Per-bone joint definitions mapped to skeleton
- Blend weight for animation-to-ragdoll transition
- Auto-settle detection

**Additional features**:
- Auto-generated box colliders on model import
- Gravity zones (directional, point, zero-G overrides)
- Temperature zones (heat/cold environmental effects)

### 2D Physics System

**PhysicsWorld2D** (2D impulse-based dynamics):
- Circle, Box, Polygon collision shapes
- SAT (Separating Axis Theorem) collision detection
- 5 joint types (Distance, Revolute, Prismatic, Weld, Wheel)
- Continuous Collision Detection (CCD)
- Physics materials (friction, restitution, density)
- 2D raycasts

### Feedback System

- `FeedbackManager` - Bug report and feedback CRUD with JSON persistence
- `DiagnosticSnapshot` - Auto-captured engine state (version, GPU, RAM, FPS, scene)
- Remote submission via HTTPClient, search/filter, JSON export

### Gameplay Systems

- `TieredSaveSystem` - 20-slot tiered save/load (17 manual + 3 rotating auto-save) with 3-tier persistence (SceneState/RunState/MetaProgression), meta-progression key-value store, auto-save, checkpoints, pluggable backends (`ISaveBackend`: `LocalSaveBackend`, `NewgroundsSaveBackend`, `SteamSaveBackend`)
- `PlayModeDiff` - JSON diff of pre/post play mode scene states with cherry-pick apply dialog (created/deleted/modified entities, component-level diffs)
- `SaveLoadMenuComponent` - In-game save/load grid overlay (ImGui) with slot cards, confirmation dialogs, save/load/delete per slot
- `HUDSystem` - Runtime health bars, resource bars, labels, crosshair
- `QuestSystem` - Quest state tracking and progression
- `FootstepSystem` - Surface-based footstep audio
- `ObjectPool` - Entity recycling with lifetime auto-release
- `CinematicSystem` - Waypoint-based camera sequences with easing
- `EntityEventBus` - Decoupled C++ entity communication
- `DestructibleSystem` - Voronoi, grid, radial, shatter fracture patterns with debris physics
- `LocalizationManager` - String tables, CSV/JSON I/O, LOC() macro for UI text
- `DialogueAsset` - .enjdlg dialogue files with tree editor

### Terrain System

**3D Terrain** (`TerrainComponent`):
- Grid-based heightmap (`gridWidth x gridHeight`, configurable cell size)
- 4-layer splatmap for texture blending (RGBA weights per cell)
- Height queries: `GetHeight(x, z)` / `SetHeight(x, z, h)` with automatic `meshDirty` flagging
- `InitializeFlat(height)` creates a flat heightmap with default splatmap (layer 0 = 100%)
- Per-layer texture paths and tile scale
- RenderSystem auto-regenerates mesh when `meshDirty = true`

**2D Terrain** (`Terrain2DComponent`):
- Polyline control points (XY plane, auto-sorted by X)
- Configurable fill depth below surface
- UV scale and texture path
- Optional auto-collider generation from control points

**Terrain Brush** (editor tool):
- 5 brush modes: Raise, Lower, Flatten, Smooth, Paint
- Configurable radius, strength, falloff
- Smoothstep falloff function for natural blending
- Ray-heightmap intersection with iterative march + binary search refinement
- Real-time brush cursor coordinate feedback in inspector
- 2D terrain: click-to-grab nearest control point, drag to reposition

### AI System

- `AIBehaviors` - State-based AI (idle, patrol, chase, attack, flee, dead)
- `NavmeshSystem` - Navigation mesh generation from scene geometry
- `Pathfinding` - A* pathfinding on navmesh with debug visualization
- `AIControllerComponent` - Per-entity AI state, detection range, FOV, patrol points
- `FollowTargetComponent` - Smooth entity following with offset and distance constraints
- `LookAtTargetComponent` - Entity rotation toward target with angle limits
- `WaypointComponent` - Linked waypoint chains for patrol routes

### Assets System

- `GLTFLoader` - Loads .gltf/.glb files (meshes, materials, skins, animations)
- `SceneImporter` - Converts loaded models to ECS entities
- `MeshFactory` - Primitive mesh generation (cube, sphere, plane, cylinder, cone, quad)

### Scripting System (AngelScript)

**Architecture**:
- `ScriptEngine` - Manages AngelScript VM, module compilation, hot-reload
- `ScriptBindings` - Registers all engine APIs with the script VM
- `TegeBehavior` - Base class for all gameplay scripts (analogous to MonoBehaviour)
- `CoroutineScheduler` - Manages script coroutines (yield seconds, frames, end-of-frame)
- `ScriptEventBus` - Script-to-script event dispatch system

**Script Bindings** (5 categories):
- **Scene**: Entity transform access (Get/Set Position/Rotation/Scale/Name), scene loading
- **Physics**: Raycast, sphere/box overlap, force/impulse/velocity, gravity scale
- **Audio**: Play/stop/volume/pitch per entity, positional audio, master volume
- **Components**: Health, Material, Light, Camera, AudioSource, Animator, Controller (40+ functions)
- **Core**: Coroutines (StartCoroutine, Yield*), Events (Listen, Send, Broadcast), logging, input, time

**Script Lifecycle**: `OnCreate()` → `OnUpdate(deltaTime)` per frame → `OnDestroy()`

### Debug & Profiler System

- `Profiler` singleton with `ENJIN_PROFILE_SCOPE("name")` macro
- Per-frame breakdown: render, physics, scripting, ECS, audio
- FPS counter, frame time history (240-frame rolling window)
- Draw call, entity, and triangle counters
- Memory usage tracking
- ImGui overlay panel with graphs, progress bars, detailed scope table

### Plugin System

- `IPlugin` interface: `OnLoad()`, `OnUnload()`, `OnUpdate()`, `GetName()`, `GetVersion()`
- Dynamic library loading (LoadLibrary/dlopen/dlopen)
- Plugin manifest (JSON: name, version, dependencies)
- Plugin registration with engine subsystems
- Editor panel for plugin management (load, unload, status display)

### Animation Timeline / Sequencer

- `TimelineComponent` with property, event, and animation tracks
- Property track: keyframe any component field over time (position, rotation, scale, material)
- Event track: fire callbacks at specific timestamps
- Animation track: play/blend skeletal animations
- Easing functions: Linear, EaseIn, EaseOut, EaseInOut, Step
- Loop and ping-pong playback modes

### Hot-Reload System

- File watcher on gameplay source directory
- On change: save state → unload DLL → recompile → reload → restore state
- `ENJIN_GAMEPLAY_CLASS(ClassName)` macro for exportable classes
- Platform-specific DLL/SO loading and compilation
- Reload callback system with error tracking

### Level Streaming

- `StreamingManager` - Distance-based chunk loading/unloading
- `StreamingChunk` - Spatial region with entity list, load state, LOD level
- `StreamingVolumeComponent` - Defines chunk boundaries
- `StreamingPortalComponent` - Connects chunks (doorways, corridors)
- Priority-sorted load queue with concurrent load limiting
- Async chunk loading via SceneSerializer
- ImGui debug overlay showing chunk states

### Render Backend Abstraction

- `IRenderBackend` interface for platform-agnostic rendering
- `BuildTarget` enum: Windows, Linux, macOS, Android, iOS, WebGL
- `TextureCompression` enum: BC1, BC3, BC7, ETC2, ASTC, PVRTC
- `PlatformCapabilities` struct for device feature queries
- `PlatformInput` abstraction for touch and motion input

## Descriptor Bindings

```
Binding 0: View/Projection UBO (vertex shader)
Binding 1: Lighting UBO with multi-light arrays (vertex + fragment)
Binding 2: Material UBO (fragment shader)
Binding 3: Base color texture sampler (fragment shader)
Binding 4: Shadow map sampler (fragment shader)
Binding 5: Height map for parallax mapping (fragment shader)
Binding 6: Normal map (fragment shader)
Binding 7: Bone matrix SSBO for skeletal animation (vertex shader)
```

## Push Constants (128 bytes, per-object)

```cpp
struct PushConstants {
    Matrix4 model;          // 64 bytes
    Vector3 baseColor;      // + metallic = 16 bytes
    Vector3 emissiveColor;  // + roughness = 16 bytes
    f32 emissiveStrength, opacity, alphaCutoff;
    i32 flags;              // bit field: render/alpha/texture/retro flags
    f32 parallaxScale;      // + padding = 16 bytes
};
```

## Design Patterns

### Component-Based Architecture
- Entities are u64 IDs
- Components are plain data structs
- Systems contain logic and operate on components
- Data-oriented design for cache efficiency

### Data-Driven Design
- Scenes saved as JSON
- Project manifests as JSON
- Material properties via component fields
- Room prefabs defined in JSON

## Code Conventions

- **Types:** `u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, usize`
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Scene`, `Enjin::Effects`, `Enjin::Gameplay`, `Enjin::Physics`, `Enjin::Scripting`, `Enjin::Debug`, `Enjin::Plugin`, `Enjin::Animation`, `Enjin::AI`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- **Log Categories:** Core, Renderer, Physics, Audio, Asset, Script, Editor, Game, AI, Assets, Procedural, Animation, Build, Player
- **API export:** `ENJIN_API` macro for DLL export

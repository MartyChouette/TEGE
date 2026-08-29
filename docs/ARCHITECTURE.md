# Enjin Engine Architecture Documentation

## Overview

Enjin Engine is an open-source (BSL 1.1) 3D game engine built with C++20 and the Vulkan graphics API. It features a complete ImGui-based editor, an Entity-Component-System architecture, and modern rendering capabilities.

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
│   │   ├── Math/           # Vector, Matrix, Quaternion, Spline, Noise
│   │   ├── Memory/         # Custom allocators (Stack, Pool, Linear, FrameAllocator)
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
│   │   ├── Networking/     # LANMultiplayer, NetworkPanel, SteamSaveBackend
│   │   ├── Physics/        # IPhysicsBackend, JoltBackend, Box2DBackend, PhysicsBackendFactory
│   │   ├── Platform/       # FileDialog
│   │   ├── Plugin/         # PluginSystem, HotReload
│   │   ├── Procedural/     # LevelGenerator
│   │   ├── Renderer/       # Multi-backend renderer, IRenderBackend, GPUTypes, GPUCapabilities
│   │   │   ├── Vulkan/     # VulkanContext, Pipeline, Buffer, etc.
│   │   │   ├── WebGPU/     # WebGPURenderer, WebRenderPipeline (Emscripten/Dawn)
│   │   │   ├── RayTracing/ # RT pipeline, acceleration structures, denoiser
│   │   │   ├── GPUDriven/  # GPU culling, HiZ occlusion culling
│   │   │   ├── VRS/        # Variable Rate Shading
│   │   │   ├── VirtualTexture/ # Page-based virtual texture streaming
│   │   │   └── VisibilityBuffer/ # Visibility buffer + material resolve
│   │   ├── Scene/          # SceneSerializer, SceneManager, LevelStreaming
│   │   ├── Scripting/      # AngelScript engine, bindings, coroutines, events
│   │   └── VisualScript/   # NodeDefinition, NodeRegistry, VisualScriptExecutor
│   ├── shaders/            # GLSL shaders (triangle.vert/frag, grass.vert/frag, compute shaders)
│   └── src/
│
├── Editor/                  # Editor application (main.cpp entry point)
│
├── Player/                  # Standalone game player (no editor/ImGui)
│
├── installer/               # Inno Setup installer (EnjinSetup.iss, icons)
│
├── third_party/            # External dependencies
│   ├── imgui/              # Dear ImGui (UI)
│   └── imguizmo/           # Transform gizmos
│
└── build/                  # Build output (bin/, lib/)
```

## Key Systems

### Rendering System

**Multi-Backend Architecture** (in progress):
- `IRenderBackend` - Abstract backend interface with sub-interfaces for buffers, textures, pipelines, shaders, bind groups, and render encoders
- `GPUTypes.h` - Typed opaque resource handles (`GPUBufferHandle`, `GPUTextureHandle`, etc.) and backend-agnostic enums
- `GPUCapabilities.h` - Feature detection (push constants, compute, RT, indirect draws) with presets for Vulkan, WebGPU, and Metal

**Vulkan Backend** (desktop — Windows, Linux):
- `VulkanRenderer` - Main renderer, swapchain management
- `VulkanContext` - Vulkan instance, device, queues
- `VulkanPipeline` - Graphics pipeline with descriptor sets
- `VulkanBuffer` - GPU buffers (vertex, index, uniform, storage)

**WebGPU Backend** (browser — WebAssembly):
- `WebGPURenderer` - WebGPU device/surface lifecycle, frame encoding
- `WebRenderPipeline` - PBR rendering with Cook-Torrance BRDF, multi-light, texture loading

**Shared**:
- `RenderSystem` - ECS system that renders entities with Mesh+Transform (migrating to `IRenderBackend`)
- `PostProcessing` - Bloom, vignette, color grading, FXAA, film grain, DoF, tilt-shift, stipple/dither, SSAO, god rays, contact shadows, caustics, fog shafts
- Outline pipeline (cel shading) - Inverted-hull geometry outlines (`outline.vert`/`outline.frag`), per-material width/color, NPR curvature-driven thickness (`celOutlineCurvatureWeight`)
- `RenderTarget` - Offscreen rendering for Game View
- Art Style Presets - 7 one-click presets (Realistic PBR, Classic Blinn-Phong, Hand-Painted, Toon/Anime, Low-Poly Retro, Pixel Art, NPR Sketch)
- `ReflectionProbeSystem` - Box-projected cubemap reflections with per-probe baking, proximity blending
- `AsyncComputeScheduler` - Overlaps RT dispatch with rasterization on dedicated compute queue
- `IndirectDrawBatcher` - Groups textured pool entities by texture hash for batched indirect draws
- IUpscaler - FSR 2 (built-in Lanczos+CAS), DLSS 3.5 (stub), XeSS (stub)
- `DynamicDifficultySystem` - Adaptive difficulty with 6 input metrics and 6 output multipliers

**Features**:
- Blinn-Phong lighting with multi-light support
- PBR material system with base color, normal, height, metallic-roughness, emissive, and matcap maps
- Material transmission (glass/water), IOR, thickness, subsurface scattering (intensity/radius/color)
- Procedural surface noise (surfaceNoiseScale/surfaceNoiseStrength per material)
- Shadow mapping with PCF filtering (directional CSM, point cubemap, spot 2D array)
- Skeletal animation with GPU skinning (bone SSBO), skinned mesh shadow pass
- Multi-material sub-mesh rendering (MaterialSlotsComponent + MeshComponent::SubMesh)
- Instanced grass rendering
- Retro rendering effects (per-material)
- Wireframe rendering mode
- Text-to-texture rasterization (stb_truetype)
- Ray tracing pipeline (hybrid raster+RT, path tracing mode)
- SVGF compute denoiser (temporal, variance, a-trous wavelet)
- OIDN denoiser (Intel neural denoise, optional)
- SH light probes (L2, grid baking, wired to LightingUBO)
- OIT (weighted blended, fullscreen composite)
- Screen-space effects (SSAO, god rays, contact shadows, caustics, fog shafts)
- Dithered gradient rendering (per-material, 2-8 bands, 6 patterns)
- Camera presets (9 built-in)
- 64-bit material sort keys (`[8:pipeline][16:material][24:texture][16:depth]`) for cache-friendly draw ordering
- LOD with hysteresis dead-zones and screen-space projected size metric
- Scene classification: `Scene2D` (sprites only, shadows skipped), `Scene2_5D` (sprites+lights), `Scene3D` (full pipeline)
- Motion Vectors: Per-pixel velocity buffer (RG16F, swapchain MRT attachment; offscreen RTs use single color + depth) for temporal techniques
- TAA: Temporal anti-aliasing with Halton(2,3) jitter, neighborhood clamping, velocity reprojection, history ping-pong buffers, configurable sharpness/feedback; AA mode selection (None/FXAA/TAA/SMAA)

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
- `RTTranslucency` - Translucency ray dispatch (transmission materials)
- `RTCaustics` - Caustic light patterns from refractive surfaces
- `PathTracer` - Progressive path tracer with accumulation buffer, SPP tracking, NEE, MIS, Russian Roulette, and firefly clamping
- `SVGFDenoiser` - 3-pass compute denoiser (temporal accumulation, variance estimation, a-trous wavelet)
- `RTCompositor` - Fullscreen compute shader compositing RT layers into scene HDR

**Features**:
- Hybrid raster+RT pipeline (raster for primary visibility, RT for lighting effects)
- BLAS per unique mesh with hash-based deduplication
- TLAS rebuilt per frame from entity transforms (UPDATE mode for transform-only changes)
- Per-effect enable/disable with independent configuration
- Progressive path tracing mode with automatic reset on camera/scene changes, Cook-Torrance BRDF with GGX importance sampling, Next Event Estimation (NEE) with uniform light selection, Multiple Importance Sampling (MIS) using power heuristic, Russian Roulette path termination, firefly clamping (per-bounce and final), and simplified material fallback for deep bounces
- SVGF denoising with configurable temporal alpha and a-trous iterations; optional OIDN and OptiX denoisers
- OptiX denoiser CUDA interop wired (timeline semaphore sync, shared Vulkan/CUDA buffers)
- Material SSBO in RT hit shaders (binding 9) for full PBR material access during ray traversal
- RTCompositor enable flags: bits 0-5 (shadows/reflections/AO/GI/translucency/caustics)
- RT descriptor set (27 bindings: 0-13 base, 14=translucency, 15=caustics, 16=NEE lights, 17=SDF, 18=simplified materials, 19-20=ReSTIR reservoirs, 21-23=screen-space radiance cache, 24-26=surfel radiance cache; separate from main pipeline set 0)
- Graceful fallback: placeholder SPIR-V stubs detected and skipped, raster path unaffected
- Only active for Scene3D render mode (2D/2.5D scenes skip RT entirely)
- Editor panel with per-effect toggles, config sliders, BLAS/instance stats

**Files**: `Engine/include/Enjin/Renderer/RayTracing/`, `Engine/src/Renderer/RayTracing/`, `Engine/shaders/rt_*.glsl`, `Engine/shaders/svgf_*.comp`, `Engine/shaders/rt_composite.comp`

## Performance Optimization Subsystems

### Clustered Forward Lighting
Replaces brute-force per-fragment light loops with spatial cluster lookup. The screen is divided into a 16×9×24 grid (3456 clusters) with exponential depth slicing. A compute pre-pass assigns lights to clusters via sphere-AABB intersection tests. Fragments look up their cluster to evaluate only relevant lights. Enabled by default (`ENJIN_CLUSTERED_LIGHTING`).

**Pipeline:** `light_cluster_bounds.comp` → `light_cluster_assign.comp` → fragment shader cluster lookup (bindings 14-15)

### GPU Two-Phase HiZ Occlusion Culling
Extends the existing GPU frustum culling with hierarchical Z-buffer occlusion testing. Phase 0 performs frustum + HiZ culling and generates a partial HiZ pyramid from visible objects. Phase 1 re-tests initially-occluded objects against the updated HiZ, recovering objects that were incorrectly culled. Always-on when HiZ pyramid is available.

### Async Compute Overlap
Overlaps compute workloads (GPU culling, light cluster assignment) with graphics work (shadow passes) using timeline semaphores. Uses the dedicated compute queue when available, falls back to graphics queue.

**Frame timeline:**
```
Compute: [Cull Phase 0] [Light Cluster Assign]
Graphics: [Shadow Pass] → wait compute → [Main Render Pass]
```

### Variable Rate Shading (VRS)
Per-tile shading rate control via `VK_KHR_fragment_shading_rate`. Modes: Peripheral (distance-based), Content Adaptive (luminance variance), Motion Based (velocity), Full (combined). Generates a shading rate image via compute shader. Off by default (`ENJIN_VRS`).

### Virtual Texturing
Page-based texture streaming with 128×128 tiles in an 8K×8K physical atlas (4096 pages). Feedback buffer at 1/8 resolution identifies needed pages. Background streaming thread with LRU eviction. Lowest-mip fallback for unloaded pages. Off by default (`ENJIN_VIRTUAL_TEXTURING`).

### Visibility Buffer
Alternative render path: geometry-only pass writes triangle ID + instance ID to an R32G32_UINT buffer, followed by a full-screen compute resolve that fetches vertices, computes barycentrics, and evaluates materials. Reduces overdraw and bandwidth for complex scenes. Off by default (`ENJIN_VISIBILITY_BUFFER`).

### CPU-Side Optimizations
- **Per-frame linear allocator:** `FrameAllocator` (8 MB bump allocator reset each frame) with `FrameArray<T>` container, replaces hot-path `std::vector` allocations (`Core/include/Enjin/Memory/FrameAllocator.h`)
- **64-bit material sort key:** `[8:pipeline][16:material][24:texture][16:depth]` layout for cache-friendly single-comparison sorting
- **MaterialGPU:** 112-byte GPU-aligned struct with transmission/IOR/thickness/SSS fields and bindless texture indices (uploaded via batched Material SSBO at binding 2; size guarded by static_assert in TestMaterial)
- **LOD hysteresis:** Directional dead-zones prevent LOD ping-ponging, with optional screen-space projected size metric
- **Binary search keyframes:** `Animation.cpp::SampleKeyframes()` uses `std::upper_bound` for O(log N) lookups instead of O(N) linear scan
- **Integer sprite sort keys:** `SpriteBatchRenderer` uses pre-hashed `usize` keys instead of `std::string` comparison in sort comparator
- **Cached ECS storage pointers:** `World::GetComponentStorage<T>()` public API; `RenderSystem` caches 5 hot storage pointers, replacing 38 `GetComponent()` calls with direct `storage->Get()`
- **Light entity list dirty flag:** `m_LightListDirty` gates rebuild on structural changes, with O(1) size-mismatch recovery
- **Pre-allocated sprite shadow vectors:** `SpriteBatchRenderer` reuses member vectors, eliminating per-frame heap allocations in shadow pass
- **Cached world matrices:** `TransformComponent` mutable dirty flag with recursive parent caching, O(1) for unchanged transforms across multiple render passes
- **Batched material SSBO:** Binding 2 converted from per-entity UBO to `STORAGE_BUFFER_DYNAMIC` with single per-frame upload and dynamic offset per draw
- **ECS View template:** `ECS::View<Components...>` variadic template for efficient filtered multi-component iteration with smallest-set optimization

### CMake Feature Flags

| Flag | Default | Description |
|------|---------|-------------|
| `ENJIN_CLUSTERED_LIGHTING` | ON | Clustered forward lighting (16x9x24 grid) |
| `ENJIN_VRS` | OFF | Variable Rate Shading (`VK_KHR_fragment_shading_rate`) |
| `ENJIN_VIRTUAL_TEXTURING` | OFF | Page-based virtual texture streaming |
| `ENJIN_VISIBILITY_BUFFER` | OFF | Visibility buffer with deferred material resolve |
| `ENJIN_PHYSICS_JOLT` | ON | Jolt Physics v5.2.0 (3D) |
| `ENJIN_PHYSICS_BOX2D` | ON | Box2D v3.0.0 (2D) |
| ~~`ENJIN_PHYSICS_SIMPLE`~~ | — | Removed (legacy backend retired) |
| `ENJIN_RAYTRACING_OIDN` | — | Intel Open Image Denoise support |
| `ENJIN_RAYTRACING_OPTIX` | — | NVIDIA OptiX denoiser support |

### ECS System

**Components**:
- `World` - Main ECS container managing entities and components
- `Entity` - ID-based entities (u64)
- 140+ serializable component types (142 measured) across categories:
  - Core (Transform, Mesh, Material, MaterialSlots, Light, Camera, Name, Notes, Text)
  - Rendering (MeshRenderer, ArtStyle)
  - Controllers (Platformer2D, TopDown2D, TopDown3D, ThirdPerson, FirstPerson, Vehicle, Possessable)
  - Terrain (TerrainComponent, Terrain2DComponent)
  - Physics (Rigidbody, BoxCollider, SphereCollider, CapsuleCollider, MeshCollider, TriggerZone)
  - Joints (DistanceJoint, HingeJoint, BallSocketJoint, SpringJoint, FixedJoint, SliderJoint, Ragdoll)
  - Environment (WeatherZone, WaterVolume, GrassVolume, Vegetation, Temperature, Gravity, CameraTrigger)
  - Combat (Health, Damage, DamageResistance, Resource)
  - Gameplay (QuestState, HUDWidget, CinematicCamera, Footstep, Poolable, SaveData [with PersistenceTier + tags], SaveLoadMenu, Interactable, Pickup, Inventory, Timer, Audio, Tag, SpawnPoint, Script, LOD, DialogueBoxComponent, PerFrameColliderComponent, PolygonCollider2DComponent, GameOver, ParallaxMachine)
  - AI (AIController, FollowTarget, LookAtTarget, Waypoint, BehaviorTreeComponent)
  - Visual (Billboard, ParticleEmitter, Sprite2D, AnimatedSprite2D, Tilemap, Camera2DBounds)
  - Animation (Skeleton, Animator, BoneAttachment, TwoBoneIK, LookAtIK, InteractionIK, AnimationRecorder, Ragdoll)
  - Streaming (StreamingVolume, StreamingPortal)
  - Timeline (TimelineComponent)
  - Networking (NetworkIdentity, NetworkTransform)
  - Other (StateMachine, Dialogue)

### Editor System

**Components**:
- `EditorLayer` - Main editor with ImGui panels and menus
- `PlayMode` - Play/Pause/Stop game preview with state save/restore
- `SceneManager` - Multi-scene project management with transitions
- Template Selector - Startup project templates

**Features**:
- 20+ editor panels (Hierarchy, Inspector, Console, Asset Browser, Editor Settings, Project Settings, Post Processing, Retro Effects, Rendering, Game View, Scene List, Stats Overlay, Profiler, Pixel Editor, Vector Drawing, Behavior Tree, Procedural Generation, Sprite Sheet Importer, Bug Reports & Feedback, Network, Animation Timeline, Visual Script, Quest Flow)
- **Game Debug Panel (F1)** — Tabbed game-focused debug window with Scene, Physics, Scripts, Audio, and Gameplay tabs. Shows entity counts, physics body lists, script status with error indicators, audio source state, and gameplay system metrics (tweens, particles, health, interactables)
- **Debug Workstation (F2)** — Tabbed engine-focused debug window with Performance, Renderer, ECS, Scene, and System tabs. Shows FPS/frame time graphs with percentile stats, render pipeline state (shadows, RT, AA, post-processing), component counts, scene/project info, GPU/Vulkan/system details
- **Drop-down Console (`` ` ``)** — Quake/Doom-style console that slides down from the top of the screen with smooth animation. Supports 60+ commands across 10 categories (entities, transform, rendering, materials, lights, camera, scene, query, bulk, debug). Features command history (Up/Down arrows), color-coded output, and auto-focus input
- **PrepareRenderTargets** — Pre-command-buffer render target resizing to prevent Vulkan resource destruction during recording (fixes 4:3 aspect ratio crash with Vulkan hooks)
- Transform gizmos (translate, rotate, scale) via ImGuizmo
- Entity selection via ray casting (click-to-select)
- Entity clipboard (Cut/Copy/Paste via JSON serialization)
- Scene management with project manifests and scene transitions
- Startup template selector with 51 built-in templates + custom templates + template marketplace
- Terrain sculpting brushes (raise, lower, flatten, smooth, paint) with viewport ray-heightmap intersection
- 2D terrain control point drag-to-edit in viewport
- Bug reporting and feedback system with auto-captured diagnostics and Discord webhook integration (screenshot + log capture)
- Project-first workflow: Project Hub with create/delete/duplicate, auto-create on disk, `.enjinproject` file association
- Viewport shading modes (Wireframe, Solid, Lit, Lit+Shadows, Full)
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

### Animation System

**Components**:
- `SkeletalAnimator` - Animation playback with state machine, blend trees, and timeline
- `AnimationStateMachine` - State-based animation transitions with parameters and triggers
- `BlendTree` - 1D parameter-driven animation blending (evaluates two bracketing clips)
- `AnimationRetargetMap` - Bone name mapping between skeletons (auto-map by name, Mixamo prefix stripping)
- `TwoBoneIKComponent` - Analytic arm/leg IK (law of cosines solver)
- `LookAtIKComponent` - Head/neck look-at IK with max rotation and smoothing
- `InteractionIKComponent` - Hand IK toward nearby interactables
- `BoneAttachmentComponent` - Parent entity transforms to skeleton bones with local offsets
- `RagdollComponent` - Per-bone physics joints with death auto-activation and animation blending
- `AnimationRecorderComponent` - Captures bone transforms over time to create new animation clips

**Pipeline**: FBX/glTF import (Assimp) -> Skeleton + animation clips -> AnimatorComponent state machine -> blend tree evaluation -> IK solvers (two-bone, look-at) -> bone attachment updates -> ragdoll (on death). The editor calls the skeletal animator update directly (not via RenderSystem::Update) to decouple animation timing from rendering.

### Effects System

**Components**:
- `WindSystem` - Global wind affecting weather, vegetation, and grass
- `WeatherSystem` - Rain, snow, fog, storm with lightning
- `Water3D` - 3D water plane with Gerstner wave simulation
- `RetroEffects` - CRT, pixelation, dithering post-processing
- `GrassRenderer` - Instanced grass blades with wind sway
- `WeatherRenderer` - 3D weather particle rendering

### Physics System

**Backend Abstraction** (pluggable physics engines):
- `IPhysicsBackend` — abstract 3D interface (SetWorld, Update, Raycast, MoveAndSlide, collision events, etc.)
- `IPhysicsBackend2D` — abstract 2D interface (Initialize, Update, Raycast2D, OverlapCircle, collision callbacks, CCD)
- `JoltBackend` — Jolt Physics v5.2.0 backend (see below)
- `PhysicsBackendFactory` — `CreatePhysicsBackend(type, mode)` creates backend by `PhysicsBackendType` (Auto/Jolt/Box2D) and `ProjectMode`. When `ENJIN_PHYSICS_JOLT=ON`, Auto selects Jolt for 3D/Mixed modes
- CMake options: `ENJIN_PHYSICS_JOLT` (Jolt v5.2.0), `ENJIN_PHYSICS_BOX2D` (Box2D v3.0.0) — both ON by default
- PlayMode and Player own physics via `unique_ptr<IPhysicsBackend>`; all consumers accept `IPhysicsBackend*`

**JoltBackend** (production-grade 3D physics via Jolt v5.2.0):
- Full ECS↔Jolt body synchronization: per-frame reconciliation creates/destroys/updates Jolt bodies from ECS state
- Body creation: Box/Sphere/Capsule shapes from collider components, center offset via `RotatedTranslatedShape`, capsule X/Z rotation
- RigidbodyComponent mapping: mass, drag/angular drag → damping, gravity scale, freeze axes → `AllowedDOFs`, CCD → `LinearCast`
- Thread-safe contact events: `JoltContactListener` buffers contacts from Jolt worker threads behind a mutex; main thread drains during `Update()`
- Bilateral collision filtering: performed in `OnContactValidate` using per-body `categoryBits`/`collisionMask` (32-bit bilateral rule)
- 6 joint types: Distance, Hinge, BallSocket (Point+Cone), Spring, Fixed, Slider — created via `BodyLockWrite` for body access
- Gravity zones: per-body `SetGravityFactor(0)` + `AddForce(customGravity * mass)` for non-standard gravity
- Raycasting: single/multi-hit via `NarrowPhaseQuery`, layer mask filtering via custom `BodyFilter`
- Spatial queries: `GetCollidersInRadius`, `OverlapBox` via shape casts
- Broad phase: 2 layers (NonMoving/Moving), fine-grained filtering in contact listener
- Entity ID stored in Jolt `mUserData` for O(1) reverse lookup
- Update loop: SyncECSToJolt → SyncJointsToJolt → ApplyGravityZones → PhysicsSystem::Update → SyncJoltToECS → ProcessContactEvents

**RagdollComponent**:
- Per-bone joint definitions mapped to skeleton
- Blend weight for animation-to-ragdoll transition
- Auto-settle detection

**Additional features**:
- Auto-generated box colliders on model import
- Gravity zones (directional, point, zero-G overrides)
- Temperature zones (heat/cold environmental effects)

### 2D Physics System

**Box2DBackend** (Box2D v3.0.0 — production 2D physics):
- Full `IPhysicsBackend2D` implementation
- Sensor bodies (`Body2DComponent::isSensor = true`): Box2D syncs positions from ECS (not to ECS), enabling collision callbacks for controller/AI/tween-driven entities without Box2D overwriting their positions
- Bilateral collision filtering (same `categoryBits`/`collisionMask` bitmask as 3D)

### Feedback System

- `FeedbackManager` - Bug report and feedback CRUD with JSON persistence
- `DiagnosticSnapshot` - Auto-captured engine state (version, GPU, RAM, FPS, scene)
- Remote submission via HTTPClient, search/filter, JSON export

### Gameplay Systems

- `TieredSaveSystem` - 20-slot tiered save/load (17 manual + 3 rotating auto-save) with 3-tier persistence (SceneState/RunState/MetaProgression), meta-progression key-value store, auto-save, checkpoints, pluggable backends (`ISaveBackend`: `LocalSaveBackend`, `SteamSaveBackend`)
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

### Assets & Build System

- `GLTFLoader` - Loads .gltf/.glb files (meshes, materials, skins, animations)
- `AssimpLoader` - Loads FBX/OBJ/DAE/PLY/VOX via Assimp v5.4.3
- `SceneImporter` - Converts loaded models to ECS entities (auto-detect format)
- `MeshFactory` - Primitive mesh generation (cube, sphere, plane, cylinder, cone, quad)
- `BuildPipeline` - Full game export: scan, validate, pack `.enjpak`, copy player, manifest
- `HTML5Exporter` - Canvas export, preloader, responsive scaling, web embed template
- Distribution: Inno Setup installer (`installer/EnjinSetup.iss` with app icon, Start Menu/Desktop shortcuts), CMake CPack (ZIP, TGZ, DEB)

### Scripting System (AngelScript)

**Architecture**:
- `ScriptEngine` - Manages AngelScript VM, module compilation, hot-reload
- `ScriptBindings` - Registers all engine APIs with the script VM
- `TegeBehavior` - Base class for all gameplay scripts (analogous to MonoBehaviour)
- `CoroutineScheduler` - Manages script coroutines (yield seconds, frames, end-of-frame)
- `ScriptEventBus` - Script-to-script event dispatch system

**Script Bindings** (~1,010 bindings across 15+ categories):
- **Scene**: Entity transform access (Get/Set Position/Rotation/Scale/Name), scene loading
- **Physics**: Raycast, sphere/box overlap, force/impulse/velocity, gravity scale
- **Audio**: Play/stop/volume/pitch per entity, positional audio, master volume, channel mixing
- **Components**: Health, Material, Light, Camera, AudioSource, Animator, Controller (40+ functions)
- **Core**: Coroutines (StartCoroutine, Yield*), Events (Listen, Send, Broadcast), logging, input, time
- **Gameplay**: Save/load, quests, cinematics, destructibles, object pooling, weather, particles, prefabs
- **UI**: Canvas element manipulation, focus management, localization
- **AI**: Controller, behavior tree blackboard, navmesh pathfinding (34 bindings)
- **Accessibility**: Subtitles, announcer, colorblind filter, font scaling (20 bindings)
- **Input**: InputActionMap remapping, sensitivity, presets (22 bindings)
- **Rendering**: Post-processing, screen-space effects, post-process volumes
- **Networking**: LAN multiplayer

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
Binding  0: View/Projection UBO (vertex shader)
Binding  1: Lighting UBO with multi-light arrays (vertex + fragment)
Binding  2: Material SSBO (dynamic offset, batched per-frame) — fragment shader
Binding  3: Base color texture sampler (fragment shader)
Binding  4: Shadow map array (fragment shader)
Binding  5: Height map for parallax mapping (fragment shader)
Binding  6: Normal map (fragment shader)
Binding  7: Bone matrix SSBO for skeletal animation (vertex shader)
Binding  8: Metallic-roughness texture (fragment shader)
Binding  9: Emissive texture (fragment shader)
Binding 10: Point shadow cubemaps (fragment shader)
Binding 11: Spot shadow maps (fragment shader)
Binding 12: Shadow data SSBO (fragment shader)
Binding 13: Object data SSBO (vertex + fragment)
Binding 14: Cluster grid SSBO — clustered lighting (fragment shader)
Binding 15: Cluster light index SSBO — clustered lighting (fragment shader)
Binding 16: VT indirection texture — virtual texturing (fragment shader)
Binding 17: VT physical atlas — virtual texturing (fragment shader)
Binding 18: Matcap texture — material-expression art style (fragment shader)
Binding 19: Reflection probe cubemap — baked environment (fragment shader)
```

**RT Descriptor Set** (separate from main pipeline set 0, bindings 19-23):
```
Binding  0-3:  Base RT bindings (TLAS, output image, camera UBO, lighting UBO)
Binding  4:    Motion vectors (RG16F velocity buffer)
Binding  5-8:  Scene data (vertex/index buffers, instance data, textures)
Binding  9:    Material SSBO (full PBR material data for hit shaders)
Binding 10-13: Additional scene data
Binding 14:    Translucency output
Binding 15:    Caustics output
```

## Push Constants (128 bytes, per-object)

```cpp
struct PushConstants {
    Matrix4 model;          // 64 bytes
    Vector3 baseColor;      // + metallic = 16 bytes
    Vector3 emissiveColor;  // + roughness = 16 bytes
    f32 emissiveStrength, opacity, alphaCutoff;
    i32 flags;              // bit field (see layout below)
    f32 parallaxScale;      // + padding = 16 bytes
};
```

**Flags layout (32 bits):**
- Bits 0-2: render mode
- Bit 3: skinned
- Bit 4: wind
- Bits 5-7: water
- Bits 8-9: alpha mode
- Bit 10: height texture
- Bit 11: ocean
- Bit 12: UV quantize
- Bit 13: gouraud
- Bits 14-15: shadow dither mode
- Bits 16-19: texture flags (base color, normal, metallic-roughness, emissive)
- Bits 20-23: retro flags (flat shading, affine texturing, vertex snapping, stipple)
- Bits 24-28: vertex snap resolution (/8)
- Bits 29-31: shadow dither pattern

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
- **Namespaces:** `Enjin::Core`, `Enjin::Math`, `Enjin::Renderer`, `Enjin::ECS`, `Enjin::Editor`, `Enjin::Scene`, `Enjin::Effects`, `Enjin::Gameplay`, `Enjin::Physics`, `Enjin::Scripting`, `Enjin::Debug`, `Enjin::Plugin`, `Enjin::Animation`, `Enjin::AI`, `Enjin::Accessibility`, `Enjin::InputSystem`, `Enjin::Build`
- **Logging:** `ENJIN_LOG_INFO/WARN/ERROR/FATAL(Category, format, ...)`
- **Log Categories:** Core, Renderer, Physics, Audio, Asset, Script, Editor, Game, AI, Assets, Procedural, Animation, Build, Player, Network
- **API export:** `ENJIN_API` macro for DLL export

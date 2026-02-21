# CLAUDE_REFERENCE.md - Detailed Subsystem Reference

This file contains detailed subsystem documentation moved from CLAUDE.md to reduce context size.
For quick reference, see `CLAUDE.md`. For architecture overview, see `ARCHITECTURE.md`.

## ECS Details

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

- **Scene2D** (sprites/tilemaps only): Shadow passes skipped entirely. Minimal LightingUBO. Normal map descriptor writes skipped.
- **Scene2_5D** (sprites + any lights, no 3D meshes): Shadows skipped, but full lighting UBO is populated.
- **Scene3D** (3D meshes present): Full pipeline — all shadow passes, lighting, normal maps.

## Ray Tracing Pipeline

Full Vulkan RT pipeline with hybrid raster+RT rendering. All 19 RT shaders compiled to SPIR-V and embedded in `RTShaderData.h`. RT pipeline activates automatically on supported hardware.

- **`RTCapabilities`** - Extension detection (`Query(VkPhysicalDevice)`) and properties. VulkanContext adds +1000 score for RT-capable GPUs
- **`AccelerationStructureManager`** - BLAS cache by mesh hash (`RegisterMesh()`), per-frame TLAS rebuild (`BuildTLAS()`)
- **`RTPipeline`** - RT pipeline + shader binding table (SBT) construction
- **RT Effects** - `RTShadows` (R16F), `RTReflections` (RGBA16F), `RTAmbientOcclusion` (R16F), `RTGlobalIllumination` (RGBA16F), `PathTracer` (progressive accumulation)
- **`SVGFDenoiser`** - 3-pass compute: temporal accumulation, variance estimation, a-trous wavelet (5 iterations)
- **`OIDNDenoiser`** - Intel OIDN alternative. CMake option `ENJIN_RAYTRACING_OIDN` (OFF by default)
- **`RTCompositor`** - Fullscreen compute shader composites RT layers into scene HDR
- **Integration** - `RenderSystem::InitializeRayTracing()` creates RT descriptor set (14 bindings). Only for Scene3D mode

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

## Sprite Texture Atlas

`SpriteTextureAtlas` auto-packs small sprite textures (<=512px) into a single 4096x4096 GPU texture at runtime using shelf packing. Owned by `RenderSystem` (`m_SpriteAtlas`), wired into `SpriteBatchRenderer` via `SetAtlas()`.

- **Request/Build cycle:** `RenderSprites()` calls `RequestTexture()` per sprite each frame, then `Build()` if dirty.
- **Batch key:** Atlased sprites use `"__atlas__"` as the effective texture key.
- **UV remapping:** Per-instance UVs remapped into atlas region: `uvOut = regionStart + uvIn * regionSize`.
- **Exclusions:** Textures >512px, failed loads, or atlas overflow fall back to individual draw calls.
- **Hot-reload:** `m_SpriteAtlas->Invalidate()` clears regions/exclusions so atlas rebuilds next frame.

## Descriptor Set Caching

Per-entity texture (bindings 3/5/6/8/9) and bone buffer (binding 7) descriptor writes are cached via `m_LastBound` state in `RenderSystem`. Main render loop sorts entities by `MaterialComponent::cachedTextureKey` so identical materials draw consecutively. `m_LastBound.Reset()` at each render pass boundary.

## Editor Details

- **`EditorLayer`** - Main editor class with ImGui panels
- **Default UI sizing:** Body font 17px, heading 23px, monospace 16px. Frame padding 8x5, item spacing 10x7, scrollbar 16px, menu bar height 28px, 4px panel gaps.
- **Unified Settings window:** Single `Settings` window with 3-tab TabBar (System / Project / Scene). `OpenSettings(tab)` opens with programmatic tab selection. `EditorSettings` bit 5 is the canonical visibility bit.
  - **System tab:** Camera, Editor Performance, External IDE, Accessibility, Fonts
  - **Project tab:** Project Mode, Window Icon, Physics, Frame Rate, Audio, Collision Groups, Build Config
  - **Scene tab:** Skybox, Shadows, Ambient Lighting, Cel Shading, Display Options, Ray Tracing, Light Probes, Post Processing, Retro Effects, Environment
- **`PlayMode`** - Play/Pause/Stop. On Stop, scene changes persist; `PlayModeDiff` shows what changed. Camera position is restored on stop.
- **Multi-select:** `m_SelectedEntities` (unordered_set), `m_PrimarySelected` for inspector/gizmo
- **Keyboard shortcuts:** `1/2/3` gizmo modes, `4` local/world, `WASD` fly cam, `Space/E` up, `Q/Ctrl` down, `Shift` sprint, RMB+mouse look, `Delete` delete, `Ctrl+D` duplicate, `F` focus
- **Entity icons:** `GetEntityIcon()` — `[C]` Camera, `[L]` Light, `[M]` Mesh, `[S]` Sprite, `[T]` Tilemap, `[P]` Particle, `[A]` Audio, `[R]` Rigidbody, `[D]` Dialogue, `[V]` Visual Script, `[U]` UI Canvas, `[AI]` AI, `[BT]` Behavior Tree
- **Empty states:** `DrawEmptyState()` helper renders centered icon, heading, body text, and optional CTA button

## UI System

- **`UICanvasComponent`** — ECS component (namespace `Enjin::GUI`). Holds element tree, design resolution, scale mode, theme
- **`UIElement`** — Single UI element with `UIAnchor` layout, `UIStyleOverride`, `UIWidgetData`, `accessibleLabel`
- **`UIWidgetType`**: Panel, Button, Label, Image, ProgressBar, Slider, Checkbox, Toggle
- **`UISystem`** — Layout + render + input + focus navigation. Focus: Tab/Shift+Tab, DPad/Arrow keys, Enter/Space activation
- **`DialogueBoxComponent`** — Auto-builds UICanvas elements for dialogue display

## Effects Systems

- **`WeatherSystem`** - Rain, snow, fog, storm with lightning
- **`Water3D`** - 3D water plane with Gerstner waves
- **`RetroEffects`** - CRT, pixelation, dithering post-processing
- **`ParticleSystem`** - CPU simulation + GPU instanced billboard renderer (up to 16384 particles)
- **`FluidTerrainCoupling`** - Erosion mode + accumulate mode. Files: `FluidTerrainCoupling.h/cpp`
- **`ReactionDiffusion`** - Gray-Scott model, 9 presets. Files: `ReactionDiffusion.h/cpp`
- **`CellularAutomataGeometry`** - 7 CA rules, 3 mesh modes. Files: `CellularAutomataGeometry.h/cpp`
- **`PhysarumSimulation`** - Agent-based slime mold sim. Files: `PhysarumSimulation.h/cpp`
- **`TimelineEditor`** - Flash-style keyframe animation. Files: `TimelineEditor.h/cpp`
- **`FourierMesh`** - DFT decomposition of 2D contours. Files: `FourierMesh.h/cpp`
- **`Projection4D`** - 4D polytope visualization. Files: `Projection4D.h/cpp`

## Assets & Build

- **`GLTFLoader`** / **`AssimpLoader`** - Loads glTF/GLB natively, FBX/OBJ/DAE/3DS via Assimp v5.4.3
- **`SceneImporter`** - `Import()` auto-detects format. `ImportOptions` controls scale, materials, animations, colliders
- **`PrefabManager`** - Create/instantiate/save/load `.enjprefab` files with per-instance overrides
- **`BuildPipeline`** - Full game export: scan → validate → pack `.enjpak` → copy player → manifest
- **Pack format:** `.enjpak` with magic `ENJPAK10`, per-file CRC32, XOR obfuscation (key: `enjin_default_pack_key_2025`)
- **Player app** (`Player/src/main.cpp`) - Standalone executable, loads `game.enjpak`. Auto title screen + pause menu via `GameMenuSystem`

## Steam Audio HRTF + Occlusion

`SteamAudioProcessor` provides physics-based HRTF binaural rendering + occlusion/transmission. CMake option `ENJIN_AUDIO_STEAM_AUDIO` (OFF by default). Steam Audio is a processing layer — miniaudio still handles file I/O, mixing, and device output. Audio chain: mono → `IPLDirectEffect` → `IPLBinauralEffect` → stereo. 2D sounds unaffected.

- **HRTF:** When active, miniaudio spatialization disabled; distance attenuation via `Calculate3DVolume()`. Coordinate conversion: Enjin LH → Steam Audio RH by negating Z.
- **Occlusion:** `BuildScene()` creates IPLScene from collider geometry. `UpdateOcclusion()` raycasts at ~10Hz.
- **Editor:** Audio section in Settings > Project tab with HRTF/Occlusion/Transmission toggles. Persisted in `.enjinproject`.

## Scripting Details

- **AngelScript** via `TegeBehavior` base class with hot-reload
- ~481 bound functions across math, entity, scene, input, physics (2D+3D), audio, components, sprites, coroutines, events, tweening, noise, rendering, post-processing, PP volumes, screen-space effects, input actions, dialogue, save/load, weather, particles, quests, cinematics, object pool, destructibles, UI canvas, localization, prefabs, networking, AI/BT, accessibility, procedural gen, camera presets, Newgrounds, audio event graph, plugins, MIDI input, Flash API shim
- See `docs/SCRIPTING_API.md` for the complete API reference
- **Visual scripting** (Blueprint-style) with 143+ built-in nodes, debugger with breakpoints/step-through

## Current Feature Status

150+ completed features. See `docs/USER_MANUAL.md` for component details, `docs/ROADMAP.md` for planned work.

**Summary:** Vulkan rendering (PBR, CSM shadows, post-processing, RT pipeline, light probes, OIT), 70+ ECS components, ImGui editor (multi-select, undo/redo, 44 templates, marketplace), 2D sprites/tilemaps/atlas, 3D model import (glTF/FBX/OBJ/DAE/PLY/VOX), Jolt 3D + Box2D 2D physics, miniaudio + Steam Audio HRTF, AngelScript (~686 bindings) + visual scripting (143+ nodes), tiered save system, LAN multiplayer (HMAC-SHA256), weather/water/particles/procedural gen, asset pack pipeline + standalone player, Linux/Steam Deck support, comprehensive accessibility (11 themes, colorblind modes, switch access, screen reader).

## Known Performance Issues (All Resolved)

All bottlenecks P0-P6 identified in audits have been resolved. Key fixes:
- `vkDeviceWaitIdle()` → fence-based `WaitForGPU()`
- `GetAllEntities()` → `GetEntitiesWithComponent<T>()`
- Shadow caster caching, descriptor set caching, material sort
- Spatial hash grid broad-phase collision
- Jolt + Box2D integration (5 phases complete)
- `Quaternion::GetRotationZ()` / `GetForward()` helpers for hot paths
- Sprite atlas region caching, pre-hashed texture paths

See `docs/ROADMAP.md` for full history.

## Security Considerations

### Input Validation
- **Scene files (JSON):** Validate array sizes, check `.contains()` before accessing keys.
- **glTF/GLB import:** Clamp loop bounds to allocated buffer size.
- **Asset pack (.enjpak):** Bounds-check all sizes/offsets against file size.

### Script Execution
- AngelScript sandboxed from filesystem/network. 1M instruction limit.
- Script `#include` paths not yet restricted to script directory.

### Asset Pack Obfuscation
- XOR obfuscation is **not cryptographically secure**. CRC32 for integrity only.

### General
- Validate enum casts from deserialized integers. Sanitize file paths. Cap allocation sizes.

## Trust Zone Map

Documented in `.enjin-boundaries.json`. Summary:

| Zone | Risk | Key rule |
|------|------|----------|
| **security-critical** | HIGH | Networking, script engine, asset packer/reader, scene serializer, plugin loader. Validate everything. |
| **trust-boundary** | HIGH | ScriptBindings, SceneSerializer, AssetReader, NetworkSerializer. Validation MUST happen here. |
| **user-api** | MEDIUM | ECS components, ScriptBindings (686+ functions), VS NodeRegistry, InputAction. Additions safe, removals break scripts. |
| **editor-internal** | LOW-MED | EditorLayer, panels, PlayMode. Still validate file paths and JSON. |
| **renderer-internals** | LOW | Vulkan, RayTracing, PostProcessing. Always check VkResult. |
| **gameplay-runtime** | LOW-MED | Physics, audio, AI, save/load. Cap iterations, guard divide-by-zero. |
| **foundation** | LOW | Core math, memory, logging, platform. Widest blast radius. |

## Roadmap

See `docs/ROADMAP.md`. Remaining planned work includes: project hub, drag-and-drop improvements, macOS (MoltenVK), Xbox/PS5/Switch 2/Mobile/VR/WebAssembly platform ports, Git integration, Flash-style authoring.

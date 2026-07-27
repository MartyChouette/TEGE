# CLAUDE_REFERENCE.md - Detailed Subsystem Reference

This file contains detailed subsystem documentation moved from CLAUDE.md to reduce context size.
For quick reference, see `CLAUDE.md`. For architecture overview, see `ARCHITECTURE.md`.

## ECS Details

- **`ECS::World`** - Manages entities and components. Thread-safe: structural ops (Create/Destroy/Add/Remove/Clear) guarded by recursive mutex. `DestroyEntity()` is deferred — queued and flushed at `Update()` start. `DestroyEntityImmediate()` for rare cases needing instant removal. `IsValid()` returns false for pending-destruction entities. `Lock()`/`Unlock()` for external batch operations.
- **`ECS::Entity`** - Just a u64 ID
- **Key Components:**
  - `TransformComponent` - position, rotation (Euler), scale, `visible` bool, cached world matrix with dirty flag
  - `MeshComponent` - vertices (position, normal, UV, color, tangent, boneWeights, boneIndices), indices
  - `MaterialComponent` - PBR properties, textures (base color, normal, height, matcap at binding 18), retro flags, dithered gradient (`ditherGradient`, `ditherGradientBands` 2-8, `ditherGradientPattern` 6 patterns), cel outline (`outlineWidth`/`outlineColor`), procedural surface noise (`surfaceNoiseScale`/`surfaceNoiseStrength`)
  - `LightComponent` - Light data (type, color, intensity, range, attenuation, cone angles, castShadows — no direction field, extract from TransformComponent rotation)
  - `NameComponent` - Entity name string
  - `CameraComponent` - In-game cameras with projection
  - `NotesComponent` - Text annotations (field: `.notes`, not `.text`)
  - `AnimatorComponent` - Skeletal animation playback
  - `CharacterController` - Movement controllers (Platformer2D, TopDown2D/3D, FPS, TPS)
  - `BoxColliderComponent` / `SphereColliderComponent` / `CapsuleColliderComponent` - Colliders with `categoryBits`/`collisionMask` bitmask filtering
  - `SaveDataComponent` - Persistence marker with `PersistenceTier` (SceneState/RunState/MetaProgression), custom tags, and key-value data
  - `SaveLoadMenuComponent` - In-game save/load grid overlay with configurable columns, mode (Save/Load), and slot display
  - `PostProcessVolumeComponent` - Spatial PP blending with Box/Sphere shapes, priority, smoothstep blend radius, selective override mask (24 effect groups incl. 5 screen-space effects at bits 19-23), global volumes
  - `ReflectionProbeComponent` - Box-projected reflections with `boxMin`/`boxMax`, `intensity`, `priority`, `blendDistance`, cubemap baking via editor button (binding 19)
  - `DynamicDifficultyComponent` - Opt-in adaptive difficulty: 6 input metrics (deaths, health, accuracy, time, resources, checkpoint health), 6 output multipliers (enemy damage/health, AI aggression, resource drops, hints, checkpoints), transparent/hidden mode, player-chosen base difficulty with auto-adjustment band

### Collision Filtering

Bitmask system: `categoryBits` (which groups it belongs to) and `collisionMask` (which groups it collides with). Bilateral rule: `(A.categoryBits & B.collisionMask) && (B.categoryBits & A.collisionMask)`. Defaults: `categoryBits = 1`, `collisionMask = 0xFFFFFFFF`. Up to 32 named groups stored in `SceneManager::m_CollisionGroupNames`. Old `layer` field migrated to `categoryBits` in deserialization.

### Physics Backend Abstraction

`IPhysicsBackend` (3D) and `IPhysicsBackend2D` (2D) are abstract interfaces for physics implementations. Shared data types live in `PhysicsTypes.h` (AABB, Ray, RaycastHit, CollisionResult, CollisionEvent, ColliderInfo) and `PhysicsTypes2D.h` (Shape2DType, Body2DComponent, Joint2DComponent, Contact2D, RayHit2D) — always available regardless of which backends are compiled. `JoltBackend` wraps Jolt Physics v5.2.0 with full ECS↔Jolt synchronization, thread-safe contact events, bilateral collision filtering, 6 joint types, gravity zones, and CCD support. `Box2DBackend` wraps Box2D v3.0.0 (C API with handle-based IDs) for production-grade 2D physics — multi-threaded sub-stepping, robust constraint solving, 5 joint types (Revolute/Prismatic/Distance/Rope/Weld), contact+sensor event polling, raycasting, overlap queries, CCD, and bilateral collision filtering. `PhysicsBackendFactory` creates backends via `CreatePhysicsBackend(type, mode)` / `CreatePhysicsBackend2D(type, mode)` with `IsJoltAvailable()` / `IsBox2DAvailable()` helpers and `ResolveBackendName()`. `PhysicsBackendType` enum: `Auto`, `Jolt`, `Box2D`. When `ENJIN_PHYSICS_JOLT=ON` (default), Auto selects Jolt for 3D/Mixed modes. When `ENJIN_PHYSICS_BOX2D=ON` (default), Auto selects Box2D for 2D/Mixed modes. PlayMode and Player own physics via `unique_ptr<IPhysicsBackend>`. All consumers accept `IPhysicsBackend*`. `ControllerSystem` accepts both `IPhysicsBackend*` (3D) and `IPhysicsBackend2D*` (2D) — Platformer2D uses 2D raycasts for ground detection when available, with 3D fallback. CMake options: `ENJIN_PHYSICS_JOLT` (ON), `ENJIN_PHYSICS_BOX2D` (ON).

### Project Mode (2D/3D)

`ProjectMode` enum: `Mode2D`, `Mode3D`, `Mixed`. Stored in `.enjinproject`. Components tagged with `DimensionTag` (Any/Only2D/Only3D) for Add Component filtering. Grid orientation: 2D = XY plane, 3D/Mixed = XZ plane.

### Scene Composition & 2D/3D Pipeline

`ClassifySceneComposition()` runs each frame and classifies the scene as `SceneRenderMode::Scene2D`, `Scene2_5D`, or `Scene3D` based on entity types present. This drives automatic pipeline optimizations:

- **Scene2D** (sprites/tilemaps only): Shadow passes skipped entirely. Minimal LightingUBO. Normal map descriptor writes skipped.
- **Scene2_5D** (sprites + any lights, no 3D meshes): Shadows skipped, but full lighting UBO is populated.
- **Scene3D** (3D meshes present): Full pipeline — all shadow passes, lighting, normal maps.

## Ray Tracing Pipeline

Full Vulkan RT pipeline with hybrid raster+RT rendering. All 25 RT shaders compiled to SPIR-V and embedded in `RTShaderData.h`. RT pipeline activates automatically on supported hardware.

- **`RTCapabilities`** - Extension detection (`Query(VkPhysicalDevice)`) and properties. VulkanContext adds +1000 score for RT-capable GPUs
- **`AccelerationStructureManager`** - BLAS cache by mesh hash (`RegisterMesh()`), per-frame TLAS rebuild (`BuildTLAS()`)
- **`RTPipeline`** - RT pipeline + shader binding table (SBT) construction
- **RT Effects** - `RTShadows` (R16F), `RTReflections` (RGBA16F), `RTAmbientOcclusion` (R16F), `RTGlobalIllumination` (RGBA16F), `PathTracer` (progressive accumulation with NEE, MIS, Russian Roulette, firefly clamping, Cook-Torrance BRDF, GGX importance sampling, simplified material fallback for deep bounces)
- **`SVGFDenoiser`** - 3-pass compute: temporal accumulation, variance estimation, a-trous wavelet (5 iterations)
- **`OIDNDenoiser`** - Intel OIDN alternative. CMake option `ENJIN_RAYTRACING_OIDN` (OFF by default)
- **`OptiXDenoiser`** - NVIDIA OptiX denoiser with CUDA↔Vulkan interop (timeline semaphores, external memory). CMake option `ENJIN_RAYTRACING_OPTIX` (OFF by default)
- **`RTCompositor`** - Fullscreen compute shader composites RT layers into scene HDR
- **Material SSBO** - Per-instance material data (PBR properties, texture flags) uploaded to binding 9 for RT hit shaders, enabling full material evaluation in closest-hit programs
- **Integration** - `RenderSystem::InitializeRayTracing()` creates RT descriptor set (27 bindings). Only for Scene3D mode

**RT Descriptor Set (Set 1)** (27 bindings):
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
Binding 14: STORAGE_IMAGE (RT Translucency output)
Binding 15: STORAGE_IMAGE (RT Caustics output)
Binding 16: STORAGE_BUFFER (NEE lights)
Binding 17: STORAGE_BUFFER (SDF data)
Binding 18: STORAGE_BUFFER (Simplified materials)
Binding 19-20: STORAGE_BUFFER (ReSTIR reservoirs, ping-pong)
Binding 21-23: STORAGE_BUFFER (Screen-space radiance cache)
Binding 24-26: STORAGE_BUFFER (Surfel radiance cache)
```

**25 GLSL Shaders** (`Engine/shaders/`): `rt_common.glsl`, `rt_shadow/reflect/ao/gi/pathtrace .rgen/.rmiss/.rchit`, `rt_translucency/caustics .rgen/.rmiss/.rchit`, `svgf_temporal/variance/atrous.comp`, `rt_composite.comp`, `taa.comp`

## Sprite Texture Atlas

`SpriteTextureAtlas` auto-packs small sprite textures (<=512px) into a single 4096x4096 GPU texture at runtime using shelf packing. Owned by `RenderSystem` (`m_SpriteAtlas`), wired into `SpriteBatchRenderer` via `SetAtlas()`.

- **Request/Build cycle:** `RenderSprites()` calls `RequestTexture()` per sprite each frame, then `Build()` if dirty.
- **Batch key:** Atlased sprites use `"__atlas__"` as the effective texture key.
- **UV remapping:** Per-instance UVs remapped into atlas region: `uvOut = regionStart + uvIn * regionSize`.
- **Exclusions:** Textures >512px, failed loads, or atlas overflow fall back to individual draw calls.
- **Hot-reload:** `m_SpriteAtlas->Invalidate()` clears regions/exclusions so atlas rebuilds next frame.

## Descriptor Set Caching

Per-entity texture (bindings 3/5/6/8/9/18) and bone buffer (binding 7) descriptor writes are cached via `m_LastBound` state in `RenderSystem`. Main render loop sorts entities by `MaterialComponent::cachedTextureKey` so identical materials draw consecutively. `m_LastBound.Reset()` at each render pass boundary. Binding 18 is the matcap texture for material-expression art style.

## CPU-Side Performance Optimizations (P0 Sprint — 2026-03-17)

- **Binary search keyframes:** `Animation.cpp::SampleKeyframes()` uses `std::upper_bound` for O(log N) lookups instead of O(N) linear scan
- **Integer sprite sort keys:** `SpriteBatchRenderer` sorts by pre-hashed `usize` keys instead of `std::string` comparison
- **Cached component storage pointers:** `World::GetComponentStorage<T>()` public API added. `RenderSystem` caches 5 hot storage pointers (Transform, Mesh, Material, Animator, Text), replacing 38 `GetComponent()` calls with direct `storage->Get()`
- **Light entity list dirty flag:** `m_LightListDirty` gates rebuild on structural changes, with O(1) size-mismatch recovery
- **Pre-allocated sprite shadow vectors:** `SpriteBatchRenderer` reuses member vectors for shadow instances, eliminating per-frame heap allocations
- **Cached world matrices:** `TransformComponent` mutable `worldMatrixDirty` flag with recursive parent caching, O(1) for unchanged transforms across shadow/outline/main passes
- **Batched material SSBO:** Binding 2 changed from UBO to `STORAGE_BUFFER_DYNAMIC`, single per-frame upload with dynamic offset per draw
- **ECS View template:** `ECS::View<Components...>` variadic template (`View.h`) for cache-friendly filtered entity iteration with smallest-set optimization and `Exclude()` filter
- **SIMD math:** SSE/SSE4.1 intrinsics for Matrix4 multiply, Vector3/4 dot/cross, Quaternion::ToMatrix (`Simd.h`). `constexpr` preserved via `std::is_constant_evaluated()`
- **Slim MaterialComponent:** Fields reordered — PBR core in cache line 0, flags/artistic in line 1, cold string paths at tail
- **Delta sprite sorting:** Persistent sorted list with FNV-1a hash fingerprint, skip sort when unchanged, `std::stable_sort` when dirty
- **Multi-draw indirect:** Non-textured pool entities via single `vkCmdDrawIndexedIndirectCount`; textured pool entities grouped by texture hash via `IndirectDrawBatcher`
- **Async compute:** `AsyncComputeScheduler` overlaps RT dispatch with rasterization on dedicated compute queue, timeline semaphores

## Vulkan Raster Pipeline Layout

**Descriptor Bindings (Set 0):** 0=ViewProj UBO, 1=Lighting UBO, 2=Material SSBO (dynamic offset, batched per-frame), 3=Base color tex, 4=Shadow map array, 5=Height map, 6=Normal map, 7=Bone SSBO, 8=Metallic-roughness tex, 9=Emissive tex, 10=Point shadow cubemaps, 11=Spot shadow maps, 12=Shadow data SSBO, 13=Object data SSBO, 14=Cluster grid SSBO (clustered lighting), 15=Cluster light index SSBO (clustered lighting), 16=VT indirection tex, 17=VT physical atlas, 18=Matcap tex, 19=Baked reflection probe cubemap

**Push Constants (128 bytes):** model matrix (64B), baseColor+metallic, emissiveColor+roughness, emissiveStrength, opacity, alphaCutoff, flags (bitfield), parallaxScale, surfaceParam1 (water: shoreWidth / artistic: reflectivity), surfaceParam2 (water: foamIntensity / artistic: fresnelPower), surfaceParam3 (water: foamScale / artistic: rimLightStrength)

**Flags bitfield:** bits 0-2 render, 3 skinned, 4 wind, 5-7 water, 8-9 alpha mode, 10 height tex, 11 ocean, 12 UV quantize, 13 gouraud, 14-15 shadow dither, 16-19 texture flags, 20-23 retro flags, 24-28 snap resolution (/8), 29-31 shadow dither pattern

**CMake feature options:** `ENJIN_CLUSTERED_LIGHTING` (ON), `ENJIN_VRS` (OFF), `ENJIN_VIRTUAL_TEXTURING` (OFF), `ENJIN_VISIBILITY_BUFFER` (OFF)

## WebGPU Backend

- **WGSL shaders** in `Engine/shaders/wgsl/`: pbr.wgsl (Cook-Torrance), shadow.wgsl, postprocess.wgsl (ACES), triangle.wgsl, web_pbr.wgsl (simplified). Embedded copies in `WebShaderData.h`
- **Shadow mapping**: 1-cascade directional shadow via `BeginDepthOnlyPass()` (depth-only render pass, `Depth32Float`). `WebGPUPipelineManager::CreateRenderPipeline` allows null fragment shader when `hasColorAttachment=false`. PBR shader samples shadow map via `sampler_comparison` with 4-tap PCF
- **Instanced draw batching**: Identical meshes batched into single instanced draw calls. `ObjectData` uploaded as storage buffer array for per-instance transforms/materials
- **Web player**: `Player/src/web_main.cpp` uses RenderSystem directly through IRenderBackend. Responsive canvas via ResizeObserver, real delta time, extern C callbacks for JS interop

## Renderer Advanced Features

- **Reflection probes:** `ReflectionProbeSystem` finds nearest probe per frame, box-projected cubemap reflections in `triangle.frag`. Cubemap baking renders 6 faces via `RenderToTarget`, stores as `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT` image at binding 19
- **ReSTIR:** 3-pass importance-weighted light selection (initial candidates → temporal reuse → spatial reuse). Ping-pong reservoir buffers at RT bindings 19-20, M capping, Jacobian correction, Hammersley neighbor sampling
- **Temporal RT reuse:** Per-buffer history ping-pong for shadow/AO/reflection/GI, confidence-weighted reprojection with disocclusion detection, dispatched between RT effects and denoiser
- **Radiance cache:** Screen-space tiled cache (32x32 pixels), depth/normal validity, directional light excluded, progressive refinement via hysteresis, stale mask bitfield, RT bindings 21-23
- **Upscaling:** IUpscaler interface with FSR 2 (built-in Lanczos+CAS), DLSS 3.5 (stub), XeSS (stub). 4 quality modes (50-77% render scale)
- **SMAA:** Single-pass edge-walking AA in postprocess.frag, 12-step progressive walk with sub-pixel smoothing, mode 3 in AA dropdown

## Editor Details

- **`EditorLayer`** - Main editor class with ImGui panels
- **Default UI sizing:** Body font 17px, heading 23px, monospace 16px. Frame padding 8x5, item spacing 10x7, scrollbar 16px, menu bar height 28px, 4px panel gaps.
- **Unified Settings window:** Single `Settings` window with 3-tab TabBar (System / Project / Scene). `OpenSettings(tab)` opens with programmatic tab selection. `EditorSettings` bit 5 is the canonical visibility bit.
  - **System tab:** Camera, Editor Performance, External IDE, Accessibility, Fonts
  - **Project tab:** Project Mode, Window Icon, Physics, Frame Rate, Audio, Collision Groups, Build Config
  - **Scene tab:** Skybox, Shadows, Ambient Lighting, Cel Shading, Display Options, Ray Tracing, Light Probes, Post Processing, Retro Effects, Environment
- **`PlayMode`** - Play/Pause/Stop. On Stop, scene changes persist; `PlayModeDiff` shows what changed. Camera position is restored on stop.
- **Multi-select:** `m_SelectedEntities` (unordered_set), `m_PrimarySelected` for inspector/gizmo
- **Keyboard shortcuts:** `1/2/3` gizmo modes, `4` local/world, `WASD` fly cam, `Space/E` up, `Q/Ctrl` down, `Shift` sprint, RMB+mouse look, `Delete` delete, `Ctrl+D` duplicate, `F` focus, `F1` Game Debug panel, `F2` Debug Workstation panel, `` ` `` (backtick) drop-down console, `F11` focus mode
- **Game Debug Panel (F1):** `DrawGameDebugPanel()` — tabbed panel (Scene/Physics/Scripts/Audio/Gameplay). Scene tab lists all entities with click-to-select. Physics tab shows backend type, collider counts, body details. Scripts tab shows per-entity script status with error indicators. Audio tab shows source list with play status and channel info. Gameplay tab shows tweens, particles, health, interactables. `m_ShowGameDebug` toggle.
- **Debug Workstation (F2):** `DrawDebugWorkstation()` — tabbed panel (Performance/Renderer/ECS/Scene/System). Performance tab has color-coded FPS, frame time percentiles (P50/P95/P99), frame time graph, render stats (draw calls, triangles, descriptor cache hit rate), memory (process/system/GPU VRAM). Renderer tab shows scene mode, shadows, RT state, AA/upscaler, wireframe/fog/culling, render target sizes, post-processing. ECS tab lists component counts for 12+ types. Scene tab shows scene/project info, physics backend, play mode. System tab shows engine/ImGui version, GPU, Vulkan API version, driver version, swapchain, HDR, window/display sizes, build config, platform. `m_ShowDebugWorkstation` toggle.
- **Drop-down Console (`` ` ``):** `DrawDropConsole(deltaTime)` — Quake-style console sliding from top of screen (40% height). `m_ShowDropConsole` toggle, `m_DropConsoleAnim` (0-1 lerp at 8x speed). Features: auto-focus input, command history (Up/Down), color-coded output (error=red, warn=yellow, input=green), backtick character filter. Shares `m_ConsoleLog` with Console panel. `ExecuteConsoleCommand()` handles 60+ commands across 10 categories (general, entities, transform, rendering, retro, scene, components, materials, lights, camera, query, bulk, debug).
- **Console commands:** `help`, `clear`, `stats`, `fps`, `version`, `list`, `select <id>`, `deselect`, `create <name>`, `delete`, `inspect`, `getpos`, `pos/rot/scale`, `wireframe`, `shadows`, `fog <density>`, `ambient <r g b>`, `culling`, `hdr`, `flatshading`, `vertexsnap`, `affine`, `gouraud`, `stipple`, `save/load <path>`, `play/stop/pause`, `addcomp/removecomp <type>`, `setname/setnotes`, `visible`, `components`, `setcolor/setemissive/setmetallic/setroughness/setopacity`, `lightcolor/lightintensity/lighttype/lightrange`, `fov/near/far`, `find <name>`, `count <type>`, `children`, `parent`, `selectall`, `hideall/showall`, `deleteall confirm`, `colliders`, `grid`, `rain`, `snow <intensity>`, `shadowres/shadowdist`, `ambient_intensity`, `curvature`. Addable component types: mesh, material, light, camera, script, audio, rigidbody, name, notes, sprite, particle, tween, lod.
- **PrepareRenderTargets:** `PrepareRenderTargets()` runs before command buffer recording. Resizes editor viewport and game view render targets with 8-pixel threshold to avoid thrashing. Handles focus mode (full display resolution), scene render target, post-processing pipeline updates, and effect pipeline recreation. Prevents crashes with Vulkan hooks (OBS, RenderDoc) that hold resource references during command buffer recording. Fixes 4:3 aspect ratio crash.
- **Entity icons:** `GetEntityIcon()` — `[C]` Camera, `[L]` Light, `[M]` Mesh, `[S]` Sprite, `[T]` Tilemap, `[P]` Particle, `[A]` Audio, `[R]` Rigidbody, `[D]` Dialogue, `[V]` Visual Script, `[U]` UI Canvas, `[AI]` AI, `[BT]` Behavior Tree
- **Empty states:** `DrawEmptyState()` helper renders centered icon, heading, body text, and optional CTA button

## Animation

- **BlendTree** on `AnimatorComponent`: 1D parameter-driven blending. Set `blendTree.enabled = true`, nodes with thresholds, `SetBlendParameter()` at runtime
- **Animation events:** `SkeletalAnimation::events` (vector of `AnimEvent` with `time` + `name`)
- **Retargeting:** `RetargetAnimation()` + `BuildAutoRetargetMap()` — auto-maps bone names, strips Mixamo `mixamorig:` prefix
- **Shadow shader has skinning:** Bone SSBO sampling in shadow vertex shader for skinned mesh shadow correctness
- **Editor calls skeletal animator update directly** (not via `RenderSystem::Update`) to decouple animation timing from rendering
- **PoseLibraryComponent:** Save/recall named bone poses (expressions, gestures). Per-bone rotations with blend weight. Inspector has large buttons grouped by category
- **BoneRegion auto-detection:** `ClassifyBoneRegion()` detects 12 body regions from bone names (Face, Head, LeftHand, RightHand, LeftArm, RightArm, Spine, LeftLeg, RightLeg, LeftFoot, RightFoot, Tail). Rig Regions panel shows filterable bone list per region

## Gameplay Systems

- **RecordRewindSystem:** Per-entity (Braid-style, `RecordRewindComponent`) and scene-wide (Sands of Time-style, `SceneRewindComponent`) time rewind. `StateRingBuffer<T>` for O(1) push/eviction. Delta-compressed `DeltaFrame` snapshots with configurable keyframe interval. 6 channel flags (Transform/Velocity/Health/Animation/Physics/Material). Physics state sync via `ForceSetBodyState()` on Jolt/Box2D. API: `StartEntityRewind()`, `StopEntityRewind()`, `SeekSceneToTime()`. 11 AngelScript bindings (`Rewind_*`)
- **DialogueTree narrative integration:** 3 node types — `QuestAction` (start/complete/fail quest), `PlayCinematic` (trigger cinematic entity), `SetGameFlag` (persistent key-value flag via TieredSaveSystem). `DialogueCondition::Source` enum — `Variable`/`QuestStatus`/`GameFlag`. `DialoguePlayer` has `ActionCallback` + `ConditionResolver` for decoupled cross-system dispatch. Wired in PlayMode and Player

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
- **`WindSystem`** - Global wind with gusts + turbulence. Drives vegetation sway, weather particles, water waves. Zone overrides via `WeatherZoneComponent`. Heat source feedback from fire. `GetWindAt(pos)` for CPU query, `GetWindVector()` packed vec4 for GPU (xyz=dir*strength, w=time)
- **`TreeRenderer`** - GPU-instanced trees (trunk + canopy quads). Seasonal color blending, quadratic trunk bend, auto-collider generation. Shader: `tree.vert/frag`
- **`GrassRenderer`** - GPU-instanced grass blades (7-vertex tapered strip). `height²` wind curve, player step bending, world curvature. Shader: `grass.vert/frag`
- **`ShrubRenderer`** - GPU-instanced shrubs (3 intersecting quads). `height^1.5` wind curve (stiffer than grass). Shader: `shrub.vert/frag`
- **`VegetationComponent`** - Tag for mesh entities. Enables `FLAG_WIND_SWAY` (bit 4) in vertex shader. Uses vertex color red channel as sway weight (0=trunk, 1=tips)
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

## Audio System

**Backend:** miniaudio (`ma_engine`, `ma_sound`) for file I/O, mixing, and device output. Supports WAV, MP3, FLAC, OGG.

**Bus Hierarchy:** `AudioMixer` manages Master→SFX/Music/UI/Voice buses with per-bus VU metering, 3-band EQ. Snapshot system (Dialogue/Pause/Combat/Cutscene) adjusts all bus volumes simultaneously. `MusicCrossfader` with 3 transition modes (linear, equal-power, S-curve).

**AudioReactiveSystem** — processes 14 subsystems per frame:
- AudioReactive (drive visual properties from amplitude)
- ThresholdTriggers (fire events on level crossing)
- RTPC (real-time parameter control, Wwise-style)
- BeatClock + BeatSync (metronome + rhythm sync)
- Conductor (AI-driven dynamic music stems)
- AudioCollision (TOTK-style material interaction audio with distance culling)
- Sidechain (per-bus volume ducking)
- Occlusion (raycast-based low-pass + volume reduction)
- ReverbZones (spatial reverb with blend radius)
- AmbientLayers (positional ambient soundscapes)
- MusicZones (spatial music regions with crossfade)
- LipSync (amplitude → morph target weights)
- MIDIBindings (CC/note/pitchbend → entity properties)

**MaterialInteractionTableComponent** — defines collision sounds per material pair (10 surface types). Symmetric lookup, per-interaction soft/hard/scrape clips with pitch offset and volume multiplier. Inspector shows coverage matrix grid.

**AudioFidelityComponent** — 8 sound chip emulation presets (Modern, LoFi, Retro16Bit, Retro8Bit, ChipTune, FMSynth, PSOne, Cassette). Auto-matches ArtStyleComponent when enabled. Per-parameter tweaking (sample rate, bit depth, low-pass, noise, wobble, saturation, stereo width).

**MIDI Input:** `MIDIInput` class (WinMM on Windows, stubs elsewhere). Device enumeration, open/close, double-buffered event polling, persistent CC state. 12 AngelScript bindings.

### Steam Audio HRTF + Occlusion

`SteamAudioProcessor` provides physics-based HRTF binaural rendering + occlusion/transmission. CMake option `ENJIN_AUDIO_STEAM_AUDIO` (OFF by default). Steam Audio is a processing layer — miniaudio still handles file I/O, mixing, and device output. Audio chain: mono → `IPLDirectEffect` → `IPLBinauralEffect` → stereo. 2D sounds unaffected.

- **HRTF:** When active, miniaudio spatialization disabled; distance attenuation via `Calculate3DVolume()`. Coordinate conversion: Enjin LH → Steam Audio RH by negating Z.
- **Occlusion:** `BuildScene()` creates IPLScene from collider geometry. `UpdateOcclusion()` raycasts at ~10Hz.
- **Editor:** Audio section in Settings > Project tab with HRTF/Occlusion/Transmission toggles. Persisted in `.enjinproject`.

## Scripting Details

- **AngelScript** via `TegeBehavior` base class with hot-reload. The base class + enjin_api scripts (Timer/Tween/Math/StateMachine) are embedded in the engine and auto-injected — scripts need no includes; regenerate the embeds with `python _gen_api.py` after editing `enjin_api/*.as`
- ~1000+ bound functions across math, entity, scene, input, physics (2D+3D), audio, components, sprites, coroutines, events, tweening, noise, rendering, post-processing, PP volumes, screen-space effects, input actions, dialogue, save/load, weather, particles, quests, cinematics, object pool, destructibles, UI canvas, localization, prefabs, networking, AI/BT, accessibility, procedural gen, camera presets, Newgrounds, audio event graph, plugins, MIDI input, Flash API shim
- See `docs/SCRIPTING_API.md` for the complete API reference
- **Visual scripting** (Blueprint-style) with 262 built-in nodes, debugger with breakpoints/step-through

## Current Feature Status

210+ completed features. See `docs/USER_MANUAL.md` for component details, `docs/ROADMAP.md` for planned work.

**Summary:** Vulkan rendering (PBR, CSM shadows, post-processing, RT pipeline with path tracer NEE/MIS/Russian Roulette/firefly clamping, motion vectors, TAA, light probes, OIT), 80+ ECS components, ImGui editor (multi-select, undo/redo, 15 curated templates (more in-code, disabled pending QA), marketplace, F1 Game Debug panel, F2 Debug Workstation, Quake-style drop-down console with 60+ commands, settings conflict detection), 2D sprites/tilemaps/atlas, 3D model import (glTF/FBX/OBJ/DAE/PLY/VOX) with validation/undo, Jolt 3D + Box2D 2D physics, miniaudio + Steam Audio HRTF + audio bus hierarchy + 14 audio subsystems (reactive, RTPC, beat sync, conductor, TOTK collision, sidechain, reverb zones, occlusion, lip sync, MIDI bindings, audio fidelity), morph targets/blend shapes (GPU SSBO pipeline), AngelScript (~1,010 bindings) + visual scripting (262 nodes), record & rewind system, tiered save system, LAN multiplayer (HMAC-SHA256), weather/water/particles/procedural gen, asset pack pipeline + standalone player, Linux/Steam Deck support, comprehensive accessibility (11 themes, colorblind modes, switch access, screen reader, gamepad inspector).

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
- **Scene files (JSON):** Validate array sizes, check `.contains()` before accessing keys. Vertex cap (10M), index cap (10M), entity cap, string caps via `SafeStr()`.
- **glTF/GLB import:** Clamp loop bounds to allocated buffer size. **Known issue:** `cgltf.h:1288` has unchecked `strcpy()` in path concatenation (third-party, upstream fix pending).
- **Asset pack (.enjpak):** Bounds-check all sizes/offsets against file size. Path traversal rejected. CRC32 integrity.

### Script Execution
- AngelScript sandboxed from filesystem/network. 1M instruction limit.
- Script `#include` paths not yet restricted to script directory.

### Asset Pack Obfuscation
- XOR obfuscation is **not cryptographically secure**. CRC32 for integrity only.

### Thread Safety (updated 2026-04-12)
- `ComponentRegistry::s_NextComponentId` is `std::atomic` — safe for concurrent component type registration.
- `MiniaudioBackend::m_Channels` protected by `m_ChannelMutex` — safe for concurrent Play/Stop/Update.
- **Known open issues:** `World::GetComponent()` lock-free reads, `NameCache` rebuild not locked, `m_DeviceLost` non-atomic, global visual script pointers unsynchronized. See `docs/AUDIT_2026_04_12.md` for full list.

### Process Execution
- No `std::system()` calls remain in the codebase. All external process launches use `ShellExecuteA` (Windows), `fork`/`execlp` (macOS/Linux), or `CreateProcessA`.

### General
- Validate enum casts from deserialized integers. Sanitize file paths. Cap allocation sizes.

## Trust Zone Map

Documented in `.enjin-boundaries.json`. Summary:

| Zone | Risk | Key rule |
|------|------|----------|
| **security-critical** | HIGH | Networking, script engine, asset packer/reader, scene serializer, plugin loader. Validate everything. |
| **trust-boundary** | HIGH | ScriptBindings, SceneSerializer, AssetReader, NetworkSerializer. Validation MUST happen here. |
| **user-api** | MEDIUM | ECS components, ScriptBindings (900+ functions), VS NodeRegistry, InputAction. Additions safe, removals break scripts. |
| **editor-internal** | LOW-MED | EditorLayer, panels, PlayMode. Still validate file paths and JSON. |
| **renderer-internals** | LOW | Vulkan, RayTracing, PostProcessing. Always check VkResult. |
| **gameplay-runtime** | LOW-MED | Physics, audio, AI, save/load. Cap iterations, guard divide-by-zero. |
| **foundation** | LOW | Core math, memory, logging, platform. Widest blast radius. |

## Roadmap

See `docs/ROADMAP.md`. Remaining planned work includes: project hub, drag-and-drop improvements, macOS (MoltenVK), Xbox/PS5/Switch 2/Mobile/VR/WebAssembly platform ports, Git integration, Flash-style authoring.

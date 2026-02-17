# Enjin Engine Technical Roadmap

This document captures detailed technical plans, performance findings, and strategic initiatives identified through codebase audits. It complements CLAUDE.md's feature roadmap with implementation-specific details.

## Status Summary (2026-02-16)

**150+ features complete.** The remaining work is platform ports, infrastructure polish, and QA.

### Still Remaining

| Category | Item | Priority |
|----------|------|----------|
| **Platforms** | macOS (MoltenVK) | P2 |
| **Platforms** | WebAssembly/WebGPU export | P1 (blocks Flash web distribution) |
| **Platforms** | Xbox Series X/S (GDK/D3D12) | P2 |
| **Platforms** | PlayStation 5 (PSDK/AGC) | P3 |
| **Platforms** | Nintendo Switch 2 (Vulkan 1.3) | P3 |
| **Platforms** | Mobile (Android/iOS) | P4 |
| **Platforms** | VR/XR (OpenXR) | P4 |
| **Editor** | Settings UX restructure (System/Project/Scene tiers) | P2 |
| **Editor** | Networking config editor UI | P3 |
| **RT** | OptiX AI Denoiser integration (NVIDIA) | P4 |
| **RT** | RT Caustics (photon mapping / path traced) | P4 |
| **RT** | RT Translucency (subsurface scattering) | P4 |
| **Flash** | Yarn Spinner / Twine dialogue import/export | P4 |
| **QA** | Comprehensive testing across all 150+ features | P1 |
| **Known Bug** | RT pipeline crash on pool-allocated entity BLAS builds (parked) | P2 |

---


## Networking Config & Settings UX (2026-02-16)

**Change Summary**
- Added external JSON config for networking and security defaults: `config/network_settings.json`.
- Added load/save helpers on `NetworkConfig` and `NetworkSystem` so defaults can be overridden without recompiling.
- Added rate-limit + abuse protection knobs (burst limits, violation window, ban/kick behavior).

**User Editability**
- User-editable: Yes.
- How: Edit `config/network_settings.json` (loaded at host/join). No editor UI yet.
- Safe defaults ship in repo; values are runtime-tunable for different games/traffic profiles.

**Needs Restructure (Settings UX)**
- Consolidate settings into three tiers with clear ownership:
  - System Settings (editor/app-level, machine or installation scope)
  - Project Settings (game-specific, shared in repo)
  - Scene Settings (per-scene overrides, local to scene file)
- Networking config should live under Project Settings with optional Scene overrides.

## Performance Optimization Findings

### Critical Rendering Pipeline Issues

These issues cause frame hitches and should be addressed first.

#### 1. ~~GPU Synchronization Blocking~~ ✅ RESOLVED

Replaced `vkDeviceWaitIdle()` with `VulkanContext::WaitForGPU()` — fence-based wait registered by VulkanRenderer (waits on in-flight fences only, fast graphics-queue-only). 18 subsystems migrated. Falls back to `vkDeviceWaitIdle` only during device teardown. ECS World thread safety added: recursive mutex on structural ops, deferred entity destruction queue flushed at Update() start.

#### 2. ~~Entity Iteration Inefficiency~~ ✅ RESOLVED

Replaced all `GetAllEntities()` + filter patterns with `GetEntitiesWithComponent<T>()` across render, shadow, and flower systems.

#### 3. ~~Per-Entity Texture Lookups~~ ✅ RESOLVED

Added `cachedBaseColorTexture`, `cachedHeightTexture`, `cachedNormalTexture`, `cachedMetallicRoughnessTexture`, `cachedEmissiveTexture` on `MaterialComponent`. Cache invalidated via `InvalidateTextureCache()` on path changes.

#### 4. ~~Play Mode Double-Draw~~ ✅ RESOLVED

During play mode, `m_SkipMainPassRendering` flag bypasses all main swapchain geometry/effects. The sorted render list is now built before the skip check so the offscreen path reuses it (previously fell back to unsorted entity list with zero descriptor cache hits). Pipeline bind, descriptor set bind, viewport/scissor commands moved outside the per-entity loop in `RenderToTarget()`.

#### 5. ~~Frame Jitter on Windows~~ ✅ RESOLVED

Added `timeBeginPeriod(1)` on Windows startup for 1ms sleep resolution (default 15.6ms was causing 5-14ms variance in `sleep_for()`). Increased frame limiter spin margin from 1ms to 2ms. Linked `winmm.lib` via CMake.

### Medium Priority Optimizations

#### 5. ~~Redundant Component Lookups~~ ✅ RESOLVED

Added `GetEntitiesWithComponents<T1, T2>()` multi-component query to World. Replaced `HasComponent() + GetComponent()` double lookups with single `GetComponent()` + null check in ControllerSystem (all 6 controller loops + FollowTarget + LookAtTarget) and SimplePhysics. Extracted `GetColliderInfo()` helper in SimplePhysics to replace repeated 3-way Box/Sphere/Capsule discrimination across 6+ locations.

#### 6. ~~String-Based Entity Lookups in Scripts~~ ✅ RESOLVED

Added `m_NameCache` (`unordered_map<string, Entity>`) to World with lazy rebuild on `FindEntityByName()`. Cache invalidated on entity destroy, world clear, and name changes. `Scene_FindEntity()` now O(1) via `FindEntityByName()`.

#### 7. ~~Vector Allocations Without Reserve~~ ✅ RESOLVED

Added `reserve()` calls before all particle spawn loops in FlowerSystem: `SpawnBreakParticles()`, `SpawnGroundSplash()`, `SpawnTensionDrip()`.

### Physics & Scripting Optimization Pass ✅ RESOLVED (2026-02-11)

#### 8. ~~Physics O(N²) Collision Detection~~ ✅ RESOLVED

Added `SpatialHashGrid` broad-phase to `DetectCollisionEvents()`. Entities inserted into hash cells (FNV-1a, 4m cell size), only entities sharing adjacent cells are tested. 100 colliders: 4,950 brute-force pairs → typically <200 candidate pairs.

#### 9. ~~Physics GetAllEntities() in Hot Paths~~ ✅ RESOLVED

Added `RebuildColliderCache()` — builds deduplicated list of all Box/Sphere/Capsule collider entities once per `Update()`. Replaced 6 `GetAllEntities()` scans (ground check, Raycast, RaycastAll, MoveAndSlide, GetCollidersInRadius, OverlapBox) with `m_CachedColliderEntities`. Scenes with 500 entities but 50 colliders now check 50 instead of 500.

#### 10. ~~Physics Gravity Zone Query Per Rigidbody~~ ✅ RESOLVED

`GetEntitiesWithComponent<GravityZoneComponent>()` hoisted outside the rigidbody loop — 1 query per frame instead of N (one per rigidbody).

#### 11. ~~ScriptSystem Redundant Entity Queries~~ ✅ RESOLVED

Merged 6 `GetEntitiesWithComponent<ScriptComponent>()` calls (4 in Update + 1 FixedUpdate + 1 LateUpdate) into a single per-frame cached query (`m_CachedScriptEntities`). Init + OnStart merged into one pass.

#### 12. ~~EditorLayer GetAllEntities() for Player Lookup~~ ✅ RESOLVED

Cached `m_CachedPlayerEntity` for CharacterController zone detection. Full entity scan only runs on cache miss (play mode start, entity destroyed). Re-scan avoided during steady-state play.

#### 13. ~~LevelStreaming O(N) Duplicate Checks~~ ✅ RESOLVED

Replaced linear scan in load/unload queues with `unordered_set` for O(1) duplicate detection. Pre-built `unordered_map` priority lookup before sorting (sort comparator O(1) instead of O(M) per comparison).

#### 14. ~~World::IsValid() O(N) Pending Destruction Scan~~ ✅ RESOLVED

`IsValid()` and `IsPendingDestruction()` scanned the `m_PendingDestructions` vector linearly for every call. Added companion `m_PendingDestructionSet` (unordered_set) for O(1) lookup. `DestroyEntity()` duplicate check also upgraded from O(N) to O(1).

### Data Structure Improvements

| Current | Recommended | Location | Status |
|---------|-------------|----------|--------|
| ~~`std::map<string, string>`~~ | ~~`std::unordered_map`~~ | ~~DialogueTree.h:106, Gameplay.h:883~~ | ✅ RESOLVED |
| ~~`unordered_map<Entity, RenderData>`~~ | ~~Dense vector indexed by entity~~ | ~~RenderSystem.h~~ | ✅ RESOLVED — `std::vector<EntityRenderData>` indexed by entity ID with `valid` flag, O(1) direct lookup |
| ~~String-keyed texture cache~~ | ~~Integer-keyed or pointer cache~~ | ~~RenderSystem.h~~ | ✅ RESOLVED — `m_TexturePathToId` (path→u32 ID, only on first load) + `m_TextureById` (dense vector), eliminates per-frame string hashing |

### Physics: Jolt + Box2D Integration

**Context:** Stress test (2026-02-11) showed SimplePhysics hits 18ms at 1000 colliders (single-threaded, no sleep, no CCD). Rather than incrementally optimizing SimplePhysics, replace it entirely with battle-tested libraries: **Jolt Physics** (3D, MIT) and **Box2D v3** (2D, MIT). SimplePhysics is retired — Jolt handles 3D+mixed, Box2D handles pure 2D. Both are permissive-licensed, zero royalty, full Enjin ownership retained.

#### Phase 1: IPhysicsBackend Interface + CMake Setup ✅ COMPLETE

- `IPhysicsBackend` abstract 3D interface: `SetWorld`, `Update`, `SetGravity`/`GetGravity`, `Raycast`, `RaycastAll`, `CheckGround`, `GetCollidersInRadius`, `OverlapBox`, `MoveAndSlide`, `CheckAABBCollision`, `CheckSphereCollision`, `GetPendingCollisionEvents`, `ClearPendingCollisionEvents`, `GetConstraintSolver`, `GetName`
- `IPhysicsBackend2D` abstract 2D interface: `Initialize`, `Update`, `Shutdown`, `SetGravity`/`GetGravity` (Vector2), `Raycast`, `RaycastAll` (2D), `OverlapCircle`, `OverlapBox` (2D), collision callbacks (Enter/Exit/SensorEnter/SensorExit), `SetCCDEnabled`, `SetVelocityIterations`, `SetPositionIterations`, `GetName`
- `SimplePhysicsBackend` / `SimplePhysicsBackend2D` — adapter wrappers delegating 1:1 to existing SimplePhysics / PhysicsWorld2D
- `PhysicsBackendFactory` — `CreatePhysicsBackend(type, mode)` / `CreatePhysicsBackend2D(type, mode)` (currently always returns SimplePhysics adapters)
- `PhysicsBackendType` enum: `Auto`, `Jolt`, `Box2D`
- All consumers rewired: PlayMode (`unique_ptr<IPhysicsBackend>` + `unique_ptr<IPhysicsBackend2D>`), ControllerSystem, ScriptBindings, VisualScriptExecutor, NodeDefinition (ExecutionContext), NodeRegistry, EditorLayer, Player app
- Fixed orphaned `SetBindingsPhysics()` — now properly wired in PlayMode::Play()/Stop()
- CMake: `ENJIN_PHYSICS_JOLT` (FetchContent Jolt v5.2.0) and `ENJIN_PHYSICS_BOX2D` (FetchContent Box2D v3.0.0), both OFF by default
- **Files:** `IPhysicsBackend.h`, `IPhysicsBackend2D.h`, `PhysicsBackendType.h`, `SimplePhysicsBackend.h/.cpp`, `SimplePhysicsBackend2D.h/.cpp`, `PhysicsBackendFactory.h/.cpp`, plus 9 modified consumer files

#### Phase 2: Jolt Backend (3D) ✅ COMPLETE

- `JoltBackend : IPhysicsBackend` — full Jolt v5.2.0 wrapper (1273 lines)
- `JoltContactListener` — thread-safe contact buffer + bilateral collision filtering in `OnContactValidate`
- Body creation: `BoxColliderComponent` → `BoxShape`, `SphereColliderComponent` → `SphereShape`, `CapsuleColliderComponent` → `CapsuleShape` (with X/Z rotation via `RotatedTranslatedShape`)
- Collider center offset via `RotatedTranslatedShape`, scale applied at creation, min size clamped to 0.01
- `RigidbodyComponent` mapping: `BodyType` → `EMotionType`, mass override, linear/angular damping, gravity factor, DOF freezing via `mAllowedDOFs`, CCD via `mMotionQuality::LinearCast`, initial velocity
- Entity ID stored in `mUserData` for O(1) reverse lookup from Jolt body to ECS entity
- Jolt `JobSystemThreadPool` (hardware_concurrency - 1 threads), 16 MB `TempAllocatorImpl`
- ECS↔Jolt sync: full per-frame reconciliation (create new bodies, destroy removed, update kinematic via `MoveKinematic`, static position sync)
- Jolt → ECS writeback: position, rotation, velocity, angular velocity, sleep state, ground check (short downward raycast)
- Collision filtering: bilateral rule in `JoltContactListener::OnContactValidate` using per-body `categoryBits`/`collisionMask` (Jolt's `ObjectLayerPairFilter` only sees ObjectLayers 0/1, not BodyIDs — filtering must happen in contact listener)
- Broad phase: 2 layers (NonMoving/Moving), all-pass `ObjectVsBroadPhaseLayerFilter` and `ObjectLayerPairFilter`
- Collision events: Enter/Exit detection via previous/current pair tracking (same pattern as SimplePhysics)
- Raycasting: `NarrowPhaseQuery::CastRay` with `EnjinBodyFilter` for layer mask, surface normal via `BodyLockRead` + `GetWorldSpaceSurfaceNormal`. `RaycastAll` via `AllHitCollisionCollector`
- MoveAndSlide: iterative AABB slide resolution (3 iterations, same API as SimplePhysics)
- Joint mapping (body locking via `BodyLockWrite` for constraint creation):
  - `DistanceJointComponent` → `DistanceConstraint` (spring settings for stiffness < 1)
  - `HingeJointComponent` → `HingeConstraint` (limits, velocity motor)
  - `BallSocketJointComponent` → `PointConstraint` + optional `ConeConstraint`
  - `SpringJointComponent` → `DistanceConstraint` with spring stiffness/damping
  - `FixedJointComponent` → `FixedConstraint` (auto-detect point)
  - `SliderJointComponent` → `SliderConstraint` (axis, limits, velocity motor)
- Breakable joints: stress checked per frame, constraint destroyed if > breakForce
- Gravity zones: per-body `SetGravityFactor(0)` + `AddForce(customGravity * gravityScale * mass)`, highest-priority zone wins
- Sleep: automatic via Jolt (reported as `rb->isSleeping = !bodyInterface.IsActive()`)
- Factory: `PhysicsBackendFactory` returns `JoltBackend` when `ENJIN_PHYSICS_JOLT=ON` and (type==Jolt or Auto with 3D/Mixed mode)
- **Files:** `JoltBackend.h`, `JoltContactListener.h`, `JoltBackend.cpp`, modified `PhysicsBackendFactory.cpp`

#### Phase 3: Box2D Backend (2D) ✅ DONE

- `Box2DBackend : IPhysicsBackend2D` wrapping Box2D v3.0.0 C API (~755 lines)
- Handle-based IDs (`b2WorldId`, `b2BodyId`, `b2ShapeId`, `b2JointId`), not C++ classes
- Full ECS↔Box2D synchronization (create/destroy/update bodies per frame)
- Map ECS 2D components to Box2D bodies:
  - `Body2DComponent` (static/dynamic) → `b2BodyDef` + `b2ShapeDef`
  - Circle/Box/Polygon shapes → `b2CreateCircleShape` / `b2CreatePolygonShape`
  - Physics materials (friction, restitution, density) via direct `b2ShapeDef` fields
  - Collision filtering: `b2Filter` with `categoryBits`/`maskBits` (`uint32_t`)
- Contact + sensor events via polling: `b2World_GetContactEvents()` / `b2World_GetSensorEvents()`
- Raycasting: `b2World_CastRayClosest()` for single, `b2World_CastRay()` with C callback for all
- Overlap queries: `b2World_OverlapAABB()` with C callback for circle/box
- 5 joint types: Revolute, Prismatic, Distance, Rope (Distance+limit), Weld — with limits, motors, spring settings
- CCD via `isBullet` flag on `b2BodyDef`
- Sub-step count (default 4) replaces old velocity/position iterations
- Factory: `PhysicsBackendFactory` returns `Box2DBackend` when `ENJIN_PHYSICS_BOX2D=ON` and (type==Box2D or Auto with 2D/Mixed mode)
- **Files:** `Box2DBackend.h`, `Box2DBackend.cpp`, modified `PhysicsBackendFactory.cpp`

#### Phase 4: Enable Production Backends ✅ COMPLETE

- ~~PlayMode: swap `m_Physics` from `SimplePhysics` to `IPhysicsBackend*`~~ ✅ done (Phase 1)
- ~~ControllerSystem: `SetPhysics(IPhysicsBackend*)` + `SetPhysics2D(IPhysicsBackend2D*)`~~ ✅ done (Phase 1 + Phase 6) — Platformer2D uses 2D raycasts for ground detection, 2D templates switched to Body2DComponent
- ~~ScriptBindings: `SetBindingsPhysics(IPhysicsBackend*)`~~ ✅ done (Phase 1)
- ~~VisualScriptExecutor: `SetPhysics(IPhysicsBackend*)`~~ ✅ done (Phase 1)
- ~~Player app: `unique_ptr<IPhysicsBackend>` via factory~~ ✅ done (Phase 1)
- ~~EditorLayer: Physics debug draw~~ ✅ done (collider wireframes + joint lines via View > Show Colliders toggle)
- ~~Project Settings UI: Physics Backend dropdown (Auto / Jolt / Box2D / Simple)~~ ✅ done — 4 options with availability indicators and resolved name display
- ~~Jolt/Box2D ON by default in CMake~~ ✅ done
- ~~PhysicsTypes.h / PhysicsTypes2D.h extracted~~ ✅ done — interfaces no longer depend on SimplePhysics
- ~~`Simple = 3` added to `PhysicsBackendType`~~ ✅ done
- ~~Factory handles Simple type, dimension mismatch warnings, fallback logging~~ ✅ done
- ~~`IsJoltAvailable()` / `IsBox2DAvailable()` / `IsSimpleAvailable()` helpers~~ ✅ done
- ~~`ResolveBackendName()` for UI display~~ ✅ done

#### Phase 5: Retire SimplePhysics Behind Compile Guard ✅ COMPLETE

- ~~`ENJIN_PHYSICS_SIMPLE` CMake option (ON by default)~~ ✅ done
- ~~All SimplePhysics files guarded: `SimplePhysics.h/.cpp`, `SimplePhysicsBackend.h/.cpp`, `SimplePhysicsBackend2D.h/.cpp`, `ConstraintSolver.h/.cpp`, `Physics2D.h/.cpp`, `PhysicsWorld.h/.cpp`~~ ✅ done
- ~~Factory fallback paths guarded~~ ✅ done — returns `nullptr` + error log when SIMPLE=OFF
- ~~Null-safety audit~~ ✅ done — all physics pointer dereferences already guarded
- ~~StressTest guarded~~ ✅ done — physics benchmarks skipped when SIMPLE=OFF
- ~~Build verified: Jolt=ON Box2D=ON Simple=ON (all targets) and Simple=OFF (all targets)~~ ✅ done

---

## Node Graph Expansion Plan

The existing `NodeGraphEditor` framework is production-ready and currently powers only the Animation Graph. It should be expanded to power multiple systems.

### Framework Capabilities (Already Implemented)

- 12 typed pins (Flow, Bool, Float, Int, String, Vector2, Vector3, Vector4, Quaternion, Color, Entity, Any)
- Wong colorblind-safe pin colors
- Drag-to-connect with Bezier curve visualization
- Pan/zoom canvas with minimap
- Node/link selection, box select, keyboard nav (Tab, Delete, Escape)
- Type-safe link validation callbacks
- Custom node body rendering
- Right-click context menus with categories
- Full JSON serialization
- Node flags (NoDelete, NoMove, Highlighted, Error)

### Expansion Priority

#### Phase 1: Dialogue Tree Editor ✅ COMPLETE

**Status:** Fully implemented with 7 node types, DialoguePlayer state machine, Dialogue Editor panel (View > Tools), EntityEventBus integration, SubtitleSystem accessibility, 10 AngelScript bindings, typewriter effect, choice navigation, and full serialization.

#### Phase 2: Visual Scripting / Blueprints - Phase 1 ✅ COMPLETE

**Status:** Foundation implemented with:
- VisualScriptComponent (graph data, variables, event mappings, node metadata)
- NodeDefinition system with ExecutionContext, PinDefinition, NodeCategory
- NodeRegistry singleton with 15 built-in nodes (OnStart, OnUpdate, Branch, Sequence, Get/Set Variable, Add, Subtract, Multiply, GetPosition, SetPosition, Print, Greater, Less, Not, GetSelf)
- VisualScriptExecutor with flow execution, pure node caching, input resolution
- VisualScriptSystem ECS integration with Initialize/Shutdown/Update lifecycle
- VisualScriptEditor panel (View > Tools) with entity sidebar, node graph canvas, variable editor, node inspector
- Full scene serialization

**Phase 2 (Complete):**
- Extended flow control nodes (ForLoop, While, Delay, Sequence, Gate)
- Math/logic nodes (Add, Subtract, Multiply, Divide, Greater, Less, Equal, And, Or, Not, Negate, Clamp, Lerp, Normalize, DotProduct, CrossProduct, Distance, RandomFloat, RandomInt)
- Transform nodes (GetPosition, SetPosition, GetRotation, SetRotation, GetScale, SetScale, Translate, Rotate, LookAt)
- Component access nodes (GetHealth, SetHealth, Damage, PlayAudio, StopAudio, PlayAnimation)
- Node palette with fuzzy search and recently-used tracking
- Pin value display on nodes
- Undo/redo for node/link creation and deletion

**Phase 3 (Complete):**
- Collision callbacks (OnCollisionEnter, OnCollisionExit, OnTriggerEnter, OnTriggerExit)
- Latent Delay node with execution resume
- Execution visualization (highlighted currently-executing node)

**Phase 4 (Complete):**
- Breakpoints and step-through debugging (F5 continue, F10 step, F9 toggle)
- Execution timeline profiler with color-coded node bars
- WaitForAudioComplete latent node
- WaitForAnimationComplete latent node
- Multi-select copy/paste with preserved internal links
- Box/marquee selection and Ctrl+click multi-select
- Multi-node drag
- Undo/redo for node property edits and variable value edits

**Phase 5+ (Complete):**
- Physics nodes: Raycast, SphereCheck, BoxCheck, AddForce, AddImpulse, SetVelocity, GetVelocity, SetGravityScale + Math_Negate, Component_Has, Debug_PrintWarning/Error
- Subgraph/function nodes: Function_Entry, Function_Return, Function_Call (call depth 32, Functions panel in VS editor)
- AngelScript interop: Script_Call node (call AS functions from VS), reverse bindings (VisualScript_SendEvent/SetVariable/GetVariable from AS)
- Conditional breakpoints (BreakpointInfo with condition string, hit count, Shift+F9 to edit)
- Watch window for variable inspection (panel when paused)
- Call stack view (CallStackEntry push/pop in ExecuteFlow, panel when paused)

#### Phase 3: AI Behavior Tree Editor ✅ COMPLETE

**Status:** Fully implemented with 20 node types (BTNodeType/BTStatus/BTNodeMeta/Blackboard), BehaviorTreeComponent, BehaviorTreeExecutor (tick dispatch by category), BehaviorTreeSystem (ISystem with tickInterval), and color-coded node editor with context menu, auto-layout, blackboard editor, and play-mode status visualization. EditorPanel `1<<19`.

#### Phase 4: Quest Flow Editor ✅ COMPLETE

**Status:** Fully implemented with QuestFlowComponent, visual quest graph editor, node types for quest start, objectives (sequential/parallel), conditions, rewards, branches, and end states. EditorPanel `1<<20`.

### Future Graph Systems ✅ COMPLETE

| System | Effort | Use Case | Status |
|--------|--------|----------|--------|
| Shader Graph | 6-8 weeks | Visual shader authoring, GLSL generation | ✅ Complete (58 node types incl. SceneColor/SceneNormal/SceneDepth/StaticSwitch, type mismatch validation, topological sort GLSL codegen, .enjshader save/load, editor wired) |
| Audio Event Graph | 2-3 weeks | Dynamic audio mixing based on game state | ✅ Complete (runtime execution, trigger events, parameter thresholds, delay scheduling, .enjaudiopkg, 4 AS + 3 VS bindings) |
| Particle System Graph | 2-3 weeks | Sub-emitter chains, complex particle systems | ✅ Complete (compiler to ParticleEmitterComponent, emitter/modifier/control/renderer mapping, .enjparticle, apply-to-entity) |
| Procedural Generation Graph | 4-6 weeks | Visual WFC/L-system/BSP rule composition | ✅ Complete (node-based pipeline editor with 38 node types, graph execution, preview) |

---

## GUI Modernization Plan ✅ COMPLETE

### Brand Color Palette

Replace current blue accent with TEGE brand sage green for distinct identity.

| Role | Hex | RGB | Use |
|------|-----|-----|-----|
| Primary Accent | #C7DAC4 | 199, 218, 196 | Selection, active tabs, buttons, gizmos |
| Secondary Accent | #5B7FA1 | 91, 127, 161 | Links, info icons, secondary buttons |
| Success | #6DB876 | 109, 184, 118 | Validation, play button |
| Warning | #D4A855 | 212, 168, 85 | Cautions |
| Error | #D6726B | 214, 114, 107 | Deletions, errors |
| Background | #0E0E11 | 14, 14, 17 | Keep current |
| Secondary BG | #0B0B0E | 11, 11, 14 | Slightly darker for depth |

### Typography System

| Level | Size | Weight | Color | Use |
|-------|------|--------|-------|-----|
| H1 | 23px | Bold (700) | White | Panel headers |
| H2 | 18px | SemiBold (600) | Light gray | Section titles |
| Body | 17px | Regular (400) | Medium gray | Content |
| Small | 14px | Regular (400) | Dim gray | Labels, hints |
| Mono | 16px | Regular (400) | Soft cyan | Code, IDs |

### Spacing Scale (8px grid)

| Token | Pixels | Use |
|-------|--------|-----|
| XS | 4 | Icon-label gap |
| S | 8 | Tight grouping |
| M | 12 | Default item spacing |
| L | 16 | Section boundaries |
| XL | 24 | Major panel dividers |
| XXL | 32 | Empty state spacing |

### Micro-Interactions ✅ COMPLETE

**Spring Easing:** `cubic-bezier(0.175, 0.885, 0.32, 1.275)` for button press bounce — implemented as `SpringEase()` utility

**Transitions:**
- Hover: 100ms smooth fade — `SmoothFade()` utility with speed parameter
- State change: 200ms spring — `AdvanceSpring()` with configurable stiffness/damping/mass
- Focus: 150ms with glow shadow

**Implementation in ImGui (ImGuiLayer.h):**
- `SpringState` + `SpringParams` + `AdvanceSpring()` — per-frame spring physics simulation
- `ColorTransition` — RGBA color interpolation over frames for hover/focus transitions
- `HoverAnimation` — per-widget tracker with hover alpha fade + press bounce spring
- `UIAnimationState` singleton — global widget animation state manager with ID-keyed hover anims
- `SmoothFade()` / `SpringEase()` inline utilities

### Empty States

Design pattern for panels with no content:
- Centered icon (64x64px, outlined, 40% opacity)
- Heading: "No [Items]"
- Body: "Create one to get started"
- Optional CTA button

### Implementation Phases

1. **Foundation (1-2 weeks):** ✅ Color tokens, font hierarchy, spacing constants
2. **Core Panels (2-3 weeks):** ✅ Hierarchy, Inspector, Viewport styling
3. **Micro-interactions (2 weeks):** ✅ Spring easing, hover effects, color transitions
4. **Polish (1-2 weeks):** ~~Empty states~~ ✅ (DrawEmptyState helper with centered icon, heading, body, CTA button; applied to Hierarchy, Inspector, Asset Browser, Scene List, Dialogue, Plugin Browser, Console, Network, and all "No world loaded" panels), ~~notification toasts~~ ✅ (4 types: Info/Success/Warning/Error, slide-in animation, fade-out on expiry, wired to save/build/template/scene-load/model-import/component-remove events), ~~accent color presets~~ ✅ (6 harmony presets, auto-derive 11 colors), ~~theme preview~~ ✅ (250x160 live preview pane), ~~keyboard shortcuts help~~ ✅ (Ctrl+Shift+/, searchable modal, 5 categories), loading indicators, ~~tooltips~~ ✅ (50+ inspector tooltips across Transform/Material/Light/Camera/Rigidbody/Collider), ~~hierarchy search~~ ✅ (case-insensitive text filter), ~~delete confirmation~~ ✅ (modal dialog for entity deletion), ~~LOD warning~~ ✅ (disabled state warning when LOD system inactive)

---

## Creative Intelligence Features

Ideas for "simple creation of complex games":

### Smart Defaults

- When adding "Chase Player" AI node, auto-suggest connecting to "Player Entity" pin
- When creating dialogue, auto-populate speaker name from entity name
- When adding physics joint, auto-suggest connected entity from selection

### One-Click Patterns

- "Make this enemy a patroller" button wires up behavior tree automatically
- "Add basic movement" creates controller + camera + input bindings
- "Setup 2D platformer" configures gravity, camera bounds, collision layers

### Cross-System Integration

- Dialogue tree nodes can auto-create quest objectives
- Behavior tree states can trigger animation graph states
- Quest completion can trigger cinematic camera sequences

### Template Marketplace ✅ COMPLETE

`TemplateMarketplace` class (`TemplateMarketplace.h/cpp`) with bundled catalog of 15 curated templates across 5 categories (Starter, Genre, Systems, Retro, Advanced). `MarketplaceEntry` struct with id, name, description, category, author, version, license, projectMode, tags, accentColor, downloadCount, rating, ratingCount, fileSizeBytes, quality. Features: multi-field fuzzy search, category filter chips, sort by name/rating/downloads, install/uninstall to `templates/` directory. Full marketplace UI panel with accent-colored cards and metadata display. Uses `IsOpen()/SetOpen()` pattern. Menu: View > Tools > Template Marketplace.

---

## Feature Accessibility Audit — Walled-Off Systems

Comprehensive audit (2026-02-10) of all engine features to identify systems that are built but unreachable by game developers. **31 gaps found** across 12 categories — **all 31 resolved** ✅

### CRITICAL — Features Built But Broken/Unreachable

| # | System | Issue | Fix |
|---|--------|-------|-----|
| 1 | **Player App — Physics** | ~~Player doesn't create/update PhysicsWorld or PhysicsWorld2D~~ ✅ Fixed | Player now uses `IPhysicsBackend` via `PhysicsBackendFactory` |
| 2 | ~~**Player App — Weather/Water**~~ | ~~No WeatherSystem or Water3D init in Player~~ ✅ Fixed | WeatherSystem initialized, updated, wired to script bindings. Water3D blocked on Vulkan render pass integration (no water shader pipeline yet) |
| 3 | ~~**Player App — Particles**~~ | ~~ParticleSystem not created in Player~~ ✅ Fixed | Player now creates and updates ParticleSystem |
| 4 | ~~**Player App — Post-Processing**~~ | ~~No bloom/vignette/FXAA/film grain/color grading/retro effects~~ ✅ Wired | PostProcessing initialized from swapchain, wired to script bindings |
| 5 | ~~**Player App — Save System**~~ | ~~TieredSaveSystem not initialized in Player~~ ✅ Fixed | Default LocalSaveBackend, LoadMeta/SaveMeta, Update, script bindings, VS extern all wired |
| 6 | ~~**Build Pipeline — Asset Packing**~~ | ~~BuildPipeline does NOT pack .as scripts, .enjdlg dialogue, .enjprefab, DataAssets, or audio files~~ ✅ Fixed | All asset types packed (.as, .wav/mp3/ogg/flac, .enjdlg, .enjprefab, .enjdata/.enjschema, .csv, .gltf/.glb/.fbx/.obj, .svg, .png/.jpg/.jpeg/.bmp/.tga/.hdr). Scene scanning fixed: removed broken "components" wrapper check, fixed sprite2D/scriptComponent key names, added tilemap/tree/shrub texture scanning |
| 7 | ~~**Script Engine in Player**~~ | ~~ScriptEngine subsystem pointers (physics, audio, scene manager, etc.) never wired in Player~~ ✅ Fixed | All SetXxx() calls wired after system creation |
| 8 | ~~**UI System — No Script Bindings**~~ | ~~UICanvas/UISystem have zero AngelScript or Visual Script bindings~~ ✅ Fixed | UI canvas bindings + focus management + VS nodes added |
| 9 | ~~**Level Streaming**~~ | ~~Fully implemented but no trigger volumes, no editor UI, no script bindings~~ ✅ Fixed | StreamingVolume/Portal serialization, inspector UI, 6 AS + 6 VS bindings |
| 10 | ~~**Localization System**~~ | ~~LocalizationManager complete but no editor panel, no runtime script bindings~~ ✅ Fixed | 5 AS bindings + 1 VS node added |
| 11 | ~~**Newgrounds Bindings**~~ | ~~RegisterNewgroundsBindings() fully written but never called~~ ✅ Fixed | Called in RegisterAllBindings() |

### HIGH — Systems Partially Walled Off (No Script Bindings)

| # | System | Missing |
|---|--------|---------|
| ~~12~~ | ~~Weather~~ | ~~No AS/VS bindings~~ ✅ 6 AS + 2 VS bindings added |
| ~~13~~ | ~~Water~~ | ~~No AS/VS bindings~~ ✅ 6 VS nodes added (SetStyle/WaveHeight/WaveSpeed/GetWaveHeight/Opacity/Color) |
| ~~14~~ | ~~Particles~~ | ~~No AS/VS bindings~~ ✅ 10 AS + 3 VS bindings added |
| ~~15~~ | ~~Networking~~ | ~~No AS/VS bindings~~ ✅ 20 AS + 6 VS bindings added |
| ~~16~~ | ~~Quest System~~ | ~~No AS/VS bindings~~ ✅ 4 AS + 3 VS bindings added |
| ~~17~~ | ~~HUD System~~ | ~~No AS/VS bindings~~ ✅ 2 VS nodes added (SetEnabled/IsEnabled) |
| ~~18~~ | ~~Cinematic System~~ | ~~No AS/VS bindings~~ ✅ 2 AS + 2 VS bindings added |
| ~~19~~ | ~~Destructible System~~ | ~~No AS/VS bindings~~ ✅ 1 AS + 1 VS binding added |
| ~~20~~ | ~~Physics 2D~~ | ~~No AS/VS bindings~~ ✅ 15 AS + 5 VS bindings added |
| ~~21~~ | ~~Prefab System~~ | ~~No AS/VS bindings~~ ✅ 4 AS + 1 VS binding added |
| ~~22~~ | ~~Procedural Generation~~ | ~~No AS/VS bindings~~ ✅ ~15 AS + 9 VS bindings added |

### MEDIUM — Missing Editor Exposure

| # | System | Issue |
|---|--------|-------|
| ~~23~~ | ~~IKComponent~~ | ~~Not in Add Component menu~~ ✅ Added |
| ~~24~~ | ~~TerrainComponent~~ | ~~Not in Add Component menu~~ ✅ Added (Terrain + Terrain2D) |
| ~~25~~ | ~~SkeletonComponent~~ | ~~Not in Add Component menu~~ ✅ Added |
| ~~26~~ | ~~FlowerComponent~~ | ~~Not in Add Component menu~~ ✅ Added (FlowerStem + FlowerPetal) |
| ~~27~~ | ~~Shader Graph~~ | ~~Editor shell exists but generates no actual shader code~~ ✅ Full GLSL code generation with topological sort, 54 node types |
| ~~28~~ | ~~Audio Event Graph~~ | ~~Editor shell exists but doesn't connect to AudioSystem~~ ✅ Full runtime execution via SimpleAudio, 4 AS + 3 VS bindings |
| ~~29~~ | ~~Particle Graph~~ | ~~Editor shell exists but doesn't connect to ParticleSystem~~ ✅ Full compiler to ParticleEmitterComponent |
| ~~30~~ | ~~Animation Graph~~ | ~~Editor shell exists but doesn't drive AnimatorComponent~~ ✅ Dual-mode editor: AnimatorComponent (clip dropdown, speed, blend/exit time, ASM parameters) + StateMachineComponent (game logic SM) |

### LOW

| # | System | Issue |
|---|--------|-------|
| ~~31~~ | ~~Visual Script Nodes~~ | ~~No nodes for weather/water/networking/quest/HUD/cinematic/destructible/physics2D/prefabs/procedural gen~~ ✅ Complete — 126+ nodes across all categories including Gameplay, Physics 2D, Networking, AI/BT, Accessibility, Noise, Streaming, Water, HUD, Procedural Gen |

---

## Implementation Priority Matrix

| Task | Impact | Effort | Priority | Status |
|------|--------|--------|----------|--------|
| Replace GetAllEntities() loops | High | Low | P0 | ✅ Complete |
| Cache texture pointers on materials | High | Medium | P0 | ✅ Complete |
| Replace vkDeviceWaitIdle() with fences | Critical | Medium | P0 | ✅ Complete |
| GPU frustum culling | High | Medium | P0 | ✅ Complete |
| Shadow pipeline overhaul (CSM) | High | Medium | P0 | ✅ Complete |
| Per-entity shadow dither mode | Medium | Low | P1 | ✅ Complete |
| receiveShadows flag wiring | Low | Low | P1 | ✅ Complete |
| Dialogue Tree Editor | Very High | Medium | P1 | ✅ Complete |
| Visual Scripting (Phases 1-4) | Very High | High | P1 | ✅ Complete |
| Project Hub enhancements | Medium | Medium | P1 | ✅ Complete |
| Skybox rendering fixes | Medium | Low | P1 | ✅ Complete |
| Skeleton/Animator serialization | High | High | P1 | ✅ Complete |
| Visual Scripting (Phase 5+) | High | Medium | P1 | ✅ Complete |
| Soft shadows (Poisson disk PCF) | Medium | Medium | P1 | ✅ Complete |
| Sprite batching by texture atlas | High | Medium | P1 | ✅ Complete |
| Point/spot light shadows | Medium | High | P1 | ✅ Complete |
| 2D sprite art pipeline | High | High | P1 | ✅ Complete |
| AI Behavior Tree Editor | High | Medium | P2 | ✅ Complete |
| Quest Flow Editor | High | Low | P2 | ✅ Complete |
| GUI color palette update | Medium | Low | P2 | ✅ Complete |
| Typography system | Medium | Low | P2 | ✅ Complete |
| Multi-threaded command buffer recording | High | High | P2 | ✅ Complete |
| Indirect rendering + GPU culling fix | High | High | P1 | ✅ Complete |
| Async compute queue | High | Medium | P2 | ✅ Complete |
| Hi-Z occlusion culling | High | High | P2 | ✅ Complete |
| Frame graph resource scheduling | Medium | High | P2 | ✅ Complete |
| Merged geometry buffer | High | Medium | P1 | ✅ Complete |
| Undo/redo for inspector property edits | Medium | Medium | P2 | ✅ Complete |
| Customizable accent colors | Medium | Low | P2 | ✅ Complete |
| Asset browser with thumbnails | Medium | Medium | P2 | ✅ Complete |
| Micro-interactions | Medium | Medium | P3 | ✅ Complete |
| Basic networking (LAN multiplayer) | High | High | P2 | ✅ Complete |
| RT Shadows | High | High | P2 | ✅ Complete |
| RT Reflections + AO | High | High | P2 | ✅ Complete |
| RT Global Illumination | High | High | P2 | ✅ Complete |
| Path tracing mode | Medium | High | P3 | ✅ Complete |
| SVGF denoiser | Medium | Medium | P2 | ✅ Complete |
| RT Compositor | Medium | Medium | P2 | ✅ Complete |
| Scene & Entity Locking | Medium | Medium | P2 | ✅ Complete |
| Motor Accessibility | Medium | Low | P2 | ✅ Complete |
| Keyboard Navigation | Medium | Low | P2 | ✅ Complete |
| Procedural Generation Graph | High | Medium | P2 | ✅ Complete |
| Alternative Input Devices | Medium | Low | P2 | ✅ Complete |
| Command Palette | Medium | Low | P2 | ✅ Complete |
| Audio Visual Indicators | Medium | Low | P2 | ✅ Complete |
| Screen Reader Announcer | Medium | Low | P2 | ✅ Complete |
| Scene Lock UI Enhancements | Medium | Low | P2 | ✅ Complete |
| Panel Reorganization | Medium | Medium | P2 | ✅ Complete |
| **— Runtime Accessibility —** | | | | |
| UICanvas Keyboard/Gamepad Navigation | High | Medium | P1 | ✅ Complete |
| In-Game Accessibility Menu Template | High | Medium | P1 | ✅ Complete |
| Focus Indicators for UICanvas | Medium | Low | P1 | ✅ Complete |
| Wire AlternativeInput to Player/UISystem | Medium | Medium | P2 | ✅ Complete |
| Wire Announcer to UISystem (screen reader) | Medium | Medium | P2 | ✅ Complete |
| Accessible Labels on UIElement | Medium | Low | P2 | ✅ Complete |
| High Contrast UI Theme | Medium | Low | P2 | ✅ Complete |
| Player Font Scaling | Medium | Low | P2 | ✅ Complete |
| Reduced Motion for UI | Medium | Low | P2 | ✅ Complete |
| Dyslexia-Friendly Font/Spacing | Low | Low | P3 | ✅ Complete |
| Colorblind-Safe UI Palettes | Low | Low | P3 | ✅ Complete |
| One-Button / Switch Access for UICanvas | Low | Medium | P3 | ✅ Complete |
| Security & Robustness Audit | High | Low | P1 | ✅ Complete |
| **— Feature Accessibility (Walled-Off Systems) —** | | | | |
| Player App: Wire physics/particles/weather/post-process/save | Critical | High | P0 | ✅ Complete (physics via IPhysicsBackend) |
| Build Pipeline: Pack scripts/audio/dialogue/prefabs/data assets | Critical | Medium | P0 | ✅ Complete |
| Wire ScriptEngine subsystem pointers in Player | Critical | Medium | P0 | ✅ Complete |
| Register Newgrounds script bindings (dead code) | High | Low | P0 | ✅ Complete |
| Add UI System script bindings (AS + VS) | Critical | High | P1 | ✅ Complete |
| Add Weather/Water/Particles script bindings | High | Medium | P1 | ✅ Complete |
| Add Physics 2D script bindings | High | Medium | P1 | ✅ Complete |
| Add Quest/HUD/Cinematic/Destructible script bindings | High | Medium | P1 | ✅ Complete |
| Add Networking script bindings | High | High | P1 | ✅ Complete |
| Add Prefab script bindings | Medium | Medium | P2 | ✅ Complete |
| Add Procedural Gen script bindings | Medium | Medium | P2 | ✅ Complete |
| Wire Level Streaming (trigger volumes + editor + bindings) | High | High | P1 | ✅ Complete |
| Wire Localization (editor panel + script bindings) | High | Medium | P1 | ✅ Complete |
| Add IK/Terrain/Skeleton/Flower to Add Component menu | Medium | Low | P2 | ✅ Complete |
| Add missing Visual Script nodes (mirrors binding gaps) | Medium | Medium | P2 | ✅ Complete (22 Gameplay + 6 Noise + 6 Streaming nodes) |
| Connect graph editor shells (shader/audio/particle/anim) | Low | Very High | P3 | ✅ Complete (shader codegen, audio runtime, particle compiler) |
| Default 60 FPS Frame Rate Limit | Low | Low | P1 | ✅ Complete |
| Tiered Save System (20 slots, 3 tiers) | High | High | P1 | ✅ Complete |
| Play Mode Diff Dialog | High | Medium | P1 | ✅ Complete |
| Save/Load UI (Editor + In-Game) | Medium | Medium | P2 | ✅ Complete |
| Cloud Save Backends (NG/Steam) | Medium | Medium | P2 | ✅ Complete |
| Save System Script Bindings (AS + VS) | Medium | Medium | P2 | ✅ Complete |
| Wire AISystem into PlayMode and Player | Critical | Low | P0 | ✅ Complete (patrol/chase/flee/wander/navmesh — was 615+ lines dead code) |
| Add AS bindings for 22 gameplay component types | High | High | P1 | ✅ Complete (~180 new bindings, total ~570) |
| Wire plugin/audio graph/input action bindings in PlayMode | Medium | Low | P1 | ✅ Complete |
| Pack graph assets in BuildPipeline | Medium | Low | P2 | ✅ Complete (.enjshader/.enjaudiopkg/.enjparticle) |
| Quaternion GetRotationZ/GetForward/GetRight/GetUp helpers | High | Low | P1 | ✅ Complete (eliminates ToEuler/ToMatrix in 19+ hot paths) |
| OIDN integration | Medium | Medium | P3 | ✅ Complete (OIDNDenoiser as SVGF alternative, CMake ENJIN_RAYTRACING_OIDN, editor denoiser type selector) |
| **— Rendering & Camera —** | | | | |
| Camera presets (iso, side-scroller, etc.) | Medium | Low | P2 | ✅ Complete |
| Tilt-shift / miniature effect | Low | Medium | P3 | ✅ Complete |
| Bokeh depth of field | Medium | High | P2 | ✅ Complete |
| Cel shading / toon rendering | High | Medium | P2 | ✅ Complete |
| Full-screen stippling & dither | Medium | Low | P3 | ✅ Complete |
| **— Artistic Rendering —** | | | | |
| Parallax occlusion mapping (advanced) | Medium | Medium | P2 | ✅ Complete |
| Flat-shaded low-poly with dithered gradients | Medium | Medium | P3 | ✅ Complete (ditherGradient on MaterialComponent, 2-8 bands, 6 dither patterns, surfaceParam1 push constant) |
| Metaball / blob rendering | Medium | High | P3 | ✅ Done |
| Spherical harmonics lighting | High | High | P2 | ✅ Complete |
| Beam tracing and cone tracing (VXGI) | High | Very High | P3 | ✅ Done |
| SDF ray marching | High | High | P2 | ✅ Complete |
| SDF rendering (3D vector art) | Medium | High | P3 | ✅ Done |
| Order-independent transparency (depth peeling) | Medium | Medium | P2 | ✅ Complete |
| Framebuffer feedback effects | Medium | Low | P3 | ✅ Done |
| Screen-space distortion as primary aesthetic | Medium | Medium | P3 | ✅ Done |
| IK-driven mesh deformation | Medium | High | P3 | ✅ Done |
| Fractal terrain & L-system vegetation (advanced) | High | High | P2 | ✅ Complete (fBm terrain with ridged multifractal, hydraulic/thermal erosion, 3D L-system turtle with stochastic rules) |
| **— Simulation-Driven Geometry —** | | | | |
| ~~Reaction-diffusion on meshes~~ | Medium | High | P3 | ✅ Done |
| ~~Cellular automata as geometry~~ | Medium | Medium | P3 | ✅ Done |
| ~~Slime mold simulation (Physarum)~~ | Medium | Medium | P3 | ✅ Done |
| Fluid simulation as terrain | Medium | Medium | P2 | ✅ Complete (FluidTerrainCoupling: erosion + accumulate modes, bidirectional terrain↔fluid) |
| Voronoi fracture with persistent physics | High | Medium | P2 | ✅ Complete |
| **— Simulation & Flow —** | | | | |
| Curl noise flow fields | High | Medium | P2 | ✅ Complete |
| Wave Racer 64 water | Medium | High | P3 | ✅ Done |
| Mesh audio reactivity via FFT | Medium | Medium | P3 | ✅ Done |
| **— Mathematical & Exotic Geometry —** | | | | |
| Fourier transform meshes | Low | Medium | P4 | ✅ Done |
| Non-Euclidean geometry rendering | High | Very High | P3 | ✅ Done |
| Stereographic projection of 4D objects | Low | Medium | P4 | ✅ Done |
| **— Inverse & Advanced Rendering —** | | | | |
| Inverse / differentiable rendering | Medium | Very High | P4 | ✅ Done |
| **— Asset Libraries —** | | | | |
| Font library (30-50 OFL fonts) | High | Low | P2 | ✅ Complete (42 OFL/Apache fonts, 8 categories, FontLibrary.h/cpp, editor browser with search/category/install) |
| 3D asset library (CC0) | High | High | P2 | ✅ Complete (16 CC0 3D model packs — Kenney/Quaternius, AssetLibrary.h/cpp, editor browser) |
| 2D asset library (CC0) | High | Medium | P2 | ✅ Complete (15 CC0 2D sprite/tileset/UI packs, 14 categories, editor browser) |
| **— Editor & Project —** | | | | |
| Template rebuild & demo scenes | Medium | Medium | P2 | ✅ Complete (38→22→44 templates, all polished +50%) |
| Project Hub redesign (landing + wizard) | Medium | High | P2 | ✅ Complete |
| Template creator tool | Medium | Medium | P3 | ✅ Complete (TemplateCreator.h/cpp, save/load/scan/delete, View > Tools > Template Creator, templates/ directory) |
| Template marketplace | Medium | Medium | P3 | ✅ Complete (TemplateMarketplace.h/cpp, 15 curated templates, search/filter/sort, install/uninstall, View > Tools) |
| Notification toast system | Medium | Low | P3 | ✅ Complete (4 types, slide-in/fade-out, wired to save/build/template/scene-load/model-import/component-remove events) |
| Accent color harmony presets | Medium | Low | P3 | ✅ Complete (6 presets, auto-derive 11 accent colors, fine-tune in sub-tree) |
| Theme preview pane | Low | Low | P3 | ✅ Complete (250x160 live preview in Editor Settings) |
| Keyboard shortcuts help | Medium | Low | P3 | ✅ Complete (Ctrl+Shift+/, searchable modal, 5 categories) |
| Hierarchy search bar | Medium | Low | P3 | ✅ Complete (case-insensitive text filter, hides non-matching entities) |
| Entity delete confirmation | Low | Low | P3 | ✅ Complete (modal dialog with Cancel/Delete) |
| Inspector tooltips | Medium | Low | P3 | ✅ Complete (50+ tooltips across Transform/Material/Light/Camera/Rigidbody/Collider) |
| Source-app import presets | Medium | High | P3 | ✅ Complete (10 DCC presets with auto-detection, per-axis flip toggles, texture search paths, editor import dialog) |
| Pre-built binary distribution | High | Medium | P2 | ✅ Complete (CMake install rules + CPack, Windows ZIP, scripts/package.bat + package.sh) |
| Installer distribution | Medium | High | P3 | ✅ Complete (NSIS installer with Start Menu/Desktop shortcuts, .enjin/.enjpak file associations, uninstaller) |
| Hub application (launcher) | Medium | Very High | P4 | ✅ Done |
| **— Flash Game Revival —** | | | | |
| SWF import & conversion | Medium | Very High | P4 | ✅ Done |
| ~~Flash-style timeline authoring~~ | Medium | High | P3 | ✅ Done |
| AS2/AS3 → AngelScript transpiler | Medium | Very High | P4 | ✅ Done |
| **— Collaboration —** | | | | |
| Collaborative editing (OT/CRDT) | Medium | Very High | P4 | ✅ Complete (EditorLayer wiring: remote edit callbacks, scene sync, edit recording at 5 points, status indicator) |

---

## Metrics to Track

### Performance

- Frame time P99 (target: <16ms for 60fps)
- Draw calls per frame
- Entity iteration count per system per frame
- Texture cache hit rate

### Editor UX

- Time to create first playable entity (target: <30s)
- Clicks to add a component (target: 2)
- Time to find a component in search (target: <2s)

### System Complexity

- Node graph node count (warning at 500+)
- Script instruction count per frame
- Scene entity count (warning at 10,000+)

---

## Editor Tools & UX ✅ COMPLETE

### Completed

- ~~**Extended Model Format Support**~~ ✅ — PLY (ASCII/binary point cloud/mesh) and VOX (MagicaVoxel voxel with greedy face merging) import via custom loaders, routed through SceneImporter
- ~~**Template Rebuild & Demo Scenes**~~ ✅ — Redesigned from 38 to 22 focused templates, then restored 20 removed templates for 44 total across 7 categories (Foundations, Genre Showcases, Systems Deep-Dives, Retro & Flash, Advanced, Multiplayer, Debug/Test), each showcasing real engine features. All 44 templates polished with ~50% more content: additional entities, components, atmosphere, HUD elements, and gameplay setups
- ~~**Planet Gravity Template**~~ ✅ — Super Mario Galaxy-style spherical gravity third-person platformer (GravityZoneComponent Point mode, SurfaceAlignedController, orbit camera, 4 surface platforms, 6 coins)
- ~~**Editor Accent Color & Theming**~~ ✅ — ~~Replace blue accent with TEGE brand sage green~~ (done), ~~customizable accent colors in editor settings~~ (done), ~~accent color harmony presets~~ (done — 6 presets: Default Blue, Warm Orange, Forest Green, Royal Purple, Crimson Red, Teal, auto-derive 11 colors), ~~theme preview pane~~ (done — 250x160 live preview), ~~notification toasts~~ (done — 4 types, slide-in/fade-out), ~~keyboard shortcuts help~~ (done — Ctrl+Shift+/, searchable, categorized), rounded corners, softer panel borders, distinct visual identity
- ~~**Curved Grid Snapping**~~ ✅ — Snap entity placement to curved/spherical grid surfaces with orientation alignment. Surface Snap mode projects entities onto terrain heightmaps and sphere gravity zones, with normal alignment (yaw-preserving) and settings persistence. `Quaternion::FromToRotation()` utility added
- ~~**Improved Icon/Window Inspector**~~ ✅ — Entity icons in hierarchy (bracket-tagged by primary component type), component icons on inspector headers, window icon picker in Project Settings with browse/apply/persist
- ~~**Asset Browser Panel**~~ ✅ — Grid/list view toggle with cached directory listing, image thumbnails via texture cache, search/filter bar, hover tooltip with 256px preview, drag source for future drag-to-viewport, type-colored labels (3D/SCN/SHD/IMG/AS/SFX/PFB), adjustable thumbnail size

### Partially Complete

- ~~**Project Hub & Creation Wizard**~~ ✅ (v1) — All 4 tabs (Recent/New/Open/Demos), 38 templates with category filtering and search, git init option, custom templates, folder structure auto-creation, template hover preview all done. ~~Auto-thumbnail capture~~ ✅ done. **v2 redesign planned** — see "Project Hub Redesign & Template Creator" section: 3-action landing (New/Open/Sandbox), template creator with panel checkboxes, project name in separate popup, `TEGE_Projects` default directory, software distribution tiers
- ~~**Undo/Redo**~~ ✅ — Entity operations, visual script node edits, inspector property edits, tilemap paint (per-stroke with cell deduplication), terrain sculpt (heightmap+splatmap snapshot), UI editor edits (move/resize/nudge/delete) all done
- ~~**Drag and Drop**~~ ✅ — OS file drop, hierarchy reparenting, asset browser to Game View (model/prefab/image/scene/audio/script dispatch), material inspector texture fields, sprite inspector texture fields all done
- ~~**Asset Import Pipeline**~~ ✅ — Import settings dialog, .enjinasset metadata, asset browser drag-import, source-app import presets (10 DCC tools), thumbnails (CPU rasterizer with caching), texture compression (BC1/BC3/BC4/BC5/BC7/ASTC with mipmap gen) — fully complete

### Source-App Import Presets ✅ COMPLETE

Smart import presets for common DCC tools with automatic axis/scale/material fixups. Editor dialog with auto-detection from file metadata, per-axis flip toggles, and texture search paths.

- **Blender** — Z-up → Y-up axis swap, -X forward convention, scale factor (Blender default 1.0 = 1m), auto-detect .blend material names for PBR slot mapping
- **Maya** — Y-up (native match), cm-to-m scale conversion (0.01), FBX ASCII vs binary handling, Lambert/Phong → PBR material approximation
- **3ds Max** — Z-up → Y-up axis swap, system-unit scale auto-detect, multi/sub-object material splitting, .max bitmap path resolution
- **Houdini** — Y-up (native match), scale normalization, procedural attribute mapping (Cd → vertex color, N → normals), packed primitive instancing
- **Cinema 4D** — Y-up (native match), cm-to-m scale, C4D material channels → PBR mapping, MoGraph instance support
- **ZBrush** — Z-up → Y-up, massive poly count warning + auto-decimate option, polypaint → vertex color, subdivision level selection
- **Substance Painter** — Texture set auto-detection, PBR channel mapping (baseColor/normal/roughness/metallic/height/emissive), texture resolution options
- **Unreal/Unity** — Left-hand → right-hand coordinate flip, FBX scale factor differences, material parameter name remapping
- **SketchUp** — Z-up → Y-up, inches/feet-to-meters, face-front/back material separation
- **Custom** — User-defined axis order, handedness, scale, forward/up vectors, material slot overrides

Import dialog enhancements:
- Source app dropdown with auto-detection heuristic (parse FBX metadata `Creator` field, glTF `generator` field)
- Per-axis sign flip toggles (negate X/Y/Z individually) for edge cases
- Material companion file support: OBJ+MTL (already supported via Assimp), FBX embedded textures, glTF separate .bin + texture folder resolution
- Material path remapping: search paths for missing textures, relative ↔ absolute path conversion, texture folder override
- Preview panel: show model with current import settings before committing (axis orientation, scale, material preview)

---

## Runtime Systems ✅ COMPLETE

- ~~**Improved Physics**~~ ✅ — 2D physics (Box2D-style): PhysicsWorld2D with circle/box/polygon shapes, 5 joint types (revolute, prismatic, distance, rope, weld), CCD, physics materials (friction, restitution, density), 2D raycasts/overlap queries, impulse-based collision resolution with SAT, collision enter/exit callbacks, bitmask filtering
- ~~**Basic Networking**~~ ✅ — Host-authoritative UDP networking with client-side prediction: `NetworkSystem` (connections, heartbeats, timeouts), `NetworkTransport` (cross-platform non-blocking UDP sockets — Winsock2/BSD), `NetworkSerializer` (binary read/write), `NetworkIdentityComponent` + `NetworkTransformComponent`, entity ownership + transfer, 20Hz state sync with delta compression (field bitmask), interpolation buffer (4-state ring with configurable delay), RPC system (FNV-1a hashed names, reliable/unreliable), lobby (player list, ready state, host migration stubs), reliable delivery (sequence numbers, ack bitfield, retransmission), HMAC-SHA256 packet authentication (`NetworkSecurity.h`: SHA-256 + HMAC + session key exchange + 64-bit replay window with constant-time verify), editor Network Panel (host/join/disconnect, player list table, ping/loss/bandwidth stats), full scene serialization
- ~~**Destructible Environments**~~ ✅ — DestructibleSystem with 4 fracture patterns (Voronoi, Grid, Radial, Shatter), debris spawning with physics (velocity, gravity, angular velocity, lifetime), chain destruction propagation with radius/delay/falloff, per-entity FractureConfig, health-based damage triggers. Extended with persistent Voronoi fracture (VoronoiMeshFracture + FractureConfigComponent): real ECS fragment entities with rigidbodies, pre-fracture with breakable joints, recursive re-fracture, fragment entity limit, auto-cleanup
- **Simple Fluid Simulation** — ~~Grid-based Eulerian fluid (water, lava, gas). FluidVolumeComponent with preset configs. Target: 64x64 2D / 32x32x32 3D at 60fps~~ ✅ Stable Fluids solver (Jos Stam), 5 presets (Water/Lava/Gas/Smoke/Steam), GPU instanced cell renderer, full editor integration
- **SVG Support** — ~~nanosvg parsing, rasterize-to-texture via SVGLoader, GetOrLoadTexture routing for .svg files~~ ✅. ~~SDF vector rendering~~ ✅. ~~UIElement Image widget integration~~ ✅ (RenderImage uses TextureResolver, SVG routes automatically)
- ~~**Dialogue System Future Work**~~ ✅ (partial) — .enjdlg asset files (DialogueAsset save/load with versioned JSON), LocalizationManager singleton (string key → locale tables, CSV/JSON import/export, parameterized strings with {key} substitution, runtime locale switching, LOC() macro). ~~UICanvas dialogue box~~ ✅ (DialogueBoxComponent auto-builds UICanvas elements with speaker name, typewriter text, portrait, choice buttons, continue indicator). Remaining: Yarn Spinner/Twine import/export
- ~~**Tiered Save System**~~ ✅ — 20-slot save system (17 manual + 3 rotating auto-save) with 3-tier persistence (`PersistenceTier`: SceneState/RunState/MetaProgression). `TieredSaveSystem` class with slot operations, meta-progression key-value store (float/int/bool/string), auto-save timer (configurable interval + on scene transition + on checkpoint), `ISaveBackend` interface with pluggable backends (`LocalSaveBackend`, `NewgroundsSaveBackend`, `SteamSaveBackend` via `ENJIN_STEAM` CMake flag). 15 AngelScript bindings (SaveGame_ToSlot/FromSlot/DeleteSlot/Checkpoint, Meta_Set/Get Float/Int/Bool/String, Meta_Save, AutoSave_Enable/SetInterval). 6 visual script nodes (Gameplay category: SaveToSlot, LoadFromSlot, DeleteSlot, Checkpoint, MetaSetFloat, MetaGetFloat). `SaveLoadMenuComponent` for in-game save/load grid UI. Save Debug editor panel (bit 31). `PlayModeDiff` for cherry-pick entity changes on play mode Stop

---

## Rendering Pipeline & Performance

### Recently Completed

- **GPU Frustum Culling** — Integrated into render pipeline, skips off-screen entities before draw calls
- **Shadow Pipeline Overhaul** — Back-face culling in shadow pass with pipeline depth bias (CSM 0.75/0.75, point 0.5/0.5, spot 0.5/0.5), removed shader-side bias. Fixes ring-of-light under curved objects. Correct cascade frustum computation with world-space ray interpolation
- **Per-Entity Shadow Dither** — 3 modes (by darkness, distance, angle) with 6 built-in dither patterns (Bayer 4x4, Bayer 8x8, Blue Noise, Halftone, Crosshatch, Overlook). Mode in flag bits 14-15, pattern in bits 29-31
- **receiveShadows Flag** — Now checked in shader; entities can opt out of receiving shadows
- **Shadow Caster Caching** — Pre-filtered shadow caster list avoids redundant iteration per cascade

### Recently Completed (cont.)

- **Sprite Texture Atlas** — Runtime shelf-packing of sprite textures (<=512px) into a single 4096x4096 GPU texture. Sprites sharing the atlas batch into one instanced draw call via `"__atlas__"` sentinel key. Per-instance UVs linearly remapped into atlas regions. Lazy rebuild on new textures, invalidation on texture hot-reload. Oversized/failed textures excluded and fall back to individual draw calls
- **Soft Shadows (Poisson Disk PCF)** — 16-sample Poisson disk PCF with configurable shadow softness radius. Applied to directional (CSM), point, and spot light shadows
- **Point/Spot Light Shadow Maps** — Cubemap array depth maps for up to 4 point lights (1024² per face, 6 faces each), 2D array depth maps for up to 4 spot lights (1024²). Shadow data SSBO (binding 12), new descriptor bindings 10-12. Shadow-casting light selection by intensity/distance² scoring. Soft shadows via 3D tangent-frame Poisson disk for point lights, standard 2D Poisson for spot lights
- **3D/2D Pipeline Audit** — Auto-disable shadow pass for 2D-only scenes (`ClassifySceneComposition()` gates all shadow passes by `Scene3D`), sprite batching by texture atlas, ortho/perspective camera mixing diagnostic (every 300 frames), Scene2D fast paths (skip normal map descriptor for unlit sprites, early-out of `UpdateFrameUniforms()` light iteration)
- **Editor Frustum Culling Fix** — GPU frustum culling now disabled in editor mode (`SetEditorMode(true)`) so all entities are visible in the scene view for editing. Player builds retain frustum culling against the game camera. Guards on `BuildCullableObjectList()`, culling dispatch, and all `IsVisible()` skip checks
- **Inspector Undo/Redo** — `InspectorUndo.h` with `PropertyEditCommand<T>` template and drop-in ImGui widget wrappers (DragFloat, DragFloat3, SliderFloat, SliderInt, DragInt, ColorEdit3, Checkbox, Combo, InputText, InputTextMultiline). Continuous widgets snapshot on `IsItemActivated()` and push one undo entry on `IsItemDeactivatedAfterEdit()`. All 60+ `Draw*Component` functions in EditorLayer converted (~500+ widget calls)

### Post-Process Volumes ✅ COMPLETE

- **PostProcessVolumeComponent** — Box/Sphere spatial volumes with priority-based blending, smoothstep falloff at edges via `blendRadius`, global volumes (apply everywhere), selective override mask (19 bits for all effect groups). `BlendPostProcessSettings()` lerps all ~80 PP fields grouped by override mask. Full inspector UI with shape/extents/blend radius/weight/priority, override group checkboxes, embedded PP settings tree. Purple wireframe visualization for volume bounds. Full serialization (all 3 serialize + 4 deserialize paths). 10 AS bindings + 4 VS nodes. Player wired.
- **PP Pipeline Optimization** — `HasAnyActiveEffects()` skips entire PP pass when no effects active (no intermediate RT, no barriers, no fullscreen draw). `NeedsDepthBuffer()` skips 2 depth barriers when DoF/TiltShift/CelOutline are all disabled.

### Screen-Space Effects ✅ COMPLETE

- **God Rays** ✅ — Radial blur from screen-space light position. Configurable density, weight, decay, exposure, samples. Runs in postprocess.frag using depth + invViewProj + light direction
- **SSAO** ✅ — Hemisphere sampling with depth-reconstructed view-space positions. Configurable radius, bias, intensity, sample count
- **Contact Shadows** ✅ — Screen-space ray march toward light for sub-cascade shadowing. Configurable ray length, step count, thickness, fade distance
- **Fake Caustics** ✅ — Animated Voronoi pattern projected below configurable water height. Configurable scale, speed, intensity, depth-aware fade
- **Fog Shafts** ✅ — Volumetric-style light shafts through fog via depth-aware radial sampling. Configurable density, intensity, decay, samples
- All 5 effects: ~256 bytes added to PostProcessSettings UBO, editor UI with 5 collapsing headers, 30 AS bindings + 5 VS nodes, full SceneRenderSettings serialization, Player wiring (invViewProj + light direction/screen-pos), PostProcessVolume override bits 19-23

### Pipeline Optimization ✅ COMPLETE

All pipeline optimization items resolved: multi-threaded command buffer recording, GPU payload batching (sort by pipeline/material), indirect rendering (VkCmdDrawIndexedIndirect), async compute for culling/particles/post-process, frame graph resource scheduling, Hi-Z culling.

### Camera Presets & Cinematic Effects ✅ COMPLETE

- **Camera Presets** ✅ — 9 built-in presets (Isometric45/30, TopDown, SideScroller, FirstPerson, ThirdPerson, CinematicWide, SecurityCam, BirdsEye). `CameraPreset` enum with `ApplyCameraPreset()` returning configured camera + recommended rotation. Inspector dropdown in Camera component header. Script bindings: `Camera_ApplyPreset()`, `Camera_GetPresetName()`
- **Tilt-Shift / Miniature Effect** ✅ — Post-process blur with configurable focus Y position, band width, and blur amount. Full PostProcessSettings UBO fields, SceneRenderSettings config, JSON serialization, editor UI. GPU shader: 25-tap blur weighted by screen-Y distance from focus band
- **Bokeh Depth of Field** ✅ — Focal distance, focal range, near/far blur strength, bokeh size, aperture shape (Circle/Hexagon/Octagon), CoC debug visualization mode. Full pipeline infrastructure with serialization. GPU shader: 16-tap Poisson disc blur weighted by Circle of Confusion, depth linearization with camera near/far planes
- ~~**Cel Shading / Toon Rendering**~~ ✅ — Configurable diffuse band quantization (2-8 bands) and hard specular cutoff in LightingUBO, per-material opt-out (`excludeFromCelShading`), post-process Sobel edge detection outlines on depth (configurable thickness, threshold, color), full editor UI in Rendering + Post-Processing settings, scene render settings serialization
- ~~**Full-Screen Stippling & Dither**~~ ✅ — Post-process stipple/dither effect with 8 combinable patterns via bitmask (Bayer 4x4/8x8, Blue Noise, Halftone, Crosshatch, Overlook, Ordered 2x2, Floyd-Steinberg — any combination, thresholds averaged), 3 color modes (Monochrome, Duo-Tone, Full Color), configurable scale/density/strength, foreground/background color pickers, full editor UI in Post Processing panel with checkbox grid, scene render settings serialization

### Artistic Surface Materials ✅ COMPLETE

Lightweight artist controls for metal, glass, and rim-light effects reusing existing push constant slots. Reflectivity (fake environment reflection via `skyReflectColor`), Fresnel Power (edge vs center falloff), Rim Light (additive edge glow). 4 presets: Metal, Glass, Rim Glow, Clear. Zero new descriptor bindings.

---

## 2D Sprite Art Pipeline ✅ COMPLETE

A complete draw-to-game workflow for 2D and 2.5D projects, from pixel art creation through sprite sheets to playable prefabs.

### Built-In Pixel Editor ✅

Minimal but functional sprite editor inside the engine — no external tools needed for prototyping.

- **Canvas sizes:** Freeform + retro resolution presets (9 presets: Game Boy, NES, SNES, GBA, Genesis, PC Engine, EarthBound, DS, Custom)
- **Tools (8):** Pencil, eraser, fill (flood fill BFS), line (Bresenham), rectangle, ellipse (midpoint), eyedropper, selection
- **Layers:** Layer stack with add/remove, visibility toggle, opacity
- **Palette:** 16-color default palette with color picker, custom palette support
- **Animation:** Onion skinning (before/after tint, configurable frames/opacity), frame timeline with thumbnails, FPS playback preview
- **Export:** Save as `.png` sheet or individual frames, auto-register as engine asset
- **Undo/Redo:** 50-entry stack for all canvas operations
- **Files:** `PixelEditor.h/cpp`, EditorPanel `1<<18`

### Retro Resolution Presets

| Preset | Tile | Sprite | Portrait | System |
|--------|------|--------|----------|--------|
| Game Boy | 8×8 | 8×8, 8×16 | — | 160×144 |
| NES | 8×8 | 8×16 | — | 256×240 |
| SNES | 8×8, 16×16 | 16×16, 32×32 | — | 256×224 |
| Genesis | 8×8 | 8×16, 16×16 | — | 320×224 |
| GBA | 8×8 | 16×16, 32×32, 64×64 | — | 240×160 |
| PC Engine | 8×8 | 16×16, 32×64 | 256×512 | 256×224 |
| EarthBound-style | 8×8 | 16×24, 32×48 | 128×128 | 256×224 |
| DS | 8×8 | 16×16, 32×32 | 128×128 | 256×192 |
| Custom | User-defined | User-defined | User-defined | User-defined |

### Sprite Sheet / Atlas Workflow ✅

- **Import:** Load existing sprite sheets via `SpriteSheetImporter` (stb_image)
- **Grid slicing:** Uniform cell width/height with configurable padding
- **Auto-detect slicing:** BFS flood fill for connected non-transparent regions, noise filtering (4x4 min), position-sorted output
- **Atlas packing:** Runtime `SpriteTextureAtlas` shelf-packs sprites <=512px into 4096x4096 GPU texture
- **UI:** EditorPanel `1<<17` with grid/auto-detect toggle, 48px slice thumbnails, apply-to-entity button
- **Files:** `SpriteSheetImporter.h/cpp`

### Auto Collider Generation ✅

- **Alpha-based bounds:** `FindOpaqueBounds()` scans alpha channel against configurable threshold
- **Bounding shapes (3):** Auto-fit box, capsule, sphere colliders from sprite bounds via `SpriteColliderGenerator`
- **World-space conversion:** Pixel bounds converted relative to sprite pivot and size
- **Files:** `SpriteColliderGenerator.h/cpp`

### Vector Art / SVG Import ✅

- **SVG import:** Parse SVG via nanosvg (`third_party/nanosvg/`), rasterize to RGBA at configurable scale
- **Size capping:** Max 4096x4096, auto-scales down if exceeded
- **Engine integration:** `GetOrLoadTexture()` routes `.svg` files through `SVGLoader::LoadAsTexture()`
- **Files:** `SVGLoader.h/cpp`

### Prefab Output Pipeline ✅

Complete flow from art to playable entity:

1. **Draw** sprite in pixel editor (or import PNG/SVG)
2. **Slice** into frames if sprite sheet
3. **Define** animation sequences with frame timeline
4. **Generate** colliders (box/capsule/sphere from alpha bounds)
5. **Export as prefab** — `ExportAsPrefab()` writes horizontal sprite sheet PNG + `.enjprefab` with Transform, Sprite2D, AnimatedSprite2D (if multi-frame), BoxCollider components
6. **Drag into scene** — instantiate as ready-to-play entity

### Per-Frame Colliders ✅

- **PerFrameColliderComponent:** Vector of `FrameCollider` (offset, size, enabled) indexed by animation frame
- **Auto-apply:** On frame change in RenderSystem, updates BoxCollider center/size from current frame data
- **Inspector:** Match-to-animation button, per-frame offset/size editing

### Polygon Collider 2D ✅

- **PolygonCollider2DComponent:** Arbitrary polygon vertices (CCW winding, local space) with physics material + collision filtering
- **Silhouette tracing:** `SpriteContourTracer` (marching squares + Douglas-Peucker simplification) generates polygon from sprite alpha
- **SpriteColliderGenerator::FitPolygonCollider():** Traces contour, simplifies, converts to world-space relative to pivot
- **Inspector:** Vertex list editing, "Trace Silhouette" auto-generate button, Fit Polygon button in Sprite2D
- **Files:** `SpriteContourTracer.h/cpp`, `SpriteColliderGenerator.h/cpp` (extended)

### Polygon Vertex Editor ✅

- **PixelEditor PolygonEdit tool:** Click to add vertices, click first vertex to close, drag to move, right-click to delete
- **Visual overlay:** Colored dots, edge lines, translucent convex fill over sprite canvas
- **Integration:** 9th tool in toolbar, generates collider from pixel-space vertices

### SDF Vector Rendering ✅

- **SDFGenerator:** 8SSEDT (two-pass sweep) signed distance field from alpha images
- **SVGLoader::LoadAsSDF():** Rasterize SVG then generate SDF for resolution-independent rendering
- **UIWidgetData fields:** `sdfMode`, `sdfSoftness`, `sdfOutlineWidth`, `sdfOutlineColor`
- **Files:** `SDFGenerator.h/cpp`

### Normal Map Generation ✅

- **NormalMapGenerator:** Sobel operator on height/grayscale images → RGB normal map (flat = 128,128,255)
- **GenerateAndSave():** Load height PNG via stb_image, generate, save normal PNG via stb_image_write
- **Inspector integration:** "Generate Normal Map" button in Sprite2D inspector, auto-assigns to normalMapPath
- **Options:** Strength multiplier, Y-flip for DirectX/OpenGL convention
- **Files:** `NormalMapGenerator.h/cpp`

---

## Procedural Generation ✅ COMPLETE

- ~~**Procedural Generation Algorithms**~~ ✅ — 9 standalone algorithm classes in ProceduralAlgorithms.h/cpp:
  - CellularAutomata (cave generation, Moore neighborhood birth/death rules)
  - RandomWalker (dungeon carving, directional bias, turn chance)
  - BSPGenerator (Binary Space Partition room-corridor dungeons, recursive splitting)
  - DiamondSquare (heightmap terrain, midpoint displacement, roughness control) + FractalTerrain (fBm octave stacking, ridged multifractal, hydraulic erosion via droplet simulation, thermal erosion via talus angle)
  - LSystemGenerator (string rewriting with turtle graphics interpretation, F/+/-/[/] commands; advanced 3D turtle with yaw/pitch/roll, stochastic rules, branch radius decay)
  - WaveFunctionCollapse (tile-based with adjacency constraints, entropy-based collapse, backtracking)
  - VoronoiGenerator (Euclidean/Manhattan/Chebyshev distance, region ID grid)
  - GrammarGenerator (shape grammars for buildings, weighted rule selection)
  - PrefabAssembler (snap-together rooms with directional connection points)
  - Editor panel (EditorPanel `1<<24`): algorithm dropdown, parameter sliders, seed input, 256x256 ImGui canvas preview, "Apply to Tilemap" button
- ~~**Custom Flora Assets**~~ ✅ — `customAssetPath` field on GrassVolumeComponent and ShrubVolumeComponent (TreeVolumeComponent already had texture paths). Browse/clear buttons in inspector. Serialization support.

---

## Scripting & Extensibility ✅ COMPLETE

- ~~**Component/Plugin DLL Repositories**~~ ✅ RESOLVED — Plugin repository system with catalog browsing, search/filter by category, install/uninstall, version comparison, `repository.json` format, persistent source management. Extended `PluginManifest` with author/category/tags fields. Editor Plugin Browser panel. Enhanced with `PluginContext` (World, RenderSystem, ScriptEngine, SimpleAudio, SceneManager), `PluginSDK.h` single-header with `ENJIN_IMPLEMENT_PLUGIN()` macro, `OnSaveState/OnRestoreState` for hot-reload state preservation, 4 AS bindings (`Plugin_IsLoaded/GetVersion/Load/Unload`), 3 VS nodes, example plugin in `examples/ExamplePlugin/`.
- ~~**Documentation Generator**~~ ✅ RESOLVED — `DocGenerator` auto-generates markdown from component headers (field parser), AngelScript bindings (via `asIScriptEngine` enumeration), visual script nodes (from `NodeRegistry`), and data asset schemas. Outputs COMPONENTS.md, SCRIPTING_API.md, VISUAL_SCRIPT_NODES.md, DATA_ASSETS.md, INDEX.md to `docs/generated/`. Accessible via Tools menu.
- ~~**ScriptableObject / DataAsset System**~~ ✅ RESOLVED — `DataAssetRegistry` singleton with schemas (`.enjschema`) and assets (`.enjdata`) JSON I/O. 8 field types (String, Float, Int, Bool, Vector3, Vector4, StringArray, FloatArray). AngelScript bindings (`DataAsset_Load/GetFloat/GetInt/GetBool/GetString/GetVector3`). 3 visual script nodes (`DataAsset_Load`, `DataAsset_GetFloat`, `DataAsset_GetString`). Editor Data Asset panel with schema editor, asset browser, inline field editing.
- ~~**MIDI Input**~~ ✅ RESOLVED — Platform-specific MIDI input (WinMM on Windows, stubs on other platforms). `MIDIInput` class with device enumeration, open/close, per-frame double-buffered event polling, persistent CC state. 12 AngelScript bindings (`MIDI_GetDeviceCount/GetDeviceName/OpenDevice/CloseDevice/IsDeviceOpen/IsNoteOn/IsNoteOff/GetNoteVelocity/GetCC/GetCCValue/GetEventCount`). Wired in PlayMode and Player. Files: `MIDIInput.h/cpp`, `ScriptBindings_MIDI.cpp`.

---

## Platform & Export

### Desktop Platforms

#### Linux ✅ Done
- **Render backend:** Vulkan (native, no translation layer)
- **Window/input:** GLFW already cross-platform — validate Wayland + X11 paths
- **Audio:** miniaudio already supports ALSA/PulseAudio/PipeWire — verify and test
- **Build system:** CMake + GCC/Clang, produce `.AppImage` or Flatpak via BuildPipeline
- **File I/O:** POSIX path normalization (forward slashes), XDG directory conventions for saves/config
- **Packaging:** AppImage bundle (self-contained), Flatpak manifest, optional .deb/.rpm
- **Dev API stubs:** `PlatformLinux.h` — `LinuxWindow`, `LinuxFileBrowser`, `LinuxNotification`, XDG desktop integration

#### macOS ✦ Planned
- **Render backend:** MoltenVK (Vulkan-over-Metal translation)
- **Window/input:** GLFW already supports macOS — validate Retina scaling, trackpad gestures
- **Audio:** miniaudio CoreAudio backend — verify and test
- **Build system:** CMake + AppleClang, produce `.app` bundle via BuildPipeline
- **Code signing:** Notarization pipeline (codesign + staple) for Gatekeeper
- **Platform abstractions:** `PlatformMacOS.h` — `MacOSWindow`, `MacOSFileBrowser`, Retina DPI scaling, Dark Mode detection, menu bar integration
- **Considerations:** Apple Silicon (ARM64) + Intel (x86_64) universal binary, Metal shader cross-compilation validation

#### Steam Deck ✅ Done
- **Render backend:** Vulkan (native — AMD RDNA2 GPU, same as desktop Linux)
- **Input:** Steam Input API integration for trackpad, gyroscope, back paddles, haptic feedback. Fallback to standard gamepad via SDL/GLFW
- **Display:** 1280x800 (Deck LCD) / 1280x800 HDR (Deck OLED) target resolution, 40-60fps adaptive framerate
- **Performance profiles:** Configurable TDP-aware quality presets (Low/Medium for 15W, High for 25W)
- **Steamworks integration:** Steam overlay, cloud saves, achievements, Rich Presence, workshop
- **Dev API stubs:** `PlatformSteamDeck.h` — `SteamInput`, `SteamCloud`, `SteamOverlay`, gyro aim assist, on-screen keyboard, suspend/resume lifecycle
- **Testing:** Proton compatibility layer validation (for Windows builds running on Deck), native Linux build preferred
- **Build target:** Linux x86_64 binary + Steam app manifest, verified Deck compatibility badge requirements

### Console Platforms

#### Nintendo Switch (Original) ✅ Stub Done (requires licensed devkit for full impl)
- **Render backend:** NVN (Nintendo's proprietary Vulkan-like API) or Vulkan 1.1 subset via nnsdk
- **Platform SDK:** Nintendo SDK (NintendoSDK / nnsdk), requires approved developer agreement
- **Input:** Joy-Con (accelerometer, gyro, IR, HD rumble), Pro Controller, touch screen, motion controls
- **Display:** 1280x720 docked / 720x480 handheld, 30-60fps adaptive
- **Memory:** 4GB RAM — aggressive memory budgeting, texture streaming, LOD bias
- **Audio:** nn::audio (AAC hardware decode, 6-channel mixing)
- **Dev API stubs:** `PlatformSwitch.h` — `SwitchInput` (Joy-Con/Pro), `SwitchAccount` (user profiles), `SwitchSave` (save data mount), `SwitchPerformance` (CPU/GPU clock modes), docked/handheld detection, sleep/wake lifecycle
- **Certification:** Nintendo Lotcheck requirements checklist (controller disconnect handling, error screens, HOME button, sleep mode, user account)
- **Build target:** ARM64 (Cortex-A57) binary via Nintendo toolchain, .nsp/.nca packaging

#### Nintendo Switch 2 ✦ Planned (requires licensed devkit)
- **Render backend:** Vulkan 1.3 (expected NVIDIA Ampere-class GPU), potential DLSS/FSR upscaling
- **Platform SDK:** NintendoSDK next-gen (TBD — API stubs prepared based on Switch 1 patterns)
- **Input:** Joy-Con 2 (magnetic attachment, mouse-like optical sensor, C-button expected), Pro Controller 2, backward compatibility with Switch 1 controllers
- **Display:** 1920x1080 docked / 720p+ handheld (expected), HDR support (expected)
- **Memory:** 8-12GB RAM (expected) — more generous budgets than Switch 1
- **Dev API stubs:** `PlatformSwitch2.h` — extends `PlatformSwitch.h` with `Switch2Input` (optical sensor, C-button), `Switch2Display` (HDR, higher res), `Switch2Performance` (DLSS hooks, ray tracing stubs), backward-compat mode flag
- **Certification:** Nintendo Lotcheck 2 requirements (TBD — prepare based on Switch 1 + expected additions)
- **Build target:** ARM64 binary via Nintendo toolchain, next-gen packaging format

#### PlayStation 5 ✦ Planned (requires licensed devkit)
- **Render backend:** AGC (PlayStation proprietary low-level graphics API) or Vulkan subset via PSDK
- **Platform SDK:** PlayStation Partners SDK (PSDK), requires SIE partnership approval
- **Input:** DualSense — adaptive triggers (resistance curves), haptic feedback (HD rumble per-actuator), touchpad, motion sensor, microphone, speaker. PS VR2 passthrough
- **Display:** 4K (3840x2160) primary, 1080p performance mode, 120Hz support, HDR10/Dolby Vision
- **Audio:** Tempest 3D AudioTech (object-based spatial audio, HRTF), custom audio pipeline integration
- **Storage:** SSD streaming — custom I/O decompression (Kraken/Oodle), no loading screens design pattern
- **Dev API stubs:** `PlatformPS5.h` — `PS5Input` (DualSense adaptive triggers, haptics), `PS5Trophy` (achievements), `PS5Activity` (Activity Cards, Game Help), `PS5Save` (save data), `PS5Network` (PSN, multiplayer), `PS5SSD` (direct storage streaming), `PS5HDR` (HDR tone mapping)
- **Certification:** Sony TRC (Technical Requirements Checklist) — error handling, graceful degradation, PS button behavior, suspend/resume, network disconnection
- **Build target:** x86_64 (AMD Zen 2) binary via PSDK toolchain, .pkg packaging

#### Xbox Series X|S ✦ Planned (requires ID@Xbox or licensed devkit)
- **Render backend:** Direct3D 12 (or Vulkan via D3D12 interop layer — preferred: native D3D12 backend)
- **Platform SDK:** GDK (Game Development Kit), available via ID@Xbox program or Microsoft partnership
- **Input:** Xbox Wireless Controller (impulse triggers, share button), adaptive controller support
- **Display:** 4K Series X / 1440p Series S, 120Hz, HDR10, Dolby Vision, Auto HDR
- **Audio:** Dolby Atmos, Windows Sonic spatial audio
- **Storage:** Xbox Velocity Architecture — DirectStorage API, Sampler Feedback Streaming, BCPack texture compression
- **Smart Delivery:** Single package with Series X (high quality) and Series S (reduced resolution/LOD) asset tiers
- **Dev API stubs:** `PlatformXbox.h` — `XboxInput` (impulse triggers, share), `XboxLive` (achievements, leaderboards, cloud saves, multiplayer), `XboxGDK` (activity feed, Game Pass integration), `XboxDirectStorage` (SSD streaming), `XboxHDR` (HDR metadata)
- **Certification:** Microsoft XR (Xbox Requirements) — controller disconnection, network loss, Quick Resume save/restore, accessibility requirements (narrator, high contrast)
- **Build target:** x86_64 (AMD Zen 2) binary via GDK toolchain, MSIXVC packaging

### Platform Abstraction Layer

All platform targets share a common abstraction:

```cpp
// Engine/include/Enjin/Platform/PlatformTarget.h
class PlatformTarget {
public:
    virtual ~PlatformTarget() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void PollEvents() = 0;

    // Input
    virtual InputDeviceCapabilities GetInputCapabilities() = 0;
    virtual bool HasGyroscope() = 0;
    virtual bool HasTouchscreen() = 0;
    virtual bool HasHaptics() = 0;

    // Display
    virtual DisplayInfo GetDisplayInfo() = 0;  // resolution, refresh, HDR
    virtual bool SupportsHDR() = 0;

    // Storage
    virtual std::string GetSaveDirectory() = 0;
    virtual std::string GetCacheDirectory() = 0;
    virtual bool HasFastStorage() = 0;  // SSD direct streaming

    // Platform services
    virtual bool HasAchievements() = 0;
    virtual bool HasCloudSaves() = 0;
    virtual bool HasOverlay() = 0;

    // Power/thermal
    virtual PowerProfile GetPowerProfile() = 0;  // Desktop/Plugged/Battery/TDP-limited
    virtual float GetThermalHeadroom() = 0;       // 0.0-1.0
};
```

### Implementation Priority

| Platform | Priority | Rationale |
|----------|----------|-----------|
| Linux | P1 | ✅ Done — LinuxPlatform.h, XDG paths, AppImageBuilder, CMake Linux targets |
| Steam Deck | P1 | ✅ Done — SteamDeck.h, SteamInput.h, AdaptiveQuality.h, gyro/suspend stubs |
| macOS | P2 | MoltenVK translation layer, Apple Silicon support needed, code signing complexity |
| Xbox Series X\|S | P2 | GDK freely available via ID@Xbox, D3D12 backend required |
| PlayStation 5 | P3 | Requires SIE partnership, proprietary AGC graphics API, high cert bar |
| Nintendo Switch 2 | P3 | NDA SDK, Vulkan 1.3 expected — closer to desktop pipeline |
| Nintendo Switch 1 | P4 | ✅ Stub — NVNBackend.h, SwitchPlatform.h (requires licensed devkit for full impl) |

### Shared Systems of Support

- **Cross-platform build matrix** — CMake presets per platform, CI/CD build agents (GitHub Actions for desktop, self-hosted for console devkits)
- **Platform capability query** — Runtime `PlatformTarget::Has*()` checks drive adaptive quality, input schemes, and feature availability
- **Adaptive quality system** — Auto-select render resolution, shadow quality, particle count, LOD bias based on platform power profile and thermal headroom
- **Input abstraction** — Unified `InputAction` system already exists; extend with platform-specific device capabilities (gyro, adaptive triggers, touchpad)
- **Save data abstraction** — Platform-agnostic save API wrapping platform-specific storage (PSN save data, Xbox cloud saves, Steam cloud, local filesystem)
- **Achievement/trophy abstraction** — Common API mapping to PSN trophies, Xbox achievements, Steam achievements
- **Certification test suite** — Automated test runner covering common cert requirements: controller disconnect/reconnect, network loss recovery, suspend/resume, error dialogs, accessibility checks

### Mobile Platforms (Lower Priority)

- **Android** — Vulkan (native), touch input, gyroscope, screen density, mobile render paths, Google Play services
- **iOS** — MoltenVK, touch input, Game Center, App Store guidelines, notch/safe area handling

### Emerging Platforms (Future)

- **VR/XR Support** — OpenXR integration, stereo rendering, hand tracking, spatial input, roomscale
- **WebAssembly Export** — Target WebGPU (not WebGL), WASM via Emscripten

---

## Ray Tracing & Path Tracing ✅ COMPLETE

The full Vulkan ray tracing pipeline is implemented and wired end-to-end. `CompositeRTResults()` is called after SVGF denoising, the real depth buffer is bound to RT descriptor binding 2, and camera change detection resets path tracer accumulation. `DenoiseRTOutputs()` executes the full SVGF pass sequence (temporal, variance, a-trous wavelet). All code, shaders (GLSL), editor UI, serialization, and compiled SPIR-V bytecode are in place. All 19 RT shaders are compiled and embedded in `RTShaderData.h` (384–10,964 bytes each). The RT pipeline activates automatically on supported hardware.

### Architecture

Hybrid rendering pipeline: rasterization for primary visibility (existing Vulkan pipeline unchanged) with optional ray/path tracing for lighting effects. Vulkan Ray Tracing extensions (`VK_KHR_ray_tracing_pipeline`, `VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations`, `VK_KHR_buffer_device_address`).

### Implementation Status

| Feature | Technique | Status |
|---------|-----------|--------|
| **RT Capabilities** | Extension detection, properties query, graceful fallback | ✅ Complete |
| **Acceleration Structures** | BLAS per unique mesh (hash dedup), TLAS rebuilt per frame | ✅ Complete |
| **RT Pipeline** | vkCreateRayTracingPipelinesKHR, SBT construction | ✅ Complete |
| **RT Shadows** | 1 SPP ray traced shadows, configurable distance/radius | ✅ Complete |
| **RT Reflections** | Single-bounce specular reflections with roughness threshold | ✅ Complete |
| **RT Ambient Occlusion** | Short-range AO hemisphere sampling | ✅ Complete |
| **RT Global Illumination** | Multi-bounce diffuse GI with configurable bounces/intensity | ✅ Complete |
| **Path Tracing Mode** | Progressive accumulation, camera-reset, SPP tracking | ✅ Complete |
| **SVGF Denoiser** | 3-pass compute (temporal, variance, a-trous wavelet) | ✅ Complete |
| **RT Compositor** | Fullscreen compute composite of RT layers into scene HDR | ✅ Complete |
| **Editor Panel** | Per-effect toggles, config sliders, path tracer progress, stats | ✅ Complete |
| **Scene Settings** | 24 RT config fields with full JSON serialization | ✅ Complete |
| **Caustics** | Photon mapping or path traced caustics for glass/water | Planned |
| **RT Translucency** | Subsurface scattering via random walk in medium | Planned |
| **OIDN integration** | Intel Open Image Denoise for cross-platform neural denoising | ✅ Complete (OIDNDenoiser.h/cpp, CMake ENJIN_RAYTRACING_OIDN, editor denoiser type selector) |

### Files

**Headers** (`Engine/include/Enjin/Renderer/RayTracing/`):
- `RTCapabilities.h` — Feature detection struct + `Query()` + `GetRequiredExtensions()`
- `AccelerationStructure.h` — BLAS/TLAS low-level wrappers with vkGetDeviceProcAddr function pointers
- `AccelerationStructureManager.h` — BLAS cache by mesh hash, per-frame TLAS rebuild, instance management
- `RTPipeline.h` — RT pipeline wrapper (SBT regions, shader groups)
- `RTShadows.h` — Shadow ray dispatch + config (SPP, distance, radius)
- `RTReflections.h` — Specular reflection dispatch + config (SPP, distance, roughness threshold)
- `RTAmbientOcclusion.h` — AO hemisphere sampling dispatch + config (radius, power)
- `RTGlobalIllumination.h` — Multi-bounce diffuse GI dispatch + config (bounces, intensity)
- `PathTracer.h` — Progressive path tracer with accumulation buffer
- `IDenoiser.h` — Abstract denoiser interface
- `SVGFDenoiser.h` — 3-pass SVGF compute denoiser
- `RTCompositor.h` — Compute shader to composite RT layers into scene HDR
- `RTShaderData.h` — Embedded compiled SPIR-V for all 19 RT + compute shaders (384–10,964 bytes each)

**Sources** (`Engine/src/Renderer/RayTracing/`):
- Matching `.cpp` files for all headers above (11 source files)

**Shaders** (`Engine/shaders/`):
- `rt_common.glsl` — Shared structs, RNG, hemisphere sampling
- `rt_shadow.rgen/.rmiss/.rchit` — Shadow rays toward lights
- `rt_reflect.rgen/.rmiss/.rchit` — Specular reflection rays
- `rt_ao.rgen/.rmiss/.rchit` — Cosine-weighted AO hemisphere
- `rt_gi.rgen/.rmiss/.rchit` — Multi-bounce diffuse GI
- `rt_pathtrace.rgen/.rmiss/.rchit` — Full progressive path tracer
- `svgf_temporal.comp`, `svgf_variance.comp`, `svgf_atrous.comp` — SVGF denoiser passes
- `rt_composite.comp` — Composite RT layers into scene HDR

### Render Frame Flow (with RT)

```
BeginFrame()
  │
  FlushPendingBLASBuilds()        ← batch BLAS builds for new meshes
  │
  RenderShadowPass()              ← raster shadows (skipped if RT shadows on)
  │
  RebuildTLAS()                   ← update TLAS from entity transforms
  DispatchRTEffects()             ← trace shadow/reflect/AO/GI rays
  DenoiseRTOutputs()              ← SVGF 3-pass compute
  │
  BeginMainRenderPass()
    RenderEntities()              ← existing rasterization
    RenderSprites/Effects()
  EndMainRenderPass()
  │
  CompositeRTResults()            ← compute: multiply shadows, add reflections,
  │                                  multiply AO, add GI into scene HDR
  PostProcessing::Apply()         ← existing tonemapping/bloom/FXAA
  │
  EndFrame()
```

Only runs for `SceneRenderMode::Scene3D`. 2D/2.5D scenes skip the RT pipeline entirely.

### RT Descriptor Set Layout (Set 1)

| Binding | Type | Purpose |
|---------|------|---------|
| 0 | ACCELERATION_STRUCTURE | TLAS |
| 1 | STORAGE_IMAGE | Scene HDR (read-write for composite) |
| 2 | COMBINED_IMAGE_SAMPLER | Depth buffer |
| 3 | COMBINED_IMAGE_SAMPLER | World normals |
| 4 | COMBINED_IMAGE_SAMPLER | Motion vectors |
| 5 | STORAGE_IMAGE | RT Shadow output (R16F) |
| 6 | STORAGE_IMAGE | RT Reflection output (RGBA16F) |
| 7 | STORAGE_IMAGE | RT AO output (R16F) |
| 8 | STORAGE_IMAGE | RT GI output (RGBA16F) |
| 9 | STORAGE_BUFFER | Material data |
| 10 | STORAGE_BUFFER | Vertex data |
| 11 | STORAGE_BUFFER | Index data |
| 12 | STORAGE_BUFFER | Per-instance transforms |
| 13 | UNIFORM_BUFFER | Light data |

### Graceful Fallback

- `RTCapabilities::Query()` checks extension support at physical device selection
- If unsupported: all RT unique_ptrs remain null, editor shows "Not Supported" badge
- All 19 RT shaders are compiled SPIR-V (384–10,964 bytes); pipeline creates automatically on supported hardware
- All RT code paths guarded by `if (m_RTEnabled && m_ASManager)` checks
- Raster shadows/SSAO remain the fallback path

### Hardware Requirements

- **RT hardware path:** Vulkan RT extensions (NVIDIA RTX 20xx+, AMD RX 6000+, Intel Arc)
- **Denoising:** SVGF on any Vulkan GPU with compute shaders
- **Minimum for real-time RT:** Target 1080p @ 30fps with 1 SPP + SVGF on RTX 3060-class hardware

### Remaining Work

1. ~~**Compile RT shaders**~~ ✅ — All 19 GLSL shaders compiled to SPIR-V and embedded in `RTShaderData.h`
2. ~~**Embed SPIR-V**~~ ✅ — Real compiled bytecode (384–10,964 bytes each) replaces placeholder stubs
3. ~~**Wire composition + denoising**~~ ✅ — `CompositeRTResults()` wired after denoising, real depth buffer on binding 2, `DenoiseRTOutputs()` replaced with real SVGF calls, camera change detection for path tracer reset
4. ~~**OIDN integration**~~ ✅ — Intel Open Image Denoise as alternative cross-platform neural denoiser. `OIDNDenoiser.h/cpp`, `ENJIN_RAYTRACING_OIDN` CMake option, editor denoiser type selector. `RegisterImageMapping()` wired for all RT effect outputs + dummy image in `InitializeRayTracing()`
5. **OptiX integration** — NVIDIA OptiX AI Denoiser for best quality on NVIDIA GPUs

---

## Accessibility (Engine-Level)

### Editor Accessibility ✅ Complete

The editor itself is fully accessible:

- ~~**Screen Reader Support**~~ ✅ (partial) — AccessibilityAnnouncer with priority-queued text status bar (Low/Normal/High/Critical), visual bottom-bar overlay with color-coded priority, console logging option, configurable display duration. Groundwork for future OS accessibility API integration (UI Automation, AT-SPI, NSAccessibility)
- ~~**Keyboard-Only Navigation**~~ ✅ — Panel focus shortcuts (Ctrl+1-5 for Hierarchy/Inspector/Viewport/Console/Assets), focus ring indicators (blue border on active panel), keyboard gizmo nudge (Arrow keys: translate/rotate/scale, Ctrl+Arrow: fine nudge, PageUp/PageDown: Y axis), configurable nudge amounts
- ~~**Alternative Input Devices**~~ ✅ — AlternativeInputManager with switch access (scanning mode, configurable scan speed, 1-4 switches, auto-reverse), eye tracking (dwell-click, smoothing, dead zone, gaze indicator), sip-and-puff (configurable pressure thresholds), head tracking (sensitivity, smoothing, dead zone, axis inversion). Editor settings panel with per-device configuration. Requires external driver software for hardware integration
- ~~**Motor Accessibility**~~ ✅ — Adjustable click threshold (1-20px), drag threshold (1-30px), dwell-click (auto-click after configurable hover delay 0.3-3.0s), sticky drag (click-to-start, click-to-release), hold repeat delay/rate. "Motor Impaired" quick preset enables all motor aids + keyboard nav. All settings persistent in editor_settings.json
- ~~**Visual Accessibility**~~ ✅ — Configurable font sizes, UI scale (0.75-2.0x), custom accent colors (11 elements), 8 colorblind modes, brightness/contrast, 11 themes including 4 high-contrast
- ~~**Audio Accessibility**~~ ✅ — AudioVisualIndicatorSystem: colored dot overlays for audio events (one-shot fade-out + continuous pulse), configurable size/position/labels. Test buttons in editor settings
- ~~**Blind-Accessible Workflow**~~ ✅ — Command palette (Ctrl+P) with fuzzy search over 25+ registered commands for entity CRUD, scene operations, view panel toggles, gizmo modes, play mode, and accessibility settings. Keyboard-only navigation (Up/Down/Enter/Esc), category tags, shortcut hints. Announcer integration announces executed commands for screen reader workflows

### Runtime Game Accessibility — Gaps & Planned Work

The following accessibility features exist as infrastructure/settings structs but are **not yet wired to the runtime Player app or UICanvas system**. Games built with the engine currently lack user-facing accessibility unless developers manually implement it.

#### Working at Runtime
- ~~**Subtitles**~~ ✅ — SubtitleSystem integrated with DialogueSystem, configurable font size (16-48px), background opacity, direction indicators, speaker names, position control
- ~~**Colorblind Filter**~~ ✅ — 8 colorblind modes applied globally to GPU rendering pipeline via PostProcessing, configurable strength
- **RuntimeAccessibilitySettings struct** ✅ — Defined with reducedMotion, disableScreenShake, disableFlashingLights, disableFOVEffects, brightness/contrast, subtitle config, input toggle modes. `ApplyToPostProcessing()` method exists

#### ~~Not Yet Runtime-Ready~~ ✅ All Wired

| Feature | Status |
|---------|--------|
| ~~**Screen Reader for Game UI**~~ | ✅ Done — Announcer wired into UISystem via SetAnnouncerCallback(), announces on focus change/activation in PlayMode + Player |
| ~~**Alternative Input for Game UI**~~ | ✅ Done — AlternativeInputManager wired into PlayMode + Player, Update/RenderOverlay called, scan targets from UISystem |
| ~~**Audio Visual Indicators at Runtime**~~ | ✅ Done — AudioVisualIndicatorSystem wired into PlayMode + Player, auto-indicators from SimpleAudio callbacks |
| ~~**Content Warnings**~~ | ✅ Done — ContentWarningSystem in Player, reads per-scene flags, shows overlay before game render |
| ~~**Motor Accessibility at Runtime**~~ | ✅ Done — Dwell-click + sticky drag on UICanvas, configurable via accessibility settings |

#### Missing Entirely (Needs Implementation)

| Feature | Priority | Effort | Description |
|---------|----------|--------|-------------|
| ~~**UICanvas Keyboard Navigation**~~ | P1 | Medium | ~~Tab/Shift+Tab between focusable UIElements, Enter/Space activation, arrow key menu navigation. `tabOrder` field on UIElement, focus state machine in UISystem~~ ✅ Complete |
| ~~**UICanvas Gamepad Navigation**~~ | P1 | Medium | ~~D-pad/analog stick navigation between UIElements, A/B activation. Arrow/DPad/Left Stick navigation with key repeat~~ ✅ Complete |
| ~~**Focus Indicators**~~ | P1 | Low | ~~Visible focus ring rendering on active UIElement (configurable color/width in UITheme, per-element focusColor override). Rendered in UISystem after widget render~~ ✅ Complete |
| ~~**Accessible Labels**~~ | P2 | Low | ~~Add `accessibleLabel` field to UIElement for screen reader text. Falls back to element name if not set. Serialized in SceneSerializer, editable in inspector~~ ✅ Complete |
| ~~**In-Game Accessibility Menu**~~ | P1 | Medium | ~~Accessibility Menu startup template with subtitle toggle, subtitle size slider, colorblind toggle, reduced motion toggle, input sensitivity slider. Uses focus navigation for keyboard/gamepad control~~ ✅ Complete |
| ~~**High Contrast UI Theme**~~ | P2 | Low | ~~`HighContrastDark` and `HighContrastLight` presets with WCAG AAA 7:1+ contrast ratios, 3px borders, bright focus indicators~~ ✅ Complete |
| ~~**Font Scaling for Players**~~ | P2 | Low | ~~`fontScale` field on RuntimeAccessibilitySettings, `SetFontScale()` on UISystem — all 5 text render paths multiply by scale factor. Wired in Player from accessibility settings~~ ✅ Complete |
| ~~**Dyslexia-Friendly Options**~~ | P3 | Low | ✅ Done — FontLibrary.h with FontFamily enum (Default/Monospace/OpenDyslexic), letter/word/line spacing config |
| ~~**Reduced Motion for UI**~~ | P2 | Low | ~~✅ Done — UISystem honors `m_ReducedMotion` flag: cursor blink disabled (always visible), instant tooltips, switch access pulse/scan indicator made static. Wired via `PlayMode::GetUISystem()` setter from EditorLayer~~ |
| ~~**Colorblind-Safe UI Palettes**~~ | P3 | Low | ✅ Done — ColorblindPalette.h with 9 palettes, pattern+icon alongside color (PatternType enum) |
| ~~**One-Button Mode**~~ | P3 | Medium | ✅ Done — Switch access scanning in UISystem with configurable scan speed, visual highlight pulse |

---

## Version Control & Collaboration

- ~~**Git Integration**~~ ✅ — Built-in git panel (stage, commit, push, pull, branch, merge), visual scene diff (structured JSON), optional `git init` on project creation
- ~~**Scene & Entity Locking**~~ ✅ — SceneLockManager with advisory `.enjinlock` JSON sidecar files: entity-level locking (Lock/Unlock in hierarchy context menu), user/machine identification (auto-detected from environment), visual indicators ([X] locked by other = red overlay, [=] locked by self), inspector lock warning banner, automatic lock file cleanup (deleted when no locks remain), 5-second auto-refresh. Scene-level locking API (LockScene/UnlockScene). All locks released on scene close/shutdown
- ~~**Collaborative Editing**~~ ✅ — CollaborativeEditingUI wires OT-based system into editor: host/join panel, remote op handlers (8 op types applied to local scene), peer cursor visualization, conflict resolution dialog, lock enforcement, scene sync on join
- ~~**Clean Git Serialization**~~ ✅ — Deterministic scene files (sorted keys, stable ordering, no floating-point drift)

---

## Flash Game Revival & Retro Web Game Support

Target audience: Flash game creators and fans of the Flash/Newgrounds era looking for modern tooling.

### ~~SWF Import & Conversion~~ ✅ Done
- ~~**SWF parser**~~ ✅ — `SWFLoader` + `SWFConverter` — parse SWF, rasterize shapes to PNG, extract bitmaps/sounds, convert to ECS entities with Sprite2DComponent
- ~~**Timeline-to-Animation**~~ ✅ — SWF frames → TimelineComponent keyframes, frame labels → markers, MovieClip depths → child entities
- ~~**MovieClip mapping**~~ ✅ — SWF sprite hierarchy → entity hierarchy, SWFMatrix → TransformComponent (twip→pixel, Y-flip), SWFColorTransform → material tint

### Flash-Style Authoring Workflow
- ~~**Frame-based timeline editor**~~ ✅ — `TimelineEditor` class extends Timeline with Flash-style authoring: layers, property tracks, 4 interpolation modes (Constant/Linear/Bezier/CatmullRom), auto-key, curve editor with tangent handles, dopesheet view, onion skinning, copy/paste/delete, frame snapping
- ~~**Vector drawing tools**~~ ✅ — VectorDrawingEditor with 7 shape types (Line/Rect/Ellipse/Pen/Bezier/Star/Polygon), 8 tools, ImDrawList canvas rendering, layers, undo/redo (50 levels), SVG export, snap-to-grid, zoom/pan, property panel. EditorPanel `1<<29`
- ~~**Symbol library**~~ ✅ — SymbolLibrary.h/cpp: catalog-based system with grid browser, category tabs, search, nested editing (isolated World), prefab bridge, FlashTimeline integration, thumbnail previews

### ~~ActionScript Compatibility Layer~~ ✅ Done
- ~~**AS2/AS3 → AngelScript transpiler**~~ ✅ — `AS3Transpiler` class: pattern-based line-by-line transpilation with regex, type mapping (Number→float, etc.), class/function/var conversion, Flash API → shim calls, scope tracking
- ~~**Flash API shim library**~~ ✅ — `FlashAPIShim` namespace: `RegisterFlashBindings()` with DisplayObject (position/scale/rotation/alpha/visible), MovieClip (gotoAndPlay/Stop/Play/Stop), Stage (width/height/fps), Mouse/Keyboard, TextField, Math, Sound (via SimpleAudio), Timer (setTimeout/setInterval)
- ~~**Newgrounds.io integration**~~ ✅ — Medal/scoreboard API bindings via HTTP. `SharedObject` persistence mapped to `TieredSaveSystem` meta-progression (`so_{name}_{key}` namespacing), 5 AngelScript bindings (Flash_SO_Set/Get/Has/Flush/Clear), AS3 transpiler mappings, wired in PlayMode + Player

### Flash Game Templates
- ~~**Starter templates**~~ ✅ — Pre-built project templates for common Flash game genres: point-and-click adventure, dress-up game, tower defense, bullet hell, rhythm game, escape room, idle/clicker. All included in the 43 built-in templates and polished with HUD elements, enemies, inventory, and gameplay setups
- ~~**Newgrounds-style game page**~~ ✅ — NewgroundsGamePage.h/cpp: dark-theme game page with medal sidebar, scoreboard, NG.io API init, toast notifications, responsive layout, embed codes

### WebAssembly Export (Prerequisite)
- **WebGPU/WebAssembly target** — Compile engine to WASM with WebGPU renderer backend. Required for browser-based game distribution
- **Emscripten integration** — Asset pack loading via Fetch API, input mapping for touch/mouse, audio via Web Audio API
- **Size optimization** — Tree-shaking unused systems, texture compression (Basis Universal), audio compression (Opus), code splitting for progressive loading

---

## Project Hub Redesign & Template Creator (Partially Complete)

### Terminology (Clarified Hierarchy)

The engine uses four distinct save concepts — users must clearly understand each:

| Concept | What It Stores | File(s) | When Saved |
|---------|---------------|---------|------------|
| **Scene** | Entity hierarchy, component data, world state | `.enjin` | File > Save Scene |
| **Window Layout** | Panel visibility flags, panel sizes/positions, splitter ratios | `editor_layout.json` (per-project) | Auto-saved on close, or Layout > Save Layout |
| **Template** | A starter scene + a window layout + metadata (name, description, category, thumbnail) | `.enjintemplate` (directory: `scene.enjin` + `layout.json` + `meta.json` + `thumbnail.png`) | Template Creator > Save |
| **Project** | All scenes, assets, scripts, settings, templates, build config | `.enjinproject` (directory with `project.enjinproject` manifest) | File > Save All / auto-save |

### Project Hub Flow Redesign

Replace the current 4-tab flat layout (Recent / New / Open / Demos) with a clearer entry flow:

**Splash Screen** (unchanged: logo + version fade-in, 4s)

**Landing Page** (replaces current Project Hub — 3 primary actions):

```
┌──────────────────────────────────────────────────┐
│                  TEGE Engine                     │
│                                                  │
│   ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│   │  New     │  │  Open    │  │  Sandbox │      │
│   │ Project  │  │ Project  │  │          │      │
│   └──────────┘  └──────────┘  └──────────┘      │
│                                                  │
│              Recent Projects (below)             │
└──────────────────────────────────────────────────┘
```

- **New Project** → Template browser (current "New" tab grid), then a **popup dialog** for project name, location, scene name, git init. Template selection and naming are separate steps — pick template first, then configure project in a modal
- **Open Project** → Project browser with recent project list + "Browse..." button. Shows any valid `.enjinproject` directory. Can be linked/aliased from anywhere on disk (like Unity Hub). Quick-filter search bar, sort by name/date/size
- **Sandbox** → Starts an unnamed, unsaved session using the Template Creator. User picks which panels are visible, optionally loads a template scene, and starts working. Session is disposable by default — prompt to save on close. If saved, becomes a named project. Good for quick prototyping, testing, learning

### Template Creator ✅ COMPLETE (Core)

Core template creator implemented: `TemplateCreator.h/cpp` with `SaveTemplate()`, `LoadTemplate()`, `ScanTemplates()`, `DeleteTemplate()`. Editor UI at View > Tools > Template Creator with metadata editing (name, description, category), save/load/delete. Custom templates stored in `templates/` directory. Original detailed plan below for future enhancements:

A dedicated tool (accessible from Sandbox flow + Tools menu) for creating and editing templates:

**Template Metadata:**
- Template name, description, category (2D/3D/Multiplayer/Custom)
- Author name (auto-filled from system user)
- Thumbnail image — **auto-captured from the viewport** on save (screenshot of the initial scene state at the current camera angle). Can also be manually set via file picker

**Window Layout Configuration:**
- Checklist of all `EditorPanel` flags — user checks which panels should be visible when the template is applied:
  - [ ] Hierarchy
  - [ ] Inspector
  - [ ] Viewport
  - [ ] Console
  - [ ] Asset Browser
  - [ ] Game View
  - [ ] Settings
  - [ ] Effects
  - [ ] Profiler
  - [ ] etc.
- Panel size presets (Compact / Standard / Wide) or custom ratio sliders
- Game View default size and position

**Scene Content:**
- The current scene state becomes the template's starter scene
- Entity list preview (read-only summary of what the template will include)
- Option to strip editor-only data (selection state, camera position) or preserve it

**Save/Export:**
- Save to `templates/` directory as `.enjintemplate` folder (scene.enjin + layout.json + meta.json + thumbnail.png)
- Templates appear in the New Project template browser automatically
- Export as shareable `.enjintemplate.zip` for community sharing

### ~~Template Thumbnail Capture~~ ✅ DONE

~~Each built-in template should have a representative screenshot.~~ Implemented: `RenderTarget::CaptureToPixels()` reads Vulkan framebuffer via staging buffer with BGRA-to-RGBA swizzle. `TemplateCreator::SaveThumbnail()` downscales to 280x180 and writes PNG via `stbi_write_png`. Auto-captures game view on template save. Files: `RenderTarget.h/cpp`, `TemplateCreator.h/cpp`.

### Per-Template Window Layouts

Every built-in template should ship with a thoughtfully designed default layout:

| Template | Visible Panels | Layout Notes |
|----------|---------------|-------------|
| Blank | All defaults | Standard editor layout |
| 2D Platformer | Hierarchy, Inspector, Viewport, Console, Game View | Wide game view (ortho), compact hierarchy |
| 3D Third Person | Hierarchy, Inspector, Viewport, Console, Game View, Settings | Standard 3D layout |
| Visual Novel | Hierarchy, Inspector, Game View, Dialogue | Large game view, dialogue editor prominent |
| RPG Village | All + Effects + Skybox | Full-featured RPG development layout |
| Racing | Viewport, Game View, Console | Maximized game view for splitscreen testing |
| Pixel Art (2D) | Hierarchy, Inspector, Viewport, Pixel Editor, Console | Pixel editor front-and-center |
| Horror | Hierarchy, Inspector, Game View, Effects, Settings | Dark theme auto-applied, effects panel visible |

### Default Project Directory (`TEGE_Projects`)

- **First-run setup**: On first launch (no `editor_settings.json` found), prompt the user to select a default project directory
- Default suggestion: `Documents/TEGE_Projects/` (Windows: `%USERPROFILE%\Documents\TEGE_Projects`, Linux: `~/TEGE_Projects`, macOS: `~/Documents/TEGE_Projects`)
- Stored in `editor_settings.json` as `defaultProjectPath`
- New Project dialog pre-fills this path
- Open Project browser shows this directory by default
- Can be changed anytime in Project Settings > General

### Software Distribution & Installation

The engine is currently source-built (clone + cmake + build). Future distribution tiers:

**Tier 1: Source Distribution (Current)**
- Clone repo, install dependencies (Vulkan SDK, CMake, C++20 compiler)
- `cmake --build` produces `EnjinEditor.exe` + `EnjinPlayer.exe`
- No installer, no registry entries, fully portable
- Target audience: engine developers, contributors

**Tier 2: Pre-Built Binary Distribution ✅ COMPLETE**
- CMake install rules + CPack config in root CMakeLists.txt for Windows ZIP packaging
- `scripts/package.bat` (Windows) and `scripts/package.sh` (Linux/macOS) for one-command builds
- Extract-and-run — no installer needed (portable)
- Includes `EnjinEditor.exe`, `EnjinPlayer.exe`, `glslangValidator`, sample templates
- Target audience: game developers who don't need to modify the engine

**Tier 3: Installer Distribution ✅ COMPLETE**
- Windows: NSIS installer via CPack (`cpack -G NSIS`) with Start Menu + Desktop shortcuts, `.enjin`/`.enjpak` file associations, standard uninstaller
- Generates both ZIP and NSIS from a single `cpack` invocation
- Linux: AppImage or Flatpak (self-contained, no system dependencies) — future
- macOS: `.dmg` with drag-to-Applications — future
- Auto-updater (check GitHub releases API on startup, download + replace binaries) — future
- Target audience: end users, non-technical game developers

**Tier 4: Hub Application (Future)**
- Standalone launcher (like Unity Hub / Epic Games Launcher)
- Manages multiple engine versions side-by-side
- Project browser with create/open/recent
- Template marketplace integration
- Account system for license management (free/indie/pro tiers)

---

## Artistic Rendering & Visual Techniques ✅ COMPLETE

All planned artistic rendering techniques have been implemented.

### Surface & Material Rendering

- ~~**Parallax Occlusion Mapping**~~ ✅ — 4 POM modes (Basic/Steep/Occlusion/Relief) with self-shadowing, silhouette correction, configurable depth. Per-material toggle in inspector
- ~~**Flat-Shaded Low-Poly with Dithered Gradients**~~ ✅ — Per-material `ditherGradient` with 2-8 bands and 6 dither patterns (Bayer 4x4/8x8, Blue Noise, Halftone, Crosshatch, Overlook). `surfaceParam1` push constant
- ~~**Metaball / Blob Rendering**~~ ✅ — Marching cubes isosurface extraction, gradient normals, per-group color blending. Inspector component with live preview

### Lighting & Global Illumination

- ~~**Spherical Harmonics Lighting**~~ ✅ — SH probe baking (L2, 9 coefficients) for diffuse indirect lighting. Probe grid placement tool in editor (per-probe list with bake status colors, individual Bake/Delete buttons, L0 irradiance preview, manual probe placement via DragFloat3 + Add), viewport visualization (green=baked, red=empty spheres, yellow grid bounds AABB), blend weights. Wired to renderer via LightingUBO shProbeIrradiance. Bake button in Rendering settings
- ~~**Beam Tracing and Cone Tracing (VXGI)**~~ ✅ — Voxel cone tracing for real-time diffuse/specular GI, AO, and god rays via mip pyramid. Voxelized scene representation

### SDF & Distance Field Rendering

- ~~**SDF Ray Marching**~~ ✅ — 6 SDF primitives, 6 boolean ops, CPU evaluation, GPU buffer packing, sphere tracing
- ~~**SDF Rendering (3D Vector Art)**~~ ✅ — Mesh-to-SDF conversion, sphere tracing, 8SSEDT text rendering, volume blending. Resolution-independent 3D vector graphics

### Transparency & Compositing

- ~~**Order-Independent Transparency**~~ ✅ — Weighted blended OIT with composite pipeline and embedded SPIR-V (`oit_composite.frag` + `fullscreen.vert`)
- ~~**Framebuffer Feedback Effects**~~ ✅ — 8 presets (Echo, Melt, Infinite Mirror, VHS, etc.), ping-pong compositing, 5 blend modes, configurable decay/UV offset

### Distortion & Post-Processing

- ~~**Screen-Space Distortion**~~ ✅ — 7 distortion types including shockwave, heat haze, underwater caustics, dream warping. Per-object distortion volumes

### Mesh & Animation

- ~~**IK-Driven Mesh Deformation**~~ ✅ — FABRIK + Verlet physics IK chains with tube/ribbon mesh skinning for tentacles, ropes, tails

### Procedural & Terrain

- ~~**Fractal Terrain and L-System Vegetation**~~ ✅ — Fractal terrain generation (fBm octave stacking, ridged multifractal) with erosion simulation (hydraulic via droplet simulation, thermal via talus angle). 3D L-system with full turtle interpreter (yaw/pitch/roll), stochastic production rules, branch radius decay. Added to `ProceduralAlgorithms.h/cpp`. One-click "Generate Forest" UI in Procedural panel: 4 forest types (Mixed/Dense Conifer/Deciduous Park/Sparse Savanna), area radius, tree/shrub/grass density controls, creates Tree+Shrub+Grass volume entities

### Simulation-Driven Geometry

- ~~**Reaction-Diffusion on Meshes**~~ ✅ — `ReactionDiffusion` class (Gray-Scott model) with 9 presets (MitosisSpots, CoralGrowth, Fingerprints, Leopard, Labyrinth, WormHoles, BubblePacking, Spirals, Custom). Configurable feed/kill rates, diffusion coefficients, sub-stepping, circular/random seeding, bake-to-RGBA8 texture and heightmap export.
- ~~**Cellular Automata as Geometry**~~ ✅ — `CellularAutomataGeometry` class with 7 CA rules (GameOfLife, HighLife, DayAndNight, Seeds, BriansBrain, Rule110, Diamoeba, Custom). 3 mesh modes: Voxels (greedy face culling), Marching Cubes (256-entry tables), Point Cloud. Classic pattern stamps (Glider, Pulsar, Gosper Glider Gun).
- ~~**Slime Mold Simulation (Physarum)**~~ ✅ — `PhysarumSimulation` class with 50K+ agents, trail map diffusion/decay, 5 presets (ClassicSlime, BranchingNetwork, DenseWeb, Tendrils, Pulsating). Food source placement, bilinear trail sampling, bake-to-RGBA8 and heightmap export.
- ~~**Fluid Simulation as Terrain**~~ ✅ — `FluidTerrainCoupling` system wires FluidSimulation density/velocity grids to TerrainComponent heightmap. Erosion mode (velocity erodes terrain height) and accumulate mode (density builds terrain, e.g. lava). Bidirectional: terrain slope drives fluid flow. Configurable erosion rate, accumulation rate, coupling strength. Files: `FluidTerrainCoupling.h/cpp`
- ~~**Voronoi Fracture with Persistent Physics**~~ ✅ — VoronoiMeshFracture (Sutherland-Hodgman mesh clipping) produces real mesh fragments. FractureConfigComponent for per-entity config (fragment count, explosion force, persistence, re-fracture depth, pre-fracture with breakable joints, auto-cleanup). DestructibleSystem extended with `CreatePersistentFragments()` — fragments become full ECS entities (Transform+Mesh+Material+Rigidbody+BoxCollider). FIFO fragment entity limit, recursive re-fracture with reduced count per depth level. Editor inspector UI and full serialization

### Simulation & Flow

- ~~**Curl Noise Flow Fields for Particle/Mesh Advection**~~ ✅ — Header-only CurlNoise3D math (finite differences of FBM3D, 12 FBM calls per sample). CurlNoiseFieldComponent with AABB volume, falloff (None/Linear/Smooth), configurable octaves/frequency/amplitude/seed/timeScale. CurlNoiseSystem applies divergence-free forces to particles within volume. Editor inspector with debug arrow visualization and full serialization
- ~~**Wave Racer 64 Water**~~ ✅ — Interactive water with spring-damper wave propagation, splashes/wakes/buoyancy, boat/object interaction. `InteractiveWaterComponent` wired in PlayMode, Player, editor inspector, and full serialization
- ~~**Mesh Audio Reactivity via FFT Vertex Displacement**~~ ✅ — Cooley-Tukey FFT analysis, bass/mid/treble frequency bands mapped to per-vertex mesh displacement. 4 mapping modes. `AudioReactiveMesh` component

### Mathematical & Exotic Geometry

- ~~**Fourier Transform Meshes**~~ ✅ — `FourierMesh` class: DFT decomposition of 2D contours, progressive reconstruction animation, 3D extrusion. Epicycle visualization
- ~~**Non-Euclidean Geometry Rendering**~~ ✅ — Portal rendering with stencil recursion, hyperbolic/spherical/toroidal space warping. Impossible geometry for puzzle games and horror
- ~~**Stereographic Projection of 4D Objects**~~ ✅ — `Projection4D` class: 5 polytopes (tesseract, 120-cell, 600-cell, 24-cell, 16-cell), 6 rotation planes (XY/XZ/XW/YZ/YW/ZW), stereographic 4D→3D projection

### Inverse & Advanced Rendering

- ~~**Inverse Rendering / Differentiable Rendering**~~ ✅ — Gradient descent parameter optimization. Optimize scene parameters (materials, lights, camera) to match target images. Compute shader forward pass with parameter gradients, iterative optimizer

---

## Asset Libraries ✅ COMPLETE

Goal: Ship a curated, commercially licensable library so users have beautiful assets out of the box — important for a licensable engine.

### Font Library ✅ COMPLETE

42 curated OFL/Apache-licensed fonts across 8 categories (Sans-Serif, Serif, Monospace, Display, Handwriting, Pixel, Fantasy, Sci-Fi). `FontLibrary.h/cpp` + `AssetLibrary.h/cpp`. Editor browser in Editor Settings > Fonts with search, category filter, and install status.

- Categories: Sans-serif (UI/HUD), Serif (narrative/books), Monospace (code/terminal), Display/decorative (titles/logos), Handwriting/script, Pixel/retro, Fantasy/medieval, Sci-fi/futuristic
- Sources: Google Fonts (OFL), Font Squirrel (verified commercial licenses), The League of Moveable Type

### 3D Asset Library ✅ COMPLETE

16 CC0 3D model packs (Kenney, Quaternius) with editor browser. `AssetLibrary.h/cpp`.

- Categories: Architecture, Nature, Props, Characters, Vehicles, Weapons, Dungeon, Sci-Fi
- Sources: Kenney.nl (CC0), Quaternius (CC0)

### 2D Asset Library ✅ COMPLETE

15 CC0 2D sprite/tileset/UI packs with editor browser. Uses shared `AssetLibrary.h/cpp`.

- Categories: UI Kits, Tilesets, Sprites, VFX, Backgrounds, Textures, Characters, Props, Architecture, Nature, Vehicles, Weapons, Dungeon, Sci-Fi
- Sources: Kenney.nl (CC0), OpenGameArt.org (curated CC0)

### Licensing Strategy

- All bundled assets must be **CC0 or OFL/Apache 2.0** — no CC-BY-NC, no CC-BY-SA (share-alike creates friction for commercial games)
- Engine license grants full sublicense rights — users can ship assets in their games without attribution requirements
- Clear `LICENSE.md` per asset category documenting provenance and license terms
- Consider commissioning original CC0 assets for a distinctive "Enjin style" that differentiates from Unity/Unreal default look

---

## UI/UX Design Philosophy

- Aesthetically accessible, clean, forward-thinking, timeless
- Own identity (not Unity grey, not Unreal dark, not Apple-style)
- Information-dense but not cluttered
- Consistent patterns (context menus, drag behavior, property editing)

---

---

## Known Stubs & Incomplete Features (Audit #4, 2026-02-14, verified 2026-02-14)

These are documented limitations that require larger feature work. Sorted by priority.

### HIGH (require significant implementation)

| Feature | Files | Status | Notes |
|---------|-------|--------|-------|
| ~~OIT Composite Shader~~ | ~~`OITManager.cpp`~~ | ~~✅ DONE~~ | ~~Fullscreen triangle composite pipeline with embedded SPIR-V (`oit_composite.frag` + `fullscreen.vert`), alpha blending over opaque scene, dynamic viewport/scissor. `CompositePass()` draws fullscreen triangle with accumulation/revealage descriptor set.~~ |
| ~~HTTP Client (Linux/Mac)~~ | ~~`HTTPClient.cpp`~~ | ~~✅ DONE~~ | ~~Full libcurl implementation (`#elif defined(ENJIN_HAS_CURL)`) with Get/Post/PostForm, 16MB response cap, percent encoding. CMake includes macOS in UNIX branch. Cross-platform: WinHTTP on Windows, libcurl on Linux/macOS.~~ |
| ~~SWF Zlib Decompression~~ | ~~`SWFLoader.cpp`~~ | ~~✅ DONE~~ | ~~Uses stb_image's built-in zlib decompressor (`stbi_zlib_decode_buffer`). CWS files fully supported. 256MB size cap. No external zlib dependency needed.~~ |
| ~~Network Auth & Replay~~ | ~~`NetworkSystem.cpp`~~ | ~~✅ DONE~~ | ~~Full HMAC-SHA256 (`NetworkSecurity.h`), session key exchange, 64-bit sliding replay window, constant-time verify, per-connection tracking. BCryptGenRandom (Windows) / /dev/urandom (POSIX) for key generation.~~ |
| ~~Network Audit Fixes~~ | ~~`NetworkSystem.cpp` + 4 files~~ | ~~✅ DONE~~ | ~~16 fixes from comprehensive audit: per-packet RTT timestamps (128-entry ring buffer), reliable ack sequence fix on retransmit, packet loss computation (32-packet sliding window), `inet_ntop` thread safety, session key sender validation + duplicate rejection, HeartbeatAck handler, player ID recycling, rotation/scale delta compression, WSA refcount, RPC forward validation, stale entity cleanup, dead code removal.~~ |

### MEDIUM (infrastructure gaps)

| Feature | Files | Status | Notes |
|---------|-------|--------|-------|
| ~~DoF & Tilt-Shift~~ | ~~`PostProcessing.cpp`~~ | ~~✅ DONE~~ | ~~GPU shader implementation complete: 16-tap Poisson disc DoF with CoC weighting, tilt-shift 25-tap blur, depth linearization with camera near/far planes, debug CoC visualization. Depth image flags fixed (SAMPLED_BIT + STORE_OP_STORE), depth barrier in Apply(). SPIR-V compiled and embedded (16871 words).~~ |
| ~~SH Light Probe Baking~~ | ~~`SHLightProbe.cpp`~~ | ~~✅ DONE~~ | ~~Baking works (2048 stratified samples, L2 SH). Now wired to renderer: `shProbeIrradiance` field in LightingUBO, queried via `GetIrradiance()` at camera position in `UpdateFrameUniforms()`, blended with ambient in `triangle.frag`. All 9 shader UBO layouts updated.~~ |
| ~~OIDN GPU→CPU Copy~~ | ~~`OIDNDenoiser.cpp`~~ | ~~✅ DONE~~ | ~~`RegisterImageMapping()` now called for all 4 RT effect outputs (shadow R16F, reflection RGBA16F, AO R16F, GI RGBA16F) and dummy image in `InitializeRayTracing()`. OIDN denoiser can resolve VkImageView→VkImage for staging copies.~~ |
| ~~ASTC Compression~~ | ~~`TextureCompressor.cpp`~~ | ~~✅ Improved~~ | ~~Weighted representative color using PCA endpoint cluster membership (replaces identical if/else branches). 4x4 Bayer ordered dithering reduces visible block boundaries between adjacent blocks. Still void-extent encoding (full ASTC encoding would require 200+ lines).~~ |
| ~~UI Phase 2+ Widgets~~ | ~~`UISystem.cpp`~~ | ~~✅ ALL 9 DONE~~ | ~~Dropdown, TextInput, RadioGroup, ScrollArea, Grid, TabGroup, Tooltip, Modal, ListView — all fully rendered with complete interaction (keyboard, mouse, focus navigation, accessibility). Grey box placeholder only used as default fallback for hypothetical future widgets.~~ |
| ~~Water3D Rendering~~ | ~~`Water.h/cpp`~~ | ~~✅ DONE~~ | ~~WaterVolumeComponent renders via standard Vulkan forward pipeline. Dedicated shader logic in `triangle.frag` (FLAG_WATER_SURFACE): rain ripples, Fresnel reflection, shore foam, freeze transitions, shallow/deep color blending. 4 water type presets (Ocean/River/Pond/Lake). InteractiveWater (Wave Race 64-style) now fully wired: PlayMode, Player, editor inspector UI, Add Component menu, full serialization.~~ |

### LOW (cosmetic/documentation)

| Item | Notes |
|------|-------|
| ~~RenderSystem missing getters~~ | ~~✅ DONE — Added 6 getters: GetCamera, GetMainPassViewports, GetMainPassWeather, GetMainPassWeatherIsRain, GetOnionSkinGhosts, GetFluidRenderer. Full API symmetry.~~ |
| ~~5 ECS systems SetEnabled/IsEnabled~~ | ~~✅ ALL DONE — TweenSystem, StateMachineSystem, DialogueSystem, VisualScriptSystem, BehaviorTreeSystem all have SetEnabled/IsEnabled~~ |
| ~~Player/PlayMode code duplication~~ | ~~✅ DONE — Refactored into `GameplayLoop.h/cpp` shared module (ProcessContactDamage, ProcessPickup, UpdateHealthSystems, FlushDeferredDestroys, DispatchCollisionEvents3D, Wire2DCollisionCallbacks). Zero duplication.~~ |
| ~~VSync toggle~~ | ~~✅ DONE — Deferred VSync change via `RequestVSyncChange()` applied at EndFrame(). Safe mid-frame toggle, checkbox fully enabled.~~ |
| ~~Apply to Prefab button~~ | ~~✅ DONE — Uses `PrefabManager::CreateFromEntity()` + `SavePrefab()` to write modified entity back to prefab file.~~ |
| ~~Shader Graph Parallax/Flipbook nodes~~ | ~~✅ DONE — Parallax: steep POM with occlusion interpolation (64-step bounded loop, height map sampler). Flipbook: frame-based UV offset with row/col/frame inputs. Inspector UI for both.~~ |
| ~~Shader Graph SceneColor/SceneNormal/SceneDepth/StaticSwitch nodes~~ | ~~✅ DONE — 4 new node types for deferred data access and conditional branching. Type mismatch validation in GenerateGLSL() reports errors for incompatible pin connections.~~ |
| ~~Particle Graph renderer inspector~~ | ~~✅ DONE — Billboard: texture, mode, sort, blend, size, color. Mesh: path, texture, scale, alignment, color. Trail: width, texture mode, vertex distance, start/end color. 15 new fields, compiler mapping, save/load serialization.~~ |
| ~~Flash Timeline SWF sprite import~~ | ~~✅ DONE — BuildTimeline() is 267 lines, production-ready. Frame-by-frame property tracks, removal keyframes, color transforms, frame labels.~~ |
| ~~Template Thumbnail Auto-Capture~~ | ~~✅ DONE — Vulkan framebuffer readback via staging buffer (BGRA→RGBA swizzle), downscale to 280x180, stbi_write_png. Auto-captures game view on template save.~~ |
| ~~Profiler P50/P95/P99 + CSV Export~~ | ~~✅ DONE — Frame time percentiles computed and displayed in stats overlay. Descriptor cache hit/miss tracking with hit rate percentage. CSV export button (perf_stats.csv).~~ |
| ~~MIDI Input~~ | ~~✅ DONE — Platform-specific MIDI input (WinMM on Windows, stubs elsewhere). Device enumeration, open/close, double-buffered event polling, persistent CC state. 12 AngelScript bindings. Wired in PlayMode and Player.~~ |

*Last updated: 2026-02-16 — Session 30: Networking audit (16 fixes): per-packet RTT, reliable ack fix, packet loss tracking, inet_ntop, session key validation, HeartbeatAck handler, player ID recycling, delta compression, WSA refcount, RPC forward validation. All 10 targets build clean.*

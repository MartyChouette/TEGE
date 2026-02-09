# Enjin Engine Technical Roadmap

This document captures detailed technical plans, performance findings, and strategic initiatives identified through codebase audits. It complements CLAUDE.md's feature roadmap with implementation-specific details.

---

## Performance Optimization Findings

### Critical Rendering Pipeline Issues

These issues cause frame hitches and should be addressed first.

#### 1. ~~GPU Synchronization Blocking~~ ✅ RESOLVED

Replaced `vkDeviceWaitIdle()` with per-frame fence waits. Pipeline recreation deferred to next frame start.

#### 2. ~~Entity Iteration Inefficiency~~ ✅ RESOLVED

Replaced all `GetAllEntities()` + filter patterns with `GetEntitiesWithComponent<T>()` across render, shadow, and flower systems.

#### 3. ~~Per-Entity Texture Lookups~~ ✅ RESOLVED

Added `cachedBaseColorTexture`, `cachedHeightTexture`, `cachedNormalTexture`, `cachedMetallicRoughnessTexture`, `cachedEmissiveTexture` on `MaterialComponent`. Cache invalidated via `InvalidateTextureCache()` on path changes.

### Medium Priority Optimizations

#### 5. ~~Redundant Component Lookups~~ ✅ RESOLVED

Added `GetEntitiesWithComponents<T1, T2>()` multi-component query to World. Replaced `HasComponent() + GetComponent()` double lookups with single `GetComponent()` + null check in ControllerSystem (all 6 controller loops + FollowTarget + LookAtTarget) and SimplePhysics. Extracted `GetColliderInfo()` helper in SimplePhysics to replace repeated 3-way Box/Sphere/Capsule discrimination across 6+ locations.

#### 6. ~~String-Based Entity Lookups in Scripts~~ ✅ RESOLVED

Added `m_NameCache` (`unordered_map<string, Entity>`) to World with lazy rebuild on `FindEntityByName()`. Cache invalidated on entity destroy, world clear, and name changes. `Scene_FindEntity()` now O(1) via `FindEntityByName()`.

#### 7. ~~Vector Allocations Without Reserve~~ ✅ RESOLVED

Added `reserve()` calls before all particle spawn loops in FlowerSystem: `SpawnBreakParticles()`, `SpawnGroundSplash()`, `SpawnTensionDrip()`.

### Data Structure Improvements

| Current | Recommended | Location | Status |
|---------|-------------|----------|--------|
| ~~`std::map<string, string>`~~ | ~~`std::unordered_map`~~ | ~~DialogueTree.h:106, Gameplay.h:883~~ | ✅ RESOLVED |
| ~~`unordered_map<Entity, RenderData>`~~ | ~~Dense vector indexed by entity~~ | ~~RenderSystem.h~~ | ✅ RESOLVED — `std::vector<EntityRenderData>` indexed by entity ID with `valid` flag, O(1) direct lookup |
| ~~String-keyed texture cache~~ | ~~Integer-keyed or pointer cache~~ | ~~RenderSystem.h~~ | ✅ RESOLVED — `m_TexturePathToId` (path→u32 ID, only on first load) + `m_TextureById` (dense vector), eliminates per-frame string hashing |

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

### Future Graph Systems ✅ COMPLETE (Skeleton)

| System | Effort | Use Case | Status |
|--------|--------|----------|--------|
| Shader Graph | 6-8 weeks | Visual shader authoring, GLSL generation | ✅ Skeleton (node types + editor shell) |
| Audio Event Graph | 2-3 weeks | Dynamic audio mixing based on game state | ✅ Skeleton (node types + editor shell) |
| Particle System Graph | 2-3 weeks | Sub-emitter chains, complex particle systems | ✅ Skeleton (node types + editor shell) |
| Procedural Generation Graph | 4-6 weeks | Visual WFC/L-system/BSP rule composition | Planned |

---

## GUI Modernization Plan

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
4. **Polish (1-2 weeks):** Empty states, loading indicators, tooltips

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

### Template Marketplace (Future)

- Pre-wired behavior trees, dialogue trees, quest flows
- Downloadable game mechanics (inventory system, shop system)
- Community-shared visual scripts

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

## Editor Tools & UX

### Pending

- ~~**Extended Model Format Support**~~ ✅ — PLY (ASCII/binary point cloud/mesh) and VOX (MagicaVoxel voxel with greedy face merging) import via custom loaders, routed through SceneImporter
- **Template Rebuild & Demo Scenes** — Update all 30 templates to latest features, add demo scene per template with "Demo" button
- **Planet Gravity Template** — Super Mario Galaxy-style spherical gravity third-person platformer (PlanetGravityZone, SurfaceAlignedController, orbit camera)
- **Editor Accent Color & Theming** — ~~Replace blue accent with TEGE brand sage green~~ (done), ~~customizable accent colors in editor settings~~ (done), rounded corners, softer panel borders, distinct visual identity
- ~~**Curved Grid Snapping**~~ ✅ — Snap entity placement to curved/spherical grid surfaces with orientation alignment. Surface Snap mode projects entities onto terrain heightmaps and sphere gravity zones, with normal alignment (yaw-preserving) and settings persistence. `Quaternion::FromToRotation()` utility added
- **Improved Icon/Window Inspector** — Entity icons in hierarchy, component icons in inspector, window icon picker in project settings
- ~~**Asset Browser Panel**~~ ✅ — Grid/list view toggle with cached directory listing, image thumbnails via texture cache, search/filter bar, hover tooltip with 256px preview, drag source for future drag-to-viewport, type-colored labels (3D/SCN/SHD/IMG/AS/SFX/PFB), adjustable thumbnail size

### Partially Complete

- ~~**Project Hub & Creation Wizard**~~ ✅ — All 4 tabs (Recent/New/Open/Demos), 30 templates with category filtering and search, git init option, custom templates, folder structure auto-creation, template hover preview all done
- ~~**Undo/Redo**~~ ✅ — Entity operations, visual script node edits, inspector property edits, tilemap paint (per-stroke with cell deduplication), terrain sculpt (heightmap+splatmap snapshot), UI editor edits (move/resize/nudge/delete) all done
- ~~**Drag and Drop**~~ ✅ — OS file drop, hierarchy reparenting, asset browser to Game View (model/prefab/image/scene/audio/script dispatch), material inspector texture fields, sprite inspector texture fields all done
- **Asset Import Pipeline** — Import settings dialog, .enjinasset metadata, and asset browser drag-import done. Remaining: thumbnails, texture compression, source-app import presets

### Planned: Source-App Import Presets

Smart import presets for common DCC tools with automatic axis/scale/material fixups:

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

## Runtime Systems (Planned)

- ~~**Improved Physics**~~ ✅ — 2D physics (Box2D-style): PhysicsWorld2D with circle/box/polygon shapes, 5 joint types (revolute, prismatic, distance, rope, weld), CCD, physics materials (friction, restitution, density), 2D raycasts/overlap queries, impulse-based collision resolution with SAT, collision enter/exit callbacks, bitmask filtering
- **Basic Networking** — Client-server architecture, state sync, entity ownership, lobbies, RPCs, lag compensation (LAN first, then relay)
- ~~**Destructible Environments**~~ ✅ — DestructibleSystem with 4 fracture patterns (Voronoi, Grid, Radial, Shatter), debris spawning with physics (velocity, gravity, angular velocity, lifetime), chain destruction propagation with radius/delay/falloff, per-entity FractureConfig, health-based damage triggers
- **Simple Fluid Simulation** — ~~Grid-based Eulerian fluid (water, lava, gas). FluidVolumeComponent with preset configs. Target: 64x64 2D / 32x32x32 3D at 60fps~~ ✅ Stable Fluids solver (Jos Stam), 5 presets (Water/Lava/Gas/Smoke/Steam), GPU instanced cell renderer, full editor integration
- **SVG Support** — ~~nanosvg parsing, rasterize-to-texture via SVGLoader, GetOrLoadTexture routing for .svg files~~ ✅. ~~SDF vector rendering~~ ✅. Remaining: UIElement Image widget integration
- ~~**Dialogue System Future Work**~~ ✅ (partial) — .enjdlg asset files (DialogueAsset save/load with versioned JSON), LocalizationManager singleton (string key → locale tables, CSV/JSON import/export, parameterized strings with {key} substitution, runtime locale switching, LOC() macro). Remaining: UICanvas dialogue box, Yarn Spinner/Twine import/export

---

## Rendering Pipeline & Performance

### Recently Completed

- **GPU Frustum Culling** — Integrated into render pipeline, skips off-screen entities before draw calls
- **Shadow Pipeline Overhaul** — Back-face culling in shadow pass with pipeline depth bias (CSM 0.75/0.75, point 0.5/0.5, spot 0.5/0.5), removed shader-side bias. Fixes ring-of-light under curved objects. Correct cascade frustum computation with world-space ray interpolation
- **Per-Entity Shadow Dither** — 3 modes (by darkness, distance, angle) using Bayer 4x4 matrix, packed in flag bits 14-15
- **receiveShadows Flag** — Now checked in shader; entities can opt out of receiving shadows
- **Shadow Caster Caching** — Pre-filtered shadow caster list avoids redundant iteration per cascade

### Recently Completed (cont.)

- **Sprite Texture Atlas** — Runtime shelf-packing of sprite textures (<=512px) into a single 4096x4096 GPU texture. Sprites sharing the atlas batch into one instanced draw call via `"__atlas__"` sentinel key. Per-instance UVs linearly remapped into atlas regions. Lazy rebuild on new textures, invalidation on texture hot-reload. Oversized/failed textures excluded and fall back to individual draw calls
- **Soft Shadows (Poisson Disk PCF)** — 16-sample Poisson disk PCF with configurable shadow softness radius. Applied to directional (CSM), point, and spot light shadows
- **Point/Spot Light Shadow Maps** — Cubemap array depth maps for up to 4 point lights (1024² per face, 6 faces each), 2D array depth maps for up to 4 spot lights (1024²). Shadow data SSBO (binding 12), new descriptor bindings 10-12. Shadow-casting light selection by intensity/distance² scoring. Soft shadows via 3D tangent-frame Poisson disk for point lights, standard 2D Poisson for spot lights
- **3D/2D Pipeline Audit** — Auto-disable shadow pass for 2D-only scenes (`ClassifySceneComposition()` gates all shadow passes by `Scene3D`), sprite batching by texture atlas, ortho/perspective camera mixing diagnostic (every 300 frames), Scene2D fast paths (skip normal map descriptor for unlit sprites, early-out of `UpdateFrameUniforms()` light iteration)
- **Editor Frustum Culling Fix** — GPU frustum culling now disabled in editor mode (`SetEditorMode(true)`) so all entities are visible in the scene view for editing. Player builds retain frustum culling against the game camera. Guards on `BuildCullableObjectList()`, culling dispatch, and all `IsVisible()` skip checks
- **Inspector Undo/Redo** — `InspectorUndo.h` with `PropertyEditCommand<T>` template and drop-in ImGui widget wrappers (DragFloat, DragFloat3, SliderFloat, SliderInt, DragInt, ColorEdit3, Checkbox, Combo, InputText, InputTextMultiline). Continuous widgets snapshot on `IsItemActivated()` and push one undo entry on `IsItemDeactivatedAfterEdit()`. All 60+ `Draw*Component` functions in EditorLayer converted (~500+ widget calls)

### Pipeline Optimization ✅ COMPLETE

All pipeline optimization items resolved: multi-threaded command buffer recording, GPU payload batching (sort by pipeline/material), indirect rendering (VkCmdDrawIndexedIndirect), async compute for culling/particles/post-process, frame graph resource scheduling, Hi-Z culling.

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
  - DiamondSquare (heightmap terrain, midpoint displacement, roughness control)
  - LSystemGenerator (string rewriting with turtle graphics interpretation, F/+/-/[/] commands)
  - WaveFunctionCollapse (tile-based with adjacency constraints, entropy-based collapse, backtracking)
  - VoronoiGenerator (Euclidean/Manhattan/Chebyshev distance, region ID grid)
  - GrammarGenerator (shape grammars for buildings, weighted rule selection)
  - PrefabAssembler (snap-together rooms with directional connection points)
  - Editor panel (EditorPanel `1<<24`): algorithm dropdown, parameter sliders, seed input, 256x256 ImGui canvas preview, "Apply to Tilemap" button
- ~~**Custom Flora Assets**~~ ✅ — `customAssetPath` field on GrassVolumeComponent and ShrubVolumeComponent (TreeVolumeComponent already had texture paths). Browse/clear buttons in inspector. Serialization support.

---

## Scripting & Extensibility

- ~~**Component/Plugin DLL Repositories**~~ ✅ RESOLVED — Plugin repository system with catalog browsing, search/filter by category, install/uninstall, version comparison, `repository.json` format, persistent source management. Extended `PluginManifest` with author/category/tags fields. Editor Plugin Browser panel.
- ~~**Documentation Generator**~~ ✅ RESOLVED — `DocGenerator` auto-generates markdown from component headers (field parser), AngelScript bindings (via `asIScriptEngine` enumeration), visual script nodes (from `NodeRegistry`), and data asset schemas. Outputs COMPONENTS.md, SCRIPTING_API.md, VISUAL_SCRIPT_NODES.md, DATA_ASSETS.md, INDEX.md to `docs/generated/`. Accessible via Tools menu.
- ~~**ScriptableObject / DataAsset System**~~ ✅ RESOLVED — `DataAssetRegistry` singleton with schemas (`.enjschema`) and assets (`.enjdata`) JSON I/O. 8 field types (String, Float, Int, Bool, Vector3, Vector4, StringArray, FloatArray). AngelScript bindings (`DataAsset_Load/GetFloat/GetInt/GetBool/GetString/GetVector3`). 3 visual script nodes (`DataAsset_Load`, `DataAsset_GetFloat`, `DataAsset_GetString`). Editor Data Asset panel with schema editor, asset browser, inline field editing.

---

## Platform & Export

### Desktop Platforms

#### Linux ✦ Planned
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

#### Steam Deck ✦ Planned
- **Render backend:** Vulkan (native — AMD RDNA2 GPU, same as desktop Linux)
- **Input:** Steam Input API integration for trackpad, gyroscope, back paddles, haptic feedback. Fallback to standard gamepad via SDL/GLFW
- **Display:** 1280x800 (Deck LCD) / 1280x800 HDR (Deck OLED) target resolution, 40-60fps adaptive framerate
- **Performance profiles:** Configurable TDP-aware quality presets (Low/Medium for 15W, High for 25W)
- **Steamworks integration:** Steam overlay, cloud saves, achievements, Rich Presence, workshop
- **Dev API stubs:** `PlatformSteamDeck.h` — `SteamInput`, `SteamCloud`, `SteamOverlay`, gyro aim assist, on-screen keyboard, suspend/resume lifecycle
- **Testing:** Proton compatibility layer validation (for Windows builds running on Deck), native Linux build preferred
- **Build target:** Linux x86_64 binary + Steam app manifest, verified Deck compatibility badge requirements

### Console Platforms

#### Nintendo Switch (Original) ✦ Planned (requires licensed devkit)
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
| Linux | P1 | Shared Vulkan backend, GLFW/miniaudio already cross-platform, Steam Deck prerequisite |
| Steam Deck | P1 | Largest handheld PC market, Linux + Steam Input, no devkit needed |
| macOS | P2 | MoltenVK translation layer, Apple Silicon support needed, code signing complexity |
| Xbox Series X\|S | P2 | GDK freely available via ID@Xbox, D3D12 backend required |
| PlayStation 5 | P3 | Requires SIE partnership, proprietary AGC graphics API, high cert bar |
| Nintendo Switch 2 | P3 | NDA SDK, Vulkan 1.3 expected — closer to desktop pipeline |
| Nintendo Switch 1 | P4 | NVN proprietary API, severe memory constraints, aging hardware |

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

## Accessibility (Engine-Level)

The editor itself must be fully accessible:

- **Screen Reader Support** — OS accessibility APIs (UI Automation, AT-SPI, NSAccessibility)
- **Keyboard-Only Navigation** — Full editor operation without mouse, tab-order, focus indicators
- **Alternative Input Devices** — Switch access, eye tracking, sip-and-puff
- **Motor Accessibility** — Adjustable click/drag thresholds, sticky keys, dwell-click, one-handed presets
- **Visual Accessibility** — Configurable font sizes, icon scaling, custom accent colors, reduced transparency
- **Audio Accessibility** — Visual indicators for all audio feedback in editor
- **Blind-Accessible Workflow** — Screen reader + keyboard investigation, text-based/CLI interface for core operations

---

## Version Control & Collaboration

- **Git Integration** — Built-in git panel (stage, commit, push, pull, branch, merge), visual scene diff (structured JSON), optional `git init` on project creation
- **Scene & Entity Locking** — Advisory locks for multi-user workflows (`.enjinlock`), entity-level locking with visual indicators
- **Collaborative Editing** — Real-time or turn-based with session locks. Future: OT/CRDT-based scene sync with entity-level conflict resolution
- **Clean Git Serialization** — Deterministic scene files (sorted keys, stable ordering, no floating-point drift)

---

## Flash Game Revival & Retro Web Game Support

Target audience: Flash game creators and fans of the Flash/Newgrounds era looking for modern tooling.

### SWF Import & Conversion
- **SWF parser** — Parse SWF file format (shapes, sprites, timeline, ActionScript bytecode). Convert vector shapes to SVG paths (via nanosvg rasterization) or native sprite sheets
- **Timeline-to-Animation** — Map SWF timeline keyframes to `AnimatedSprite2DComponent` frame sequences. Support tweened motion (position, scale, rotation, alpha) via the existing tween system (25 easing functions)
- **MovieClip mapping** — Convert Flash MovieClip hierarchy to entity hierarchy with nested `AnimatedSprite2DComponent` instances. Symbols → prefabs

### Flash-Style Authoring Workflow
- **Frame-based timeline editor** — Flash-like timeline panel with layers, keyframes, and tweens. Extends the existing Animation/Sequencer system with per-frame scrubbing, onion skinning (already implemented in PixelEditor), and shape tweening preview
- **Vector drawing tools** — Basic shape primitives (rect, ellipse, line, pen/bezier) rendered via SVG pipeline. Export to sprite sheets for runtime. Complements the existing PixelEditor for bitmap art
- **Symbol library** — Reusable graphic symbols stored as prefabs. Drag from library to stage. Nested symbol editing (like Flash's "Edit Symbol" mode). Built on existing PrefabManager

### ActionScript Compatibility Layer
- **AS2/AS3 → AngelScript transpiler** — Convert common ActionScript patterns to AngelScript equivalents. `MovieClip.gotoAndPlay()` → `Animator_Play()`, `addEventListener` → `Events_Listen()`, `Mouse.hide()` → `Input_HideCursor()`
- **Flash API shim library** — AngelScript library providing familiar Flash APIs: `Stage`, `MovieClip`, `TextField`, `Mouse`, `Keyboard`, `Sound`, `SharedObject` (→ SaveSystem), `ExternalInterface` (→ plugin/modding hooks)
- **Newgrounds.io integration** — Medal/scoreboard API bindings via HTTP (future networking layer). `SharedObject` mapped to `SaveSystem` slots

### Flash Game Templates
- **Starter templates** — Pre-built project templates for common Flash game genres: point-and-click adventure, dress-up game, tower defense, bullet hell, rhythm game, escape room, idle/clicker
- **Newgrounds-style game page** — Built-in HTML5 export template with play button, preloader, fullscreen toggle, and embed code generation

### WebAssembly Export (Prerequisite)
- **WebGPU/WebAssembly target** — Compile engine to WASM with WebGPU renderer backend. Required for browser-based game distribution
- **Emscripten integration** — Asset pack loading via Fetch API, input mapping for touch/mouse, audio via Web Audio API
- **Size optimization** — Tree-shaking unused systems, texture compression (Basis Universal), audio compression (Opus), code splitting for progressive loading

---

## UI/UX Design Philosophy

- Aesthetically accessible, clean, forward-thinking, timeless
- Own identity (not Unity grey, not Unreal dark, not Apple-style)
- Information-dense but not cluttered
- Consistent patterns (context menus, drag behavior, property editing)

---

*Last updated: 2026-02-09*

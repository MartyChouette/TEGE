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

#### 5. Redundant Component Lookups

**Problem:** Multiple `GetComponent()` calls for same entity in single loop iteration.

**Example (RenderSystem.cpp:2505-2700):**
```cpp
// 8+ GetComponent calls per RenderEntity(), even if only 1-2 present
TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
Sprite2DComponent* spriteComp = m_World->GetComponent<Sprite2DComponent>(entity);
TilemapComponent* tilemapComp = m_World->GetComponent<TilemapComponent>(entity);
VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
// ... more
```

**Solution:**
- Use multi-component queries: `GetEntitiesWithComponents<Transform, Mesh>()`
- Cache component pointers at loop start
- Only fetch optional components that actually exist

#### 6. String-Based Entity Lookups in Scripts

**Problem:** `Scene_FindEntity()` does O(n) scan through all entities.

**Location:** ScriptBindings_Scene.cpp:101-116

**Solution:**
- Add optional name cache in SceneManager: `std::unordered_map<std::string, Entity>`
- Update cache on entity create/delete/rename
- Cache invalidation on scene load

#### 7. Vector Allocations Without Reserve

**Problem:** `push_back()` without `reserve()` causes reallocations.

**Location:** FlowerSystem.cpp:773,795,830,875 (particle spawning)

**Solution:**
```cpp
// At FlowerSystem initialization or Update() start:
m_Particles.reserve(MAX_PARTICLES);
```

### Data Structure Improvements

| Current | Recommended | Location | Reason |
|---------|-------------|----------|--------|
| `std::map<string, string>` | `std::unordered_map` | DialogueTree.h:106, Gameplay.h:883 | O(1) vs O(log n) lookup |
| `unordered_map<Entity, RenderData>` | Dense vector indexed by entity | RenderSystem.h:376 | Better cache locality |
| String-keyed texture cache | Integer-keyed or pointer cache | RenderSystem.h:287 | Avoid string hashing |

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

**Remaining work (Phase 5+):**
- Physics nodes (Raycast, AddForce, AddImpulse)
- Subgraph/function nodes (reusable node groups)
- AngelScript interop (call script functions from visual script)
- Conditional breakpoints
- Watch window for variable inspection
- Call stack view

#### Phase 3: AI Behavior Tree Editor (2-3 weeks)

**Why:** Behavior trees are the industry standard for game AI (AAA titles). Current AI is state-based; trees enable richer decision hierarchies.

**Node Types:**
- Selector (OR logic, first success wins)
- Sequence (AND logic, all must succeed)
- Parallel (all run simultaneously)
- Leaf (action: Move, Attack, Patrol)
- Decorator (invert, repeat, timeout)

#### Phase 4: Quest Flow Editor (2 weeks)

**Why:** Essential for RPG/narrative games. QuestSystem backend exists but lacks visual authoring.

**Node Types:**
- Quest Start
- Objective (sequential or parallel)
- Condition (player level, inventory check)
- Reward (item, XP)
- Branch (quest branch points)
- End

### Future Graph Systems (Lower Priority)

| System | Effort | Use Case |
|--------|--------|----------|
| Shader Graph | 6-8 weeks | Visual shader authoring, GLSL generation |
| Audio Event Graph | 2-3 weeks | Dynamic audio mixing based on game state |
| Particle System Graph | 2-3 weeks | Sub-emitter chains, complex particle systems |
| Procedural Generation Graph | 4-6 weeks | Visual WFC/L-system/BSP rule composition |

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

### Micro-Interactions

**Spring Easing:** `cubic-bezier(0.175, 0.885, 0.32, 1.275)` for button press bounce

**Transitions:**
- Hover: 100ms smooth fade
- State change: 200ms spring
- Focus: 150ms with glow shadow

**Implementation in ImGui:**
- Color interpolation over frames for transitions
- Spring physics for position/scale animations
- Glow via shadow render pass

### Empty States

Design pattern for panels with no content:
- Centered icon (64x64px, outlined, 40% opacity)
- Heading: "No [Items]"
- Body: "Create one to get started"
- Optional CTA button

### Implementation Phases

1. **Foundation (1-2 weeks):** Color tokens, font hierarchy, spacing constants
2. **Core Panels (2-3 weeks):** Hierarchy, Inspector, Viewport styling
3. **Micro-interactions (2 weeks):** Spring easing, hover effects
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
| Visual Scripting (Phase 5+) | High | Medium | P1 | Pending |
| Soft shadows (Poisson disk PCF) | Medium | Medium | P1 | ✅ Complete |
| Sprite batching by texture atlas | High | Medium | P1 | Pending |
| Point/spot light shadows | Medium | High | P1 | ✅ Complete |
| 2D sprite art pipeline | High | High | P1 | Pending |
| AI Behavior Tree Editor | High | Medium | P2 | Pending |
| Quest Flow Editor | High | Low | P2 | Pending |
| GUI color palette update | Medium | Low | P2 | Pending |
| Typography system | Medium | Low | P2 | Pending |
| Multi-threaded command buffer recording | High | High | P2 | Pending |
| Undo/redo for inspector property edits | Medium | Medium | P2 | Pending |
| Asset browser with thumbnails | Medium | Medium | P2 | Pending |
| Micro-interactions | Medium | Medium | P3 | Pending |

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

- **Extended Model Format Support** — PLY (point cloud/mesh) and VOX (MagicaVoxel voxel) import via Assimp or custom loaders
- **Template Rebuild & Demo Scenes** — Update all 30 templates to latest features, add demo scene per template with "Demo" button
- **Planet Gravity Template** — Super Mario Galaxy-style spherical gravity third-person platformer (PlanetGravityZone, SurfaceAlignedController, orbit camera)
- **Editor Accent Color & Theming** — Replace blue accent with TEGE brand `#c7dac4` (sage green), rounded corners, softer panel borders, distinct visual identity
- **Curved Grid Snapping** — Snap entity placement to curved/spherical grid surfaces with orientation alignment
- **Improved Icon/Window Inspector** — Entity icons in hierarchy, component icons in inspector, window icon picker in project settings
- **Asset Browser Panel** — Thumbnail grid/list view of project assets, drag-to-viewport, texture preview, search/filter

### Partially Complete

- **Project Hub & Creation Wizard** — Template search, git init, templates folder done. Remaining: Recent Projects tab, folder structure auto-creation, collaboration-ready layout
- **Undo/Redo** — Entity operations + visual script node edits done. Remaining: inspector property edits, tilemap paint, terrain sculpt, UI editor edits
- **Drag and Drop** — OS file drop + hierarchy reparenting done. Remaining: asset browser to viewport/inspector
- **Asset Import Pipeline** — Import settings dialog and .enjinasset metadata done. Remaining: thumbnails, axis conversion, texture compression, asset browser drag-import

---

## Runtime Systems (Planned)

- **Improved Physics** — 2D physics (Box2D-style), 2D joints, CCD, more shape types, physics materials (friction, bounce), script trigger callbacks
- **Basic Networking** — Client-server architecture, state sync, entity ownership, lobbies, RPCs, lag compensation (LAN first, then relay)
- **Destructible Environments** — Prefab-level destructibility, fracture/shatter mesh splitting, debris physics, chain destruction
- **Simple Fluid Simulation** — Grid-based Eulerian fluid (water, lava, gas). FluidVolumeComponent with preset configs. Target: 64x64 2D / 32x32x32 3D at 60fps
- **SVG Support** — nanosvg parsing, rasterize-to-texture caching, UIElement Image widget integration, SDF vector rendering (future)
- **Dialogue System Future Work** — .enjdlg asset files, localization system (string keys + locale tables), UICanvas dialogue box (replace ImGui overlay), dialogue template, Yarn Spinner/Twine import/export

---

## Rendering Pipeline & Performance

### Recently Completed

- **GPU Frustum Culling** — Integrated into render pipeline, skips off-screen entities before draw calls
- **Shadow Pipeline Overhaul** — Back-face culling in shadow pass with pipeline depth bias (CSM 0.75/0.75, point 0.5/0.5, spot 0.5/0.5), removed shader-side bias. Fixes ring-of-light under curved objects. Correct cascade frustum computation with world-space ray interpolation
- **Per-Entity Shadow Dither** — 3 modes (by darkness, distance, angle) using Bayer 4x4 matrix, packed in flag bits 14-15
- **receiveShadows Flag** — Now checked in shader; entities can opt out of receiving shadows
- **Shadow Caster Caching** — Pre-filtered shadow caster list avoids redundant iteration per cascade

### Recently Completed (cont.)

- **Soft Shadows (Poisson Disk PCF)** — 16-sample Poisson disk PCF with configurable shadow softness radius. Applied to directional (CSM), point, and spot light shadows
- **Point/Spot Light Shadow Maps** — Cubemap array depth maps for up to 4 point lights (1024² per face, 6 faces each), 2D array depth maps for up to 4 spot lights (1024²). Shadow data SSBO (binding 12), new descriptor bindings 10-12. Shadow-casting light selection by intensity/distance² scoring. Soft shadows via 3D tangent-frame Poisson disk for point lights, standard 2D Poisson for spot lights

### Pending

- **3D/2D Pipeline Audit** — Auto-disable shadow pass for 2D-only scenes, sprite batching by texture atlas (biggest 2D perf win), warn on ortho/perspective mixing, flat shading fast path for sprites
- **Pipeline Optimization** — Multi-threaded command buffer recording, GPU payload batching (sort by pipeline/material), indirect rendering (VkCmdDrawIndexedIndirect), async compute for culling/particles/post-process, frame graph resource scheduling, Hi-Z culling

---

## 2D Sprite Art Pipeline

A complete draw-to-game workflow for 2D and 2.5D projects, from pixel art creation through sprite sheets to playable prefabs.

### Built-In Pixel Editor

Minimal but functional sprite editor inside the engine — no external tools needed for prototyping.

- **Canvas sizes:** Freeform + retro resolution presets
- **Tools:** Pencil, eraser, fill, line, rectangle, ellipse, eyedropper, selection/move
- **Layers:** Basic layer stack with visibility/opacity
- **Palette:** Indexed color palettes per preset, custom palette support
- **Animation:** Onion skinning, frame timeline, playback preview
- **Export:** Save as `.png` sheet or individual frames, auto-register as engine asset

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

### Sprite Sheet / Atlas Workflow

- **Import:** Load existing sprite sheets, define frame rects (grid-based or manual)
- **Auto-slice:** Grid-based auto-detection with configurable cell size and padding
- **Atlas packing:** Combine multiple sprites into optimized texture atlases (MaxRects bin packing)
- **Per-frame data:** Each frame can store pivot point, collision shape, and animation timing

### Auto Collider Generation

- **Alpha-based outline:** Trace non-transparent pixels to generate polygon collider (configurable simplification threshold)
- **Bounding shapes:** Auto-fit box, circle, or capsule colliders from sprite bounds
- **Per-frame colliders:** Different collider shapes per animation frame (attack hitboxes, hurt boxes)
- **Spline colliders:** Generate spline-based colliders from sprite silhouette for smooth curves
- **Manual editor:** Click-to-place polygon vertices over sprite preview, snap to pixel grid

### Vector Art / SVG Import

- **SVG import:** Parse SVG via nanosvg, rasterize to target resolution for sprite use
- **Resolution independence:** Store SVG source, re-rasterize at different scales for LOD or resolution changes
- **UI integration:** SVG elements usable directly in UICanvas Image widgets
- **Runtime SVG (future):** SDF-based vector rendering for resolution-independent UI at runtime

### Prefab Output Pipeline

Complete flow from art to playable entity:

1. **Draw** sprite in pixel editor (or import PNG/SVG)
2. **Slice** into frames if sprite sheet
3. **Define** animation sequences (idle, walk, attack, etc.)
4. **Generate** colliders (auto or manual, per-frame)
5. **Configure** material (emission, normal map generation from height)
6. **Save as prefab** — `.enjprefab` with sprite, animations, colliders, material all bundled
7. **Drag into scene** — instantiate as ready-to-play entity

### Implementation Phases

1. **Foundation (2-3 weeks):** Pixel editor canvas with basic tools, preset system, PNG export
2. **Sprite Sheets (2 weeks):** Import, auto-slice, atlas packing, frame data
3. **Colliders (2 weeks):** Alpha trace, bounding shapes, per-frame colliders, manual editor
4. **Pipeline Integration (1-2 weeks):** Prefab output, drag-to-scene, animation hookup
5. **SVG (1-2 weeks):** nanosvg import, rasterize-to-texture, UI widget support

---

## Procedural Generation

- **Procedural Generation Algorithms** — Modular pluggable generators for LevelGenerator:
  - Cellular Automata (cave generation, organic shapes)
  - Random Walkers (dungeon carving, directional bias)
  - Wave Function Collapse (tile-based from examples, adjacency constraints, backtracking)
  - BSP (Binary Space Partitioning room-corridor dungeons)
  - L-Systems (recursive tree/plant/river generation)
  - Voronoi Diagrams (region-based world gen, biome placement)
  - Diamond-Square (heightmap terrain, midpoint displacement)
  - Grammar-Based (shape grammars for buildings/architecture)
  - Modular/Prefab Assembly (snap-together rooms with connection points)
  - Editor UI: Generator panel with algorithm selection, parameter sliders, live preview, seed control
- **Custom Flora Assets** — Drop-in custom images/models for vegetation systems. "Custom Asset" field on volume components with browse/drag-drop/clear.

---

## Scripting & Extensibility

- **Component/Plugin DLL Repositories** — Load gameplay components from external DLLs/shared libs, package format for distribution
- **Documentation Generator** — Auto-generate docs from component definitions, script API, project structure (HTML/markdown)
- **ScriptableObject / DataAsset System** — Unity-like reusable data containers (weapon stats, enemy tables, item definitions), serialized JSON assets, inspector editing

---

## Platform & Export

- **Mobile Support** — Touch input, gyroscope, screen density, mobile render paths. Android (Vulkan) + iOS (MoltenVK)
- **Console Support** — Platform abstraction for console input, certification requirements, console render backends
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

## UI/UX Design Philosophy

- Aesthetically accessible, clean, forward-thinking, timeless
- Own identity (not Unity grey, not Unreal dark, not Apple-style)
- Information-dense but not cluttered
- Consistent patterns (context menus, drag behavior, property editing)

---

*Last updated: 2026-02-07*

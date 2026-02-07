# Enjin Engine Technical Roadmap

This document captures detailed technical plans, performance findings, and strategic initiatives identified through codebase audits. It complements CLAUDE.md's feature roadmap with implementation-specific details.

---

## Performance Optimization Findings

### Critical Rendering Pipeline Issues

These issues cause frame hitches and should be addressed first.

#### 1. GPU Synchronization Blocking (CRITICAL)

**Problem:** `vkDeviceWaitIdle()` causes full GPU-CPU stalls on shader hot-reload and pipeline recreation.

| Location | Trigger | Impact |
|----------|---------|--------|
| RenderSystem.cpp:2204 | `RecreatePipelines()` | Full GPU stall on wireframe toggle, shadow settings change |
| RenderSystem.cpp:2380 | Main shader hot-reload | Multi-frame hitch during editing |
| RenderSystem.cpp:2416 | Skybox shader hot-reload | Multi-frame hitch |
| RenderSystem.cpp:2470 | Shadow shader hot-reload | Multi-frame hitch |
| VulkanRenderer.cpp:270 | `vkWaitForFences()` infinite timeout | No frame skip on slow GPU |

**Solution:**
- Replace `vkDeviceWaitIdle()` with per-frame fence waits
- Defer pipeline recreation to next frame start (after current frame's fence)
- Cache pipeline states to avoid recreation when settings unchanged
- Add timeout-based detection with frame skip logic

#### 2. Entity Iteration Inefficiency (HIGH)

**Problem:** `GetAllEntities()` iterates ALL entities then filters, wasting O(n) iterations.

| Location | Pattern | Waste |
|----------|---------|-------|
| RenderSystem.cpp:593,662 | Main render loop x2 | 5000 iterations for 100 renderables |
| RenderSystem.cpp:2890 | Shadow pass x4 cascades | 20,000 iterations for 100 shadow-casters |
| FlowerSystem.cpp:41,117,889 | Multiple update loops | Full iteration for ~20 flower parts |

**Solution:**
```cpp
// Before (O(n) with filter):
for (Entity e : m_World->GetAllEntities()) {
    if (!m_World->HasComponent<MeshComponent>(e)) continue;
    auto* mesh = m_World->GetComponent<MeshComponent>(e);
    // ...
}

// After (O(k) where k = entities with component):
for (Entity e : m_World->GetEntitiesWithComponent<MeshComponent>()) {
    auto* mesh = m_World->GetComponent<MeshComponent>(e);
    // ...
}
```

#### 3. Per-Entity Texture Lookups (HIGH)

**Problem:** `GetOrLoadTexture()` called 2-5 times per entity per frame via string hash lookups.

| Location | Calls per entity | Total per frame (1000 entities) |
|----------|------------------|--------------------------------|
| RenderSystem.cpp:2558 | baseColorTexturePath | 1000 |
| RenderSystem.cpp:2612 | sprite texture | 500 |
| RenderSystem.cpp:2695 | metallicRoughnessTexture | 500 |
| RenderSystem.cpp:2706 | emissiveTexture | 300 |

**Solution:**
- Cache texture pointers directly on `MaterialComponent` (e.g., `cachedBaseColorTexture`)
- Invalidate cache only when material `texturePath` changes
- Move texture loading to material assignment time, not render time

#### 4. Per-Entity Descriptor Set Updates (HIGH)

**Problem:** `vkUpdateDescriptorSets()` called per-entity when texture changes.

**Location:** RenderSystem.cpp:3008

**Solution:**
- Pre-allocate descriptor sets per unique material/texture combination
- Use descriptor set caching by material hash
- Batch descriptor updates once per frame instead of per-entity
- Consider dynamic descriptor indexing (Vulkan 1.2+)

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
| Skeleton/Animator serialization | High | High | P1 | Pending |
| Dialogue Tree Editor | Very High | Medium | P1 | ✅ Complete |
| Visual Scripting System (Phase 1) | Very High | High | P1 | ✅ Complete |
| Visual Scripting System (Phase 2) | Very High | High | P1 | ✅ Complete |
| Visual Scripting System (Phase 3) | Very High | Medium | P1 | ✅ Complete |
| Visual Scripting System (Phase 4) | Very High | Medium | P1 | ✅ Complete |
| Visual Scripting System (Phase 5+) | High | Medium | P1 | Pending |
| GUI color palette update | Medium | Low | P2 | Pending |
| AI Behavior Tree Editor | High | Medium | P2 | Pending |
| Quest Flow Editor | High | Low | P2 | Pending |
| Typography system | Medium | Low | P2 | Pending |
| Micro-interactions | Medium | Medium | P3 | Pending |

---

## Metrics to Track

### Performance

- Frame time P99 (target: <16ms for 60fps)
- Draw calls per frame
- Entity iteration count per system per frame
- Texture cache hit rate
- Descriptor set update frequency

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
- **Template Rebuild & Demo Scenes** — Update all 15 templates to latest features, add demo scene per template with "Demo" button
- **Planet Gravity Template** — Super Mario Galaxy-style spherical gravity third-person platformer (PlanetGravityZone, SurfaceAlignedController, orbit camera)
- **Project Hub & Creation Wizard** — Replace template selector with Recent Projects / New Project / Demos tabs, folder structure auto-creation, collaboration-ready
- **Editor Accent Color & Theming** — Replace blue accent with TEGE brand `#c7dac4` (sage green), rounded corners, softer panel borders, distinct visual identity
- **Curved Grid Snapping** — Snap entity placement to curved/spherical grid surfaces with orientation alignment
- **Improved Icon/Window Inspector** — Entity icons in hierarchy, component icons in inspector, window icon picker in project settings

### Partially Complete

- **Undo/Redo** — Entity operations done (delete/duplicate/cut/paste/reparent/component add-remove). Remaining: inspector property edits, tilemap paint, terrain sculpt, UI editor edits
- **Drag and Drop** — OS file drop done. Remaining: asset browser to viewport/inspector, hierarchy reparenting
- **Asset Import Pipeline** — Import settings dialog and .enjinasset metadata done. Remaining: thumbnails, axis conversion, texture compression, asset browser drag-import

---

## Runtime Systems (Planned)

- **Skeleton/Animator Serialization** — `SkeletonComponent` and `AnimatorComponent` are not serialized to scene files. They hold complex runtime objects (`shared_ptr<Animation::Skeleton>`, `SkeletalAnimator`, `AnimationStateMachine`) that require serializing bone hierarchies, inverse bind matrices, and animation clips. Mesh bone data (weights/indices) IS preserved. Fix should tie into re-import from glTF/FBX source files rather than serializing runtime state. Store source asset path + animation clip references, reconstruct skeleton on load.
- **Improved Physics** — 2D physics (Box2D-style), 2D joints, CCD, more shape types, physics materials (friction, bounce), script trigger callbacks
- **Basic Networking** — Client-server architecture, state sync, entity ownership, lobbies, RPCs, lag compensation (LAN first, then relay)
- **Destructible Environments** — Prefab-level destructibility, fracture/shatter mesh splitting, debris physics, chain destruction
- **Simple Fluid Simulation** — Grid-based Eulerian fluid (water, lava, gas). FluidVolumeComponent with preset configs. Target: 64x64 2D / 32x32x32 3D at 60fps
- **SVG Support** — nanosvg parsing, rasterize-to-texture caching, UIElement Image widget integration, SDF vector rendering (future)
- **Dialogue System Future Work** — .enjdlg asset files, localization system (string keys + locale tables), UICanvas dialogue box (replace ImGui overlay), dialogue template, Yarn Spinner/Twine import/export

---

## Rendering Pipeline & Performance

In addition to the Critical Rendering Pipeline Issues documented above:

- **3D/2D Pipeline Audit** — Auto-disable shadow pass for 2D-only scenes, sprite batching by texture atlas (biggest 2D perf win), warn on ortho/perspective mixing, flat shading fast path for sprites
- **Pipeline Optimization** — Multi-threaded command buffer recording, GPU payload batching (sort by pipeline/material), indirect rendering (VkCmdDrawIndexedIndirect), async compute for culling/particles/post-process, frame graph resource scheduling, Hi-Z culling

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
*Generated from codebase audit, performance analysis, and feature roadmap*

---

## Recent Completions

### Visual Scripting System Phase 4 (2026-02-06)

Implemented debugging and advanced editing features:

**Breakpoints and Debugging:**
- Breakpoint toggle on nodes (F9 key, red dot indicator)
- Step-through debugging (F5 continue, F10 step over)
- Pause at breakpoint with visual node highlighting
- Execution timeline profiler with color-coded bars

**Latent Nodes:**
- WaitForAudioComplete - waits for AudioSourceComponent to finish playing
- WaitForAnimationComplete - waits for AnimatorComponent to finish

**Multi-Select Editing:**
- Box/marquee selection for multiple nodes
- Ctrl+click to add/remove from selection
- Multi-node drag moves all selected together
- Copy/paste preserves internal links between selected nodes

**Undo/Redo:**
- EditNodePropertyCommand for node property changes
- EditVariableCommand for variable value changes
- Command merging for consecutive edits on same property/variable

---

### Visual Scripting System Phase 1 (2026-02-06)

Implemented Blueprint-style visual scripting foundation:

**Files Created:**
- `Engine/include/Enjin/ECS/Components/VisualScript.h`
- `Engine/include/Enjin/VisualScript/NodeDefinition.h`
- `Engine/include/Enjin/VisualScript/NodeRegistry.h`
- `Engine/src/VisualScript/NodeRegistry.cpp`
- `Engine/include/Enjin/VisualScript/VisualScriptExecutor.h`
- `Engine/src/VisualScript/VisualScriptExecutor.cpp`
- `Engine/include/Enjin/ECS/Systems/VisualScriptSystem.h`
- `Engine/src/ECS/Systems/VisualScriptSystem.cpp`
- `Engine/include/Enjin/Editor/VisualScriptEditor.h`
- `Engine/src/Editor/VisualScriptEditor.cpp`

**Files Modified:**
- `Engine/include/Enjin/Editor/NodeGraph.h` (added Vector2, Vector4, Quaternion pin types)
- `Engine/include/Enjin/Editor/EditorLayer.h/cpp` (panel integration)
- `Engine/include/Enjin/Editor/PlayMode.h/cpp` (system lifecycle)
- `Engine/src/Scene/SceneSerializer.cpp` (full serialization)

**Key Features:**
- 15 built-in nodes covering events, flow control, variables, math, and actions
- Pure node evaluation with frame-level caching
- Flow-based execution with max iteration safety
- Full editor panel with entity sidebar, node graph, variable editor, inspector
- Complete scene serialization for graphs, variables, event mappings, and node metadata

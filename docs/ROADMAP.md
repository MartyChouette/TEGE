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

- 9 typed pins (Flow, Bool, Float, Int, String, Vector3, Color, Entity, Any)
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

#### Phase 1: Dialogue Tree Editor (2-3 weeks)

**Why:** Dialogue trees are fundamental for RPGs, visual novels, and narrative games. The DialogueSystem backend already exists but lacks visual authoring.

**Node Types:**
- Text (speaker + dialogue text)
- Choice (branching decision)
- Condition (if/then logic)
- SetVariable (state mutation)
- Event (trigger callback)
- Root (entry point)
- End (terminal)

**Integration:**
```cpp
struct DialogueTreeEditorAdapter {
    NodeGraphData m_TreeGraph;
    NodeGraphEditor m_GraphEditor;
    std::unordered_map<NodeId, DialogueNode*> m_NodeMap;
};
```

#### Phase 2: Visual Scripting / Blueprints (4-6 weeks)

**Why:** Industry-standard for non-programmers (Unreal Blueprints). Enables designers to create game logic without code.

**Node Types:**
- Input (parameters)
- Variable (get/set)
- Operator (+, -, *, /, &&, ||)
- Function Call (Physics_Raycast, Audio_Play, etc.)
- Control Flow (If/Then/Else, Loop, Event)
- Output (return value)

**Execution Model:**
- Flow pins drive execution order
- Data pins carry values
- Topological sort determines node evaluation order
- Compile graph to AngelScript for runtime execution

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

| Task | Impact | Effort | Priority |
|------|--------|--------|----------|
| Replace GetAllEntities() loops | High | Low | P0 |
| Cache texture pointers on materials | High | Medium | P0 |
| Replace vkDeviceWaitIdle() with fences | Critical | Medium | P0 |
| Dialogue Tree Editor | Very High | Medium | P1 |
| Visual Scripting System | Very High | High | P1 |
| GUI color palette update | Medium | Low | P2 |
| AI Behavior Tree Editor | High | Medium | P2 |
| Quest Flow Editor | High | Low | P2 |
| Typography system | Medium | Low | P2 |
| Micro-interactions | Medium | Medium | P3 |

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

*Last updated: 2026-02-06*
*Generated from codebase audit and performance analysis*

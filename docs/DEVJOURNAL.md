# Enjin Engine — Dev Journal

---

## 2026-02-12

### Full-Screen Stipple / Dither Post-Process
Added a full-screen stipple/dither post-process effect as a primary aesthetic tool. 8 pattern modes: Bayer 4x4, Bayer 8x8, Blue Noise (interleaved gradient noise), Halftone (circular dot grid), Crosshatch (diagonal lines), Overlook (hex geometric), Ordered 2x2 (coarse retro), and Floyd-Steinberg approximation (pseudo error diffusion). 3 color modes: Monochrome (fg/bg ink/paper), Duo-Tone (two configurable colors), Full Color (pattern applied to luminance, preserving hue). Controls: scale (0.5-8x), density (threshold bias), strength (blend with original), and foreground/background color pickers. Applied in the post-process chain after palette lock and before gamma correction. Full editor UI in Post Processing panel with tooltips, scene render settings persistence, and JSON serialization.

### Shadow Dither Patterns
Added 6 built-in shadow dither patterns selectable per-material: Bayer 4x4 (default), Bayer 8x8, Blue Noise (interleaved gradient noise), Halftone (circular dots), Crosshatch (diagonal lines), and Overlook (hexagonal geometric pattern inspired by The Shining's Overlook Hotel carpet). Pattern stored in flag bits 29-31 (freed by compressing vertex snap resolution from 8 bits to 5 bits, packed as value/8). Inspector shows pattern dropdown only when a dither mode is active.

### Shadow Rendering Fix
Fixed shadows not rendering in the editor game view. The root cause was that the offscreen rendering path (`RenderToTarget()` / `RenderOffscreen()`) was missing the shadow pass entirely — shadow depth maps weren't being generated for the game view camera, and the offscreen LightingUBO wasn't populated with shadow cascade matrices or `shadowEnabled`. Added `RenderShadowPassForCamera()` to RenderSystem and wired it into `EditorLayer::RenderOffscreen()`. Also fixed missing `SetupEntityBuffers()` in the shadow pass for entities that hadn't been rendered to the main swapchain yet.

### Template System Redesign
Rebuilt the startup template system from 38 templates down to 22 focused templates organized into 5 categories:
- **Foundations (5):** Blank, 2D Platformer, 2D Top-Down Action, 3D Third Person, 3D First Person
- **Genre Showcases (7):** Sokoban Puzzle, Survival, RPG Village, Horror, Vehicle Racing, PS1 RPG, Arena Fighter
- **Systems Deep-Dives (5):** Physics Playground, Dialogue & Narrative, Save System Demo, Visual Scripting, UI Canvas Demo
- **Retro & Flash (3):** Point & Click, Bullet Hell, Idle/Clicker
- **Advanced (2):** Planet Gravity, Dungeon Crawler

Each template now showcases specific engine features using real components. Layout configs simplified to a 3-tier system (2D / 3D standard / 3D wide for splitscreen). Net code reduction: ~4400 lines removed.

### SceneSerializer Robustness
Applied tolerant `JB()` bool deserialization across all scene components. This prevents crashes when loading scenes saved with the old `RF()` rounding bug that wrote booleans as `0.0`/`1.0` instead of `true`/`false`.

---

## 2026-02-11

### Physics Backend Abstraction (Phases 1-3)
Completed the full physics backend abstraction layer:
- **Phase 1:** Created `IPhysicsBackend` (3D) and `IPhysicsBackend2D` (2D) abstract interfaces. Wrapped existing SimplePhysics engines in adapter classes. Rewired all consumers (ControllerSystem, ScriptBindings, VisualScriptExecutor, NodeRegistry) to use interfaces. Added `PhysicsBackendFactory` with `PhysicsBackendType` enum (Auto/Jolt/Box2D).
- **Phase 2:** `JoltBackend` wrapping Jolt Physics v5.2.0 (1273 lines). Full ECS-to-Jolt synchronization per frame, thread-safe contact events via `JoltContactListener`, bilateral collision filtering, 6 joint types, gravity zones, raycasting, CCD support.
- **Phase 3:** `Box2DBackend` wrapping Box2D v3.0.0 (~755 lines). C API with handle-based IDs, contact/sensor event polling, 5 joint types, raycasting, overlap queries, CCD, bilateral collision filtering.

### Comprehensive Audit — 70+ Fixes
Ran a full security, stability, performance, and feature accessibility audit. Applied 70+ fixes:
- **Security:** Replaced 4 `std::system()` calls with `CreateProcessA`/`ShellExecuteA`, added Vulkan error checks (9 sites), JSON bounds checks (13), enum validation (38)
- **Stability:** PlayMode null guards, PlayModeDiff try-catch, animation PingPong underflow fix, weather NaN guard, particle resize safety
- **Performance:** Replaced 12 `GetAllEntities()` with component queries, merged 12 HasComponent+GetComponent pairs, added xorshift32 PRNG, reserve() calls
- **Feature wiring:** Connected 6 missing `SetBindings*()` in Player, initialized 5 new systems

### Spatial Hash Performance Fix
Fixed `SpatialHashGrid::Insert()` inserting large colliders into ~170 cells per frame. Capped to 8 cells per dimension; oversized entities go into a brute-force list instead.

---

## 2026-02-10

### Tiered Save System
Implemented a complete tiered save system with 20 slots (17 manual + 3 auto-save), three persistence tiers (SceneState/RunState/MetaProgression), pluggable backends (Local/Newgrounds/Steam), and an in-game save/load menu component. Added play mode diff dialog that shows entity changes on Stop with cherry-pick apply.

### Feature Accessibility Fixes
Wired up 14 missing runtime systems in the Player app. Extended the build pipeline to pack scripts, audio, dialogue, prefabs, and data assets. Added 6 new script binding files (Weather, Gameplay, UI, Particles, Prefab, Streaming) and 16 new visual script nodes. Connected Newgrounds bindings and level streaming into PlayMode.

### Panel Reorganization
Reorganized editor panels for better discoverability: Settings split into EditorSettings/ProjectSettings, Effects renamed to RetroEffects, Skybox expanded to Rendering panel. Moved collision groups, environment settings, and rendering settings to their logical homes.

### Security Audit
Documented 35 vulnerabilities (6 CRITICAL, 12 HIGH, 12 MEDIUM, 5 LOW) in `docs/SECURITY_AUDIT.md`.

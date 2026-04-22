# Enjin Engine — Development Journal

Chronological log of major development sessions, decisions, and milestones.

---

## 2026-04-22 — WebGPU Shadow Pipeline & PBR Lighting Fix

**Branch:** `main`

Fixed 3 bugs preventing WebGPU lighting and shadows from rendering:
1. **Depth-only pipeline rejection** — `WebGPUPipelineManager::CreateRenderPipeline()` required a fragment shader; shadow pipelines have none. Now allows null fragment shader when `hasColorAttachment=false`.
2. **Stencil ops on Depth32Float** — `BeginDepthOnlyPass()` set stencil Clear/Store on a format with no stencil aspect, invalidating the command buffer. Changed to `Undefined`.
3. **NaN TBN matrix from zero tangents** — Procedural meshes had no tangent data. `normalize(vec3(0))` → NaN poisoned all PBR dot products. Shader now detects zero tangents and falls back to interpolated world normal. WGSL constraint: `textureSample` must stay outside non-uniform branches.

---

## 2026-02-19 — MaturityTier Badges, Test Hardening, Steam Audio Phase 2

**Branch:** `beta/0.8`

### Work Completed

**Steam Audio Phase 2 — Occlusion & Transmission:**
- `BuildScene()` creates IPLScene + IPLStaticMesh from ECS box/sphere collider geometry
- `UpdateOcclusion()` raycasts source↔listener at ~10Hz, stores per-source occlusion/transmission factors
- IPLDirectEffect applies low-pass filtering before HRTF in the audio callback
- Default acoustic material: concrete-like absorption/transmission coefficients
- Editor UI: Occlusion + Transmission toggles in Project Settings (Transmission disabled when Occlusion off)
- Wired into PlayMode start and Player scene load via `SimpleAudio::BuildSteamAudioScene()`

**MaturityTier Badges on Templates:**
- Added `MaturityTier` enum (Stable/Beta/Preview/Experimental) with color-coded badges (Blue/Green/Amber/Red)
- Tagged all 44 hub wizard templates and 15 marketplace catalog entries with maturity tiers
- Hub wizard: color-coded pill badge rendered in top-left corner of each template card
- Marketplace: colored `[Tier]` tag in list rows + "Maturity:" field in expanded detail view

**Test Suite Reorganization & New Tests:**
- Moved 4 integration tests (SerializerRoundTrip, DungeonCrawlerTest, SMTTemplateTest, StressTest) into `Tests/Integration/` subfolder
- Added 3 new rigorous unit test suites (68 tests):
  - `TestMarketplace` — Catalog integrity, search/filter, maturity tiers, ID uniqueness
  - `TestCollisionFiltering` — Bilateral bitmask rules, group isolation, trigger semantics, serialization round-trip
  - `TestSaveSystem` — Meta-progression KV stores, persistence tiers, slot management, SaveDataComponent helpers

**HIGH Priority Test Suites — Round 1 (108 tests):**
- `TestSceneSerializerComponents` (20) — Material PBR/alpha/retro/dither/parallax, Light point/spot/dir, Camera, Notes, PostProcessVolume, multi-component
- `TestInputAction` (28) — Defaults, config mutation, sprint/crouch toggle, presets (LeftHand/RightHand/Gamepad), JSON round-trip, remapping
- `TestScriptEngine` (19) — Headless init, binding registration (680+ functions), type registration (Vector2/3/4, Quaternion, Entity), context pool, compilation, execution, global variable access
- `TestEditorSettings` (19) — Defaults (theme, UI scale, audio, accessibility, input, motor, keyboard nav, frame rate, colorblind), JSON round-trips (theme, FPS, motor, recent projects), missing file handling, accent colors, enum ranges
- `TestNetworkAdversarial` (22) — Buffer underruns (empty/truncated), string overflow, exact-fit strings, max-length strings, exhausted buffer, primitive edge cases (max u32, negative i32, NaN/Inf f32), Vector3/Quaternion partial reads, sequential multi-field parsing

**HIGH Priority Test Suites — Round 2 (126 tests):**
- `TestNetworkSecurity` (29) — SHA256 known vectors (empty, "abc", single byte), incremental vs one-shot, block boundary cases (55/56/1000 bytes), HMAC-SHA256 compute/verify/tamper/wrong-key/wrong-data/deterministic/empty, ReplayWindow (sequential, duplicate, out-of-order, too-old, boundary, large jump, reset, zero sequence), GenerateSessionKey (non-zero, uniqueness)
- `TestLocalization` (26) — Singleton, defaults, set/get/overwrite/has/count/getAllKeys, locale switching, English fallback, invalid locale, add locale, parameterized formatting (single/multiple/repeated/missing params), JSON round-trip, CSV round-trip
- `TestIKSolver` (16) — LookAtIK (basic solve, degenerate target, angle clamp, large max angle, zero dt), RotationFromDirection (forward Z, unit quaternion), FABRIK (2-joint reachable, root fixed, bone lengths preserved, unreachable stretch, single joint, empty chain, 3-bone, iteration accuracy, target at root)
- `TestDataAsset` (24) — Schema CRUD (register/find/remove/getAll/fieldCount), Asset CRUD (create/find/remove/bySchema/getAll), typed getters/setters (float/int/bool/string/Vector3, fallbacks, float↔int coercion), JSON round-trips (schema .enjschema, asset .enjdata), field type string conversions, Clear()
- `TestTimeline` (31) — Defaults, Play/Pause/Stop/Seek, forward playback, loop wrapping, ping-pong direction flip, event firing/no-double-fire/reset, keyframe storage (float/Vector3/bool/string), AnimationTrack defaults, easing functions (Linear/EaseIn/EaseOut/EaseInOut/Step at boundary values)

### Key Findings During Testing
- **ScriptEngine `AS_CHECK` macro uses `assert()` (stripped in Release):** `RegisterAllBindings` silently ignores binding registration failures, leaving the engine misconfigured for compilation. Tests restructured to compile against base engine only.
- **`vertexSnapResolution` clamped to ≤31:** Serializer rejects values >31 (encoded as /8 value in push constant bits 24-28).
- **SceneSerializer requires `World*` constructor:** No default constructor available, unlike what research suggested.

### Test Summary

| Test Run | Targets | Tests | Status |
|----------|---------|-------|--------|
| Round 0 (prior) | 18 | ~300+ | All pass |
| Round 1 (new) | 3 | 68 | All pass |
| HIGH R1 | 5 | 108 | All pass |
| HIGH R2 | 5 | 126 | All pass |
| **Total** | **31** | **~600+** | **31/31 CTest pass** |

### Files Changed

| Commit | Files | Summary |
|--------|-------|---------|
| Steam Audio Phase 2 | SteamAudioProcessor.h/cpp, SimpleAudio.h/cpp, EditorLayer.cpp | Occlusion + transmission for geometry-aware audio |
| MaturityTier badges | TemplateMarketplace.h/cpp, EditorLayer.cpp | 4-tier maturity labels on all templates |
| Test reorganization | Tests/CMakeLists.txt, Tests/Integration/*, Tests/Unit/* | Subfolder split + 3 new test suites |
| HIGH R1 tests | 5 new TestUnit/*.cpp, Tests/CMakeLists.txt | Trust-boundary test suites |
| HIGH R2 tests | 5 new TestUnit/*.cpp, Tests/CMakeLists.txt | Security & algorithm test suites |

### Next Steps — MEDIUM Priority Test Suites (researched, not yet written)
11 subsystems identified and APIs fully researched:
1. **Quest System** — QuestFlowStatus state machine, QuestNodeType categories, ResetRuntimeState
2. **Navmesh/A*** — Navmesh polygon CRUD, Pathfinder A*, NavmeshGenerator grid, NavmeshUtils geometry helpers
3. **Object Pool** — CreatePool/Acquire/Release lifecycle (needs ECS::World)
4. **Prefab System** — Prefab data structures, PrefabManager singleton (needs ECS::World for instantiation)
5. **Reaction-Diffusion** — RDConfig presets, Initialize/Step/SeedCircle, BakeToRGBA8/Heightmap
6. **Interactive Water** — InteractiveWaterComponent defaults, Initialize, GetWaterHeight, CreateSplash
7. **WorldTime/TimeOfDay** — CalendarConfig, Season enum, sun position, IsDay/IsNight, time wrapping
8. **Screen Transitions** — 9 TransitionTypes, phase progression (Idle→Out→Hold→In), midpoint detection
9. **Cinematic System** — CinematicCameraComponent waypoints, easing, play/stop state
10. **Profiler** — Singleton, RecordScope, frame history, FPS, Reset
11. **Plugin System** — PluginManifest struct, PluginEntry state, scan/query (DLL loading not testable headlessly)

---

## 2026-02-18 — Beta 0.8 Branch: Systematic Hardening

**Branch:** `beta/0.8` (created from `main`)
**Goal:** Audit and harden all Tier 1 systems before v0.8.0 release.

### Strategy

Adopted a 4-tier feature maturity system:

| Tier | Label | Color | Meaning |
|------|-------|-------|---------|
| 1 | **Stable** | Blue | Audited, tested, production-ready |
| 2 | **Beta** | Green | Feature-complete, needs hardening |
| 3 | **Preview** | Amber | Functional but incomplete |
| 4 | **Experimental** | Red | Prototypes, may break |

**Release channels:** `main` (full engine) → `beta/0.8` (hardening) → `release/0.8` (ships).

### Work Completed

**ECS Audit — 5 findings, all fixed:**
- EntityManager O(N) → O(1) validity checks via companion `unordered_set`
- EntityManager O(N) destroy → O(1) swap-and-pop + set removal
- World::GetComponent stopped silently allocating storage for read paths
- FlushPendingDestructions TOCTOU race fixed (lock before empty check)
- EventBus ID wraparound guard
- Serializer: 5 enum/bounds fixes
- Tests expanded from 13 → 40

**Renderer Audit — 16 findings, all fixed:**
- VulkanContext: 6 VkResult checks added (enumeration, surface support, queues, shutdown)
- VulkanBuffer: null context guard, zero-size rejection, 2 resource leak fixes, overflow-safe bounds, double-map prevention
- VulkanSwapchain: vkBindImageMemory check, vkGetSwapchainImages check, Recreate() error propagation

**Serialization & Asset Pack Audit — 31 findings, 18 fixed:**
- SceneSerializer: 14/17 fixed (array caps, type safety, enum clamping)
- AssetReader/Packer: 4/14 fixed (integer overflow, path traversal, decompression safety)
- Remaining 13 are MED/LOW risk, deferred

**Physics & Audio Audit — 8 findings, all fixed:**
- Jolt: constraint null check, zero-axis defaults for hinge/slider, spring param clamping
- Box2D: shape/body validity in ResolveEntity, stale collision pair cleanup, sensor carry-forward
- Audio: volume/pitch clamping at entry, 3D distance validation, 256 active sound cap

### Decisions Made

1. **Version numbering:** Current is v0.7, working toward v0.8.0. Not v1.0.
2. **Branch model:** `main` stays full/unrestricted. `beta/0.8` is the hardening branch. `release/0.8` will be cut when ready.
3. **CMake feature gating planned:** `ENJIN_PHYSICS_JOLT`, `ENJIN_RAYTRACING`, `ENJIN_NETWORKING`, `ENJIN_EXPERIMENTAL` flags to silo features.
4. **Public roadmap created:** `docs/PUBLIC_ROADMAP.html` — 6 Mermaid diagrams, 3-color scheme.

### Files Changed

| Commit | Files | Summary |
|--------|-------|---------|
| Auto-create project in Build dialog | EditorSettings.h/cpp, EditorLayer.h/cpp | `lastProjectDir` persistence, inline project creation |
| ECS audit | Entity.h/cpp, World.h/cpp, EntityEventBus.cpp, SceneSerializer.cpp, TestECS.cpp | O(1) validity, deferred destruction, 40 tests |
| Renderer audit | VulkanContext.cpp, VulkanBuffer.cpp, VulkanSwapchain.h/cpp | 16 Vulkan safety fixes |

---

## 2026-02-17 — QA Audit: Physics, Scripting, Serialization

**Branch:** `main`

Resolved all critical, high, and medium findings from three targeted audits:

**Physics (8 fixes):**
- Jolt BodyID generation counter preservation (was reconstructing from index alone, losing generation)
- Box2D body validity checks before force/torque application
- Friction/restitution/gravityScale clamping to sane ranges
- Mass guard before gravity force application

**Scripting (9 fixes):**
- Event recursion depth limit (8) prevents stack overflow
- Listener cap per event (1024) prevents memory exhaustion
- Coroutine cap (4096) prevents runaway scripts
- Include depth limit (16) prevents circular includes
- Per-frame entity creation cap (256) prevents allocation storms
- Canonical path resolution for symlink-based include escape
- `FlushDeferredEntityDestroys` wired into PlayMode and Player

**Serialization (9 fixes):**
- WaterVolume `.contains()` guards on all JSON access
- HealthComponent runtime state serialization (currentHealth/Shield/isDead)
- RigidbodyComponent runtime state (velocity/angular/grounded/sleeping)
- DamageComponent.damagedEntities cleared on load
- PostProcessSettings enum bounds validation

**Tests:** 8 new test executables written (Physics2D, VisualScript, BehaviorTree, UISystem, Networking, StressFuzz, AssetPack, Dialogue).

---

## 2026-02-16 — Shadow Flickering Investigation & Flower System

**Branch:** `main`

- Investigated shadow flickering on thin geometry — traced to CSM cascade boundaries
- Added cascade blending in fragment shader (smooth transition zone)
- Reworked FlowerSystem: wind animation, growth stages, season color variation
- Added FlowerComponent script bindings and visual script nodes

---

## Prior Sessions

See `docs/AUDIT_2026_02_11.md` through `docs/AUDIT_2026_02_13.md` for earlier audit rounds covering the full codebase (152 findings, 132 fixed).

See `docs/ROADMAP.md` for the complete technical roadmap and performance optimization history.

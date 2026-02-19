# Enjin Engine — Development Journal

Chronological log of major development sessions, decisions, and milestones.

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
- Added 3 new rigorous unit test suites:
  - `TestMarketplace` — Catalog integrity, search/filter, maturity tiers, ID uniqueness
  - `TestCollisionFiltering` — Bilateral bitmask rules, group isolation, trigger semantics, serialization round-trip
  - `TestSaveSystem` — Meta-progression KV stores, persistence tiers, slot management, SaveDataComponent helpers

### Files Changed

| Commit | Files | Summary |
|--------|-------|---------|
| Steam Audio Phase 2 | SteamAudioProcessor.h/cpp, SimpleAudio.h/cpp, EditorLayer.cpp | Occlusion + transmission for geometry-aware audio |
| MaturityTier badges | TemplateMarketplace.h/cpp, EditorLayer.cpp | 4-tier maturity labels on all templates |
| Test reorganization | Tests/CMakeLists.txt, Tests/Integration/*, Tests/Unit/* | Subfolder split + 3 new test suites |

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

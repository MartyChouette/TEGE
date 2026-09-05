# Glacier-Inspired Long-Term Engine Roadmap

> **Status: UNOWNED, and this document tracks nothing.** Reviewed 2026-09-05.
> Eight phases, week estimates, and a dependency graph, with no status marker
> anywhere in the file - it cannot tell you what is done, in flight, or
> abandoned. That matters now because the engine has overtaken parts of it:
> Phase 5 (volumetric fog) and Phase 6 (GPU particles + lighting integration)
> both have shipped work, and this plan does not know.
>
> Treat it as a research direction rather than a schedule. Before starting any
> phase, check the current state of that system first - two separate audits
> this year produced work that was already finished because a list was trusted
> over the code.

## Context

After analyzing IO Interactive's Glacier engine advances (Digital Foundry interview, 007 First Light), this plan addresses 8 major capability gaps in Enjin. The goal: bring Enjin's rendering, streaming, ECS, animation, and editor architecture to the same caliber as a AAA in-house engine — while maintaining the cross-platform story (Vulkan + WebGPU + future Metal) and the "120 FPS No Matter What" pillar.

**Key user requirement:** All particles, transparents, and volumetrics must integrate with EVERY lighting system (clustered lights, shadows, GI probes, volumetric fog). Artists must be able to build effects however they want without worrying about which lighting features apply.

---

## Phase 1: Shader Specialization + Shadow Budget (6-8 weeks)

**Why:** Every subsequent phase depends on efficient shader variants and a dynamic shadow system. The current monolithic push-constant flags cause unnecessary GPU ALU waste; fixed shadow allocations can't scale.

**Deliverables:**
1. **Vulkan specialization constants** — Replace 32-bit push-constant feature flags with `VkSpecializationInfo`. Cache SPIR-V permutations keyed by constant values in `ShaderManager`.
2. **WGSL override constants** — WebGPU equivalent using `override` declarations in all 4 WGSL shaders.
3. **Shadow atlas pool** — Unify `ShadowMap`, `PointLightShadowMap`, `SpotLightShadowMap` into a single tiled depth atlas. Dynamic tile allocation replaces fixed per-light resources.
4. **Dynamic shadow budget** — Cluster-pixel-count priority: the `ClusteredLighting` compute pass outputs per-light pixel-weight via atomics. Top N lights get shadow tiles per frame; rest fall back to SH probe irradiance.

**Key files:** `ShaderManager.h`, `ShadowMap.h`, `PointLightShadowMap.h`, `SpotLightShadowMap.h`, `ClusteredLighting.h`, all `.wgsl` shaders
**New:** `ShadowAtlas.h`, `ShadowBudget.cpp`

**Scalability:** One shadow atlas, one budget knob. Low-end = 4 tiles at 512x512. High-end = 16+ tiles at 2048x2048. `AdaptiveQualitySystem` feeds tile size/count.

---

## Phase 2: Software-Traced DDGI via SDF (8-10 weeks)

**Why:** Glacier's crown jewel — GI that works on ALL platforms including Switch 2 class hardware, no hardware RT required. The engine already has SH probes, analytic SDFs, VCT, and RT GI — but nothing connects them into a unified software-traced probe system.

**Deliverables:**
1. **GPU mesh voxelization** — Move `VoxelGrid::Voxelize()` from CPU to compute shader. Read from `MergedGeometryBuffer`, output 3D texture (R16F) for SDF storage.
2. **GPU SDF volumes from mesh data** — Extend `MeshToSDF` to output GPU-resident 3D textures for probe tracing.
3. **DDGI probe system** — Grid of irradiance probes updated via SDF ray marching in compute. Octahedral encoding in probe texture atlas. Stores both irradiance and radiance (for volumetric sampling).
4. **Temporal amortization** — Update 1/8th of probes per frame, cycling over 8 frames. Keeps GI within 2ms budget.
5. **Fallback cascade** — VCT or existing SH probes fill in at distance where DDGI grid is sparse.

**Key files:** `VoxelConeTracing.h`, `SDFRenderer.h`, `SDFScene.h`, `SHLightProbe.h`
**New:** `DDGIProbeSystem.h`, `ddgi_probe_update.comp`, `ddgi_sample.comp`, `gpu_voxelize.comp` + WGSL equivalents

**Scalability:** Low-end: 8x4x8 grid, 32 rays/probe, update 1/16 per frame. High-end: 16x8x16, 128 rays/probe, update 1/4. WebGPU: CPU voxelization at 32^3, update once/second.

---

## Phase 3: Async Streaming with 2ms Frame Budget (6-8 weeks)

**Why:** Glacier enforces that no single streaming function exceeds 2ms on main thread at 60fps. Current `StreamingManager` is main-thread-blocking. This is critical for open-world and driving sequences.

**Deliverables:**
1. **Time-sliced entity integration** — Deserialize on worker thread into staging buffer. Integrate entities in batches on main thread, yield after 2ms wall-clock.
2. **Priority queue scheduling** — Replace flat load queue with distance + `StreamPriority` heap. Critical chunks always first.
3. **Predictive loading** — Camera velocity analysis to pre-fetch chunks 1-2 seconds ahead.
4. **Brick layer composition** — Glacier-style stacking: base layer + additive/override layers. Higher layers can override or remove entities from lower layers. Enables mission-specific scene variants without duplicating content.
5. **Memory budget** — Track resident memory; evict lowest-priority chunks approaching limits.

**Key files:** `LevelStreaming.h/.cpp`, `SceneSerializer.cpp`, `World.h`
**New:** `BrickComposition.h`, `StreamingBudget.h`

**Scalability:** Fixed 2ms budget is absolute. Low-end CPUs integrate fewer entities/frame (more pop-in, never frame drops). Web: staged integration within requestAnimationFrame, no worker threads.

---

## Phase 4: ECS Optimization for 2M Entities (5-7 weeks)

**Why:** Glacier achieves 32 bytes per entity with 2M loaded (70% logic, 30% visual). Enjin's current overhead is ~104 bytes per entity with 3 components, mostly from `unordered_set` hash buckets.

**Deliverables:**
1. **Eliminate `m_ActiveEntitySet`** — Replace with `vector<u8>` validity array indexed by entity ID. Saves ~48 bytes/entity.
2. **Generation-packed entity IDs** — Upper 32 bits = generation, lower 32 = index. `IsValid()` becomes single array lookup. Eliminates separate active entity vector.
3. **Archetype fast path** — For logic-only entities (no mesh/transform), contiguous archetype storage. Bulk insertion/iteration. Coexists with existing sparse-dense storage for visual entities.
4. **Component memory pools** — Chunked allocator for `ComponentStorage<T>::Add()` instead of per-entity `push_back` reallocation.
5. **Parallel system update** — System dependency graph allows independent systems to run concurrently.

**Key files:** `Entity.h`, `Component.h`, `World.h`, `System.h`

**Scalability:** Editing cost is O(visible), not O(total). Scene hierarchy uses virtual scrolling. Generation-packed IDs make `IsValid()` constant-time regardless of entity count.

---

## Phase 5: Volumetric Fog, Clouds, and Froxel Lighting (8-10 weeks)

**Why:** The engine currently only has linear distance fog. Glacier uses volumetric fog as both atmosphere AND a lighting medium. This phase adds true 3D volumetric rendering integrated with clustered lighting and DDGI.

**Deliverables:**
1. **Froxel volume** — 3D texture (160x90x64) in view space. Reuses cluster grid concept from `ClusteredLightingSystem`.
2. **Volumetric fog compute** — Per-froxel: sample noise-driven density, accumulate in-scattered light from ALL clustered lights (using Phase 1's shadow atlas for shadow testing), sample DDGI probes (Phase 2) for indirect light contribution.
3. **Temporal reprojection** — Reproject previous frame's volume to reduce noise. Uses existing `HaltonSequence.h` for jitter.
4. **Volumetric clouds** — Separate ray-march pass driven by `WeatherSystem` coverage parameters.
5. **Full lighting integration for fog** — Every direct light, every shadow, DDGI irradiance, all affect the fog volume. No exceptions.
6. **God ray upgrade** — Replace screen-space `fogShafts` with volumetric-aware light shafts reading from froxel volume.

**Key files:** `ClusteredLighting.h`, `PostProcessing.h`, `OITManager.h`, `Weather.h`
**New:** `VolumetricFog.h`, `volumetric_fog.comp`, `volumetric_clouds.frag` + WGSL equivalents

**Scalability:** Low-end: 80x45x32 froxels, 4 temporal samples, no clouds. High-end: 160x90x128, 16 samples, full clouds.

---

## Phase 6: GPU Particles + Volumetric Particles + Full Lighting Integration (8-10 weeks)

**Why:** This is the "Smolder" equivalent. Glacier renders 400+ volumetric particle instances with full lighting integration. **Every particle and transparent object must receive light from every system — no exceptions.** Artists build effects however they want.

**Deliverables:**
1. **GPU particle simulation** — Compute shader replaces `ParticleSystem::Update()`. Position, velocity, lifetime, forces (gravity, wind via `WindSystem`) all GPU-side. Emitter config as uniform buffer, particle state in SSBOs.
2. **Indirect draw from compute** — Eliminates CPU readback. WebGPU fallback: read count, issue direct draw.
3. **Volumetric particle shapes** — Pre-baked 3D density textures (OpenVDB import or SDF-generated). Rendered as view-aligned volume slice stacks with ray marching.
4. **FULL lighting integration for ALL particles and transparents:**
   - Every clustered light affects every particle (point, spot, directional)
   - Shadow atlas lookup per particle (Phase 1's budget system)
   - DDGI probe sampling for indirect light (Phase 2)
   - Volumetric fog in-scattering applied to particles (Phase 5's froxel volume)
   - Self-shadowing within volumetric particles via density accumulation
   - Emissive particles contribute back to the froxel volume (two-way coupling)
5. **OIT compositing for everything** — Upgrade `OITManager` so volumetric particles, billboard particles, transparent meshes, and volumetric fog ALL composite through the same order-independent pipeline. No sorting artifacts, no special cases. Artists drop any effect anywhere and it just works.
6. **Particle ↔ volumetric fog interaction** — Particles inside fog receive fog scattering. Volumetric particles contribute density to the froxel grid. Explosions thicken local fog.
7. **100K+ particle cap** — GPU simulation enables massive counts. Display cap configurable per platform.

**Key files:** `ParticleSystem.h`, `ParticleRenderer.h`, `OITManager.h`, `VolumetricFog.h` (Phase 5), `ClusteredLighting.h`
**New:** `GPUParticleSystem.h`, `particle_simulate.comp`, `volumetric_particle.frag`, `VolumetricParticleRenderer.h`

**Scalability:** CPU particle fallback remains for min-spec. GPU compute with 32K cap on GTX 1660 class. Volumetric particles are high-end only; billboards remain default on low-end. But the LIGHTING integration applies to ALL particles regardless of rendering mode — even billboard particles on low-end receive clustered light + shadows + GI.

**The principle:** "One lighting path for everything." The fragment shader for particles (billboard or volumetric) calls the same `EvaluateLighting()` function as opaque geometry. It reads from the same clustered light grid, the same shadow atlas, the same DDGI probes, the same froxel volume. No separate "particle lighting" system. No "transparent lighting" system. One system. Every light. Every shadow. Every particle.

---

## Phase 7: Motion Matching + Animation Pipeline (8-10 weeks)

**Why:** Glacier ships 10x the animation count of Hitman via motion matching. Enjin has solid animation foundations but no motion matching. This is the difference between "functional" and "AAA feel."

**Deliverables:**
1. **Motion matching database** — Offline tool processes clips into searchable pose database. Features: foot positions, hip velocity, trajectory prediction (0.2s intervals). Flat float arrays for SIMD search.
2. **Runtime pose search** — Per-frame: compute current features + desired trajectory, search database for best match. `SkeletalAnimator` gains motion matching mode alongside state machine mode.
3. **Inertialization blending** — Spring-damper decay of pose difference at clip transitions. Eliminates foot-sliding that crossfades cause.
4. **Motion warping** — Warp root motion to align with gameplay targets (vault hand → ledge, punch → enemy position). Builds on existing bone manipulation API.
5. **Data-driven ragdoll** — Animation-driven joint limits. Sample current pose for per-joint limit ranges → more natural ragdoll falls.
6. **Blend tree integration** — Motion matching handles locomotion base layer; blend trees/state machines handle upper body, combat, interaction overlays.

**Key files:** `Animation.h/.cpp`, `RagdollSystem.cpp`
**New:** `MotionMatching.h`, `Inertialization.h`, `MotionWarping.h`

**Scalability:** Search budget, not search space. Low-end: 10K poses, 6 features. High-end: 100K poses, 12 features. KD-tree with early termination. Memory-limited platforms: fewer clips, fall back to existing blend trees.

---

## Phase 8: Editor Process Isolation (6-8 weeks)

**Why:** Glacier separates editor and engine into different processes. Engine crash → restart child process, no work lost. This comes last because all systems must be stable first.

**Deliverables:**
1. **Editor-runtime protocol** — Cross-process IPC (shared memory on desktop, WebSocket for Web/remote). Messages: entity CRUD, component modification, play/pause/stop, viewport camera sync, asset reload.
2. **Runtime child process** — Rendering loop in standalone child. Editor owns ImGui + scene editing state. Crash → restart child, resend scene.
3. **Viewport sharing** — Vulkan: `VK_KHR_external_memory` for zero-copy. Fallback: compress via shared memory.
4. **Multi-platform runtime connection** — Editor connects to runtime on different machine/platform (browser WebGPU, Android device) via WebSocket transport.
5. **Scene state persistence** — Editor maintains shadow JSON copy. On crash, shadow copy → new runtime. Zero data loss.
6. **Play mode hot-switch** — Edit/play mode without restarting runtime. Edit = paused physics + continued rendering.

**Key files:** `Editor/src/main.cpp`, `Player/`, `SceneSerializer.cpp`, `RenderTarget.h`
**New:** `EditorProtocol.h`, `RuntimeProcess.cpp`, `EditorBridge.h`

**Scalability:** Communication cost is O(changes), not O(scene). Delta protocol. 2M entities ≠ 2M messages. Low-bandwidth: downscaled viewport frames.

---

## Phase Dependency Graph

```
Phase 1 (Shaders + Shadows)
    │
    ▼
Phase 2 (Software DDGI) ────────────┐
    │                                │
    ▼                                ▼
Phase 3 (Async Streaming)    Phase 5 (Volumetric Fog)
    │                                │
    ▼                                ▼
Phase 4 (ECS 2M Entities)   Phase 6 (GPU Particles + Full Lighting)
    │                                │
    └────────────────┬───────────────┘
                     ▼
             Phase 7 (Motion Matching)
                     │
                     ▼
             Phase 8 (Editor Isolation)
```

Phases 3-4 (streaming/ECS track) and 5-6 (rendering/effects track) run in parallel after Phase 2.

## Timeline

| Phase | Effort | Cumulative |
|-------|--------|------------|
| 1. Shaders + Shadows | 6-8 weeks | 6-8 weeks |
| 2. Software DDGI | 8-10 weeks | 14-18 weeks |
| 3. Async Streaming | 6-8 weeks | 20-26 weeks (parallel with 5) |
| 4. ECS 2M Entities | 5-7 weeks | 25-33 weeks (parallel with 6) |
| 5. Volumetric Fog | 8-10 weeks | 22-28 weeks (parallel with 3) |
| 6. GPU Particles + Lighting | 8-10 weeks | 30-38 weeks (parallel with 4) |
| 7. Motion Matching | 8-10 weeks | 38-48 weeks |
| 8. Editor Isolation | 6-8 weeks | 44-56 weeks |

**Critical path:** ~44-56 weeks (~10-13 months) with parallel tracks.

## Verification

Each phase ships with:
- New test suites added to CTest (existing 82 targets + ~1100 cases must continue passing)
- Visual regression screenshots for rendering changes
- Performance benchmark capturing frame time before/after
- WebGPU build verified (no Vulkan-only regressions)
- `docs/ROADMAP.md` updated with completed items

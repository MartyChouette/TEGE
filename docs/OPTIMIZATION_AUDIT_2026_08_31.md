# Optimization Opportunity Audit — 2026-08-31

Scope: render hot path (RenderSystem, Vulkan), ECS iteration, simulation systems, memory.
Method: two parallel code sweeps, then every load-bearing claim re-verified against source
on the main thread before inclusion. Findings that failed verification are listed at the
bottom so nobody re-discovers them.

Ground truth established during verification:
- Component storage is a **sparse-set** (dense entity + component arrays, sparse index
  map): `Engine/include/Enjin/ECS/Component.h:49-206`. Iteration over a single component
  type is already dense and cache-friendly. Storage objects are found via a type-ID
  `unordered_map` in `World.h:461` — that hash lookup, not the storage itself, is the
  per-call cost of `GetComponent`.
- `ComputeWorldMatrix` **has a per-frame cache** (`worldMatrixDirty` fast path,
  `Hierarchy.h:127-130`). Hierarchy walks are not repeated per call.

## Verified findings, priority order

### 1. Per-entity descriptor set bind in RenderEntity — HIGH — **FIXED (2fbc13b8)**
Bind cache armed/disarmed by the entity loops; RenderEntity now also binds the
ACTIVE set, fixing latent wrong-camera uniforms for tilemap/sprite fallback
draws in offscreen and splitscreen passes. Golden-verified byte-identical.
`RenderSystem.cpp:~10786`: every `RenderEntity()` call re-binds set 0
(`m_DescriptorSets[currentFrame]`) even though the main pass binds it once at setup
(~5252) and consecutive entity draws don't change it. 100 entities = ~100 redundant
`vkCmdBindDescriptorSets` per frame, per pass (main + offscreen).
**Fix shape:** track "set 0 currently bound for this command buffer" and skip when clean.
The invalidation points are every place another pipeline/layout binds (vegetation
renderers, water, skybox, sprite batch, UI) — this is per-command-buffer state, so per
the parallel-shadow rule it must be passed per-CB, never a shared member. Do it
carefully or not at all; a missed invalidation = wrong descriptors, which is worse than
the redundant bind.

### 2. Render list sorted every frame — HIGH — **FIXED (2fbc13b8)**
FNV hash over membership + static key bits, camera-delta threshold, blend
draws hash their depth bits (movement correctness). Golden-verified.
Original finding:
`RenderSystem.cpp:5172-5228`: `m_SortedRenderList` is rebuilt and `std::sort`ed every
frame; the comment at ~2194 already admits "skipping it would need movement
dirty-tracking". Opaque geometry only needs re-sorting on entity add/remove or material
change; transparent needs it only when the camera moves meaningfully.
**Fix shape:** camera-delta threshold + entity-list dirty flag; sort transparent range
separately from opaque range.

### 3. Storage-pointer caching in per-frame system loops — MEDIUM, easy
`ControllerSystem.cpp:110+` (6 controller loops) and `AISystem.cpp:62-68` call
`GetComponent<T>` 2-3 times per entity per frame — each is a type-ID hash lookup.
RenderSystem already demonstrates the fix (`m_CachedMeshStorage->Get(entity)`).
**Fix shape:** hoist `GetComponentStorage<T>()` above each loop; also drop the
per-iteration `IsValid()` (takes the world mutex per call — see finding 5) in favor of
one lock-free pending-destruction check, or trust the flush ordering where provable.
N is small (controllers), so the win is hygiene + a pattern for hot systems, not
frame-time headroom.

### 4. GatherColliders / zone scans re-run per cloth entity — MEDIUM
`ClothSystem.cpp:158-218`: 5 full component-type iterations (box/sphere/capsule
colliders + both controller types) per cloth/rope entity per frame. `SampleWeatherWind`
(`ClothSystem.cpp:267`) additionally iterates all weather zones once per cloth and once
per rope per frame (NOT per particle — see false positives). Fine at playground scale;
becomes real cost with dozens of cloths.
**Fix shape:** gather colliders + zones once per ClothSystem::Update, share across all
cloth entities that frame. Structural-change invalidation not even needed if gathered
fresh once per frame.

### 5. IsValid()/FindEntityByName take the world mutex per call — LOW-MEDIUM
`World.cpp:111-117` and `World.cpp:154-164`. Documented/deliberate (CLAUDE.md: the read
hot path is lock-free, IsValid keeps its lock). The cost is real only when called per
entity per frame in loops (finding 3's loops do exactly that). Prefer removing the
per-iteration calls over touching the locking design.

### 6. Cluster light array rebuilt + unreserved every frame — LOW-MEDIUM
`RenderSystem.cpp:5556-5595`: `std::vector<ClusterLight>` built fresh per frame with no
`reserve` and no light-dirty gating. Trivial fixes: reserve
`m_CachedLightEntities.size() + m_TransientPointLights.size()`, or promote to a member
vector; optionally skip rebuild when the light set hasn't changed.

### 7. Custom allocators are dead weight — VERIFIED UNUSED
`Core/include/Enjin/Memory/Memory.h:39-98`: StackAllocator/PoolAllocator/LinearAllocator
are referenced by exactly one file outside their implementation — their unit test.
`GetDefaultAllocator`/`SetDefaultAllocator` are never called. Either adopt them where a
profiler shows malloc churn (particles, cloth constraints, per-frame scratch) or accept
they're a library for future use. Don't leave them implied-in-use in docs.

### 8. Web shadow pass iterates all mesh entities — LOW (web only)
`RenderSystem.cpp:~1900-1936` (web half): spot-shadow pass filters post-hoc over every
MeshComponent entity; the Vulkan path uses the pre-built `m_FrameShadowCasters` list.
Port the same list to the web half.

### 9. Known architectural smell (pre-existing, queued): sim-in-render
`EditorLayer::RenderOffscreen` still hosts ~11 simulation updates behind the FPS
limiter (the source of the 30fps-slows-rain bug class and the simDt/wind-clock
bullet-time rules). The durable fix is moving sim out of the render path entirely;
every new editor-side clock keeps paying the "remember to scale by timeScale" tax
until then.

## Claims that FAILED verification (do not act on these)
- "ComputeWorldMatrix recomputed per call / hierarchy walked repeatedly" — false, it has
  a per-frame dirty-flag cache (`Hierarchy.h:127`).
- "SampleWeatherWind runs per cloth PARTICLE (thousands of zone queries)" — false, it
  runs once per cloth (`ClothSystem.cpp:522`) and once per rope (`:679`).
- Both were plausible-sounding sweep findings; they died on a direct read of the call
  sites. Same lesson as the 08-28 audit: verify before fixing.

## What was NOT audited
GPU-side costs (shader occupancy, bandwidth, overdraw), asset load times, and the
physics backends. Those need profiler sessions, not code sweeps.

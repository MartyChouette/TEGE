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

### 3. Storage-pointer caching in per-frame system loops — MEDIUM — **FIXED**
ForEachActiveController template (ControllerSystem.cpp) hoists all three
storages; AISystem loop hoisted the same way. Per-entity IsValid() now runs
only on frames with deferred destructions pending, via the new lock-free
`World::HasPendingDestructions()` (safe under adr-0004). Original finding:
`ControllerSystem.cpp:110+` (6 controller loops) and `AISystem.cpp:62-68` call
`GetComponent<T>` 2-3 times per entity per frame — each is a type-ID hash lookup.
RenderSystem already demonstrates the fix (`m_CachedMeshStorage->Get(entity)`).
**Fix shape:** hoist `GetComponentStorage<T>()` above each loop; also drop the
per-iteration `IsValid()` (takes the world mutex per call — see finding 5) in favor of
one lock-free pending-destruction check, or trust the flush ordering where provable.
N is small (controllers), so the win is hygiene + a pattern for hot systems, not
frame-time headroom.

### 4. GatherColliders / zone scans re-run per cloth entity — MEDIUM — **FIXED**
Colliders and wind zones now gather lazily ONCE per ClothSystem::Update and
are shared by every cloth and rope; self-exclusion moved to resolve time via
`ColliderShape.srcIndex`. Zone data snapshots only the POD fields (the
component carries std::strings). Original finding:
`ClothSystem.cpp:158-218`: 5 full component-type iterations (box/sphere/capsule
colliders + both controller types) per cloth/rope entity per frame. `SampleWeatherWind`
(`ClothSystem.cpp:267`) additionally iterates all weather zones once per cloth and once
per rope per frame (NOT per particle — see false positives). Fine at playground scale;
becomes real cost with dozens of cloths.
**Fix shape:** gather colliders + zones once per ClothSystem::Update, share across all
cloth entities that frame. Structural-change invalidation not even needed if gathered
fresh once per frame.

### 5. IsValid()/FindEntityByName take the world mutex per call — RESOLVED via #3
The locking design is untouched (deliberate per CLAUDE.md); the per-iteration
IsValid() calls in hot loops are gone — they're gated behind the lock-free
`World::HasPendingDestructions()` and only run on frames that queued destroys.
FindEntityByName remains locked (called per rope per frame — few ropes, cached
O(1) inside; not worth touching).

### 6. Cluster light array rebuilt + unreserved every frame — reserve FIXED (8faffaa6)
The reserve landed. The rebuild-gating half is deliberately NOT done: transient
point lights (muzzle flashes, elemental effects) change every frame in the
scenes where light count matters, so the dirty flag would rarely hold, and the
rebuild is a few dozen structs. Revisit only if a profiler names it.

### 7. Custom allocators are dead weight — DECIDED: keep as library, no adoption
`Core/include/Enjin/Memory/Memory.h:39-98`: StackAllocator/PoolAllocator/LinearAllocator
are referenced by exactly one file outside their implementation — their unit test.
Adopting them without profiler evidence of malloc churn would be speculative churn in
hot systems; the per-frame vectors that mattered are member/static-reused already.
They stay as a tested library for when a profiler names a real malloc hotspot.

### 8. Web shadow pass iterates all mesh entities — LOW — **FIXED**
The web Update builds one lazily-gathered caster candidate list per frame
(visible, non-empty, not a viewmodel); the fit/directional/spot/point passes
all iterate it, keeping their pass-specific filters inline. One deliberate
behavior tweak: viewmodels no longer contribute to the shadow-fit AABB (they
never cast, so fitting the volume around them was wrong anyway).
Original finding:
`RenderSystem.cpp:~1900-1936` (web half): spot-shadow pass filters post-hoc over every
MeshComponent entity; the Vulkan path uses the pre-built `m_FrameShadowCasters` list.
Port the same list to the web half.

### 9. Sim-in-render — **FIXED**
All game-view simulations (weather zones, water freeze, world time, seasons,
particles, parallax, elemental, fluid, terrain coupling, curl noise) moved out
of `RenderOffscreen` into `EditorLayer::UpdateGameViewSims`, called from the
update path right after `PlayMode::Update` with the time-scaled dt. The Game
View FPS throttle now only affects rendering; the `m_GameViewSimAccum`
workaround is deleted, and no future sim can couple to render cadence. Bonus
behavior fixes: sims no longer freeze when the Game View panel is hidden
during play, and zone-driven fog/weather state updates every frame instead of
at throttled render cadence. The weather flags the render pass consumes
(`m_GameViewWeatherParticles`, `m_GameViewIsRain`) became members. Verified:
same-binary captures byte-identical (deterministic), drift vs the pre-fix
baseline bounded at <1% (sims start on frame one instead of the first
successful render — the corrected schedule); goldens re-recorded.

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

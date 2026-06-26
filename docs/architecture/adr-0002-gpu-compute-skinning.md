# ADR-0002: GPU-Driven Compute Skinning — Skin Once Per Frame to a Buffer

## Status

Proposed (not implemented). Groundwork exists (MergedGeometryBuffer, device-generated
commands, mesh shaders). Depends on no other in-flight work; the shared-animator fix in
the FBX import session is orthogonal and already landed.

## Date

2026-06-25

## Context

### Problem Statement

Skinning today runs in the vertex shader. The CPU computes the bone (skinning) matrices
from the animator each frame and uploads them to a per-entity bone buffer; the vertex
shader (`triangle.vert`, `shadow.vert`, `outline.vert`) then transforms each vertex by up
to 8 weighted bones. That works for rasterization but has three costs:

- **Re-skinning per pass.** The same mesh is skinned again in every pass that draws it:
  main color, shadow (one re-skin per shadow-casting light/cascade), outline, and the 3D
  onion-skin ghosts. A character drawn into 4 shadow cascades plus the main pass is skinned
  5+ times per frame, each time redoing the identical bone math on the identical vertices.
- **Ray tracing cannot see skinned geometry.** A BLAS is built from vertex *positions* in a
  buffer. Vertex-shader skinning never writes deformed positions anywhere, so the RT BLAS
  holds the bind-pose mesh. Skinned characters are therefore invisible to RT shadows,
  reflections, AO, and GI, or appear frozen in bind pose. This is the blocker for "skinned
  characters in RT," which the RT pipeline otherwise supports.
- **Cross-pass divergence risk.** Because each pass skins independently from the same bone
  buffer, any pass that reads a stale or differently-populated bone buffer (shared rigs,
  follower meshes, onion-skin ghosts) can deform differently from the main pass. The
  shared-animator fix removed the clock-level cause of shadow desync; skinning once removes
  the structural possibility of two passes deforming the same mesh differently.

### Constraints

- Must not regress the existing rasterization path on hardware without compute-friendly
  paths; the vertex-shader skinning path stays as a fallback.
- Must reuse the existing 136-byte `MeshComponent::Vertex` layout (8-bone weights/indices +
  2nd UV) and the existing bone-matrix computation on the animator. This is a relocation of
  where deformation happens, not a new skinning model.
- Must integrate with `MergedGeometryBuffer` (pooled static geometry) without forcing
  skinned meshes into the pool — skinned meshes stay per-entity (see `IsPoolEligible`,
  which already excludes entities with an `AnimatorComponent`).
- The skinned output buffer must be usable both as a vertex buffer (raster) and as BLAS
  geometry input (RT), so it needs the right Vulkan usage flags
  (`VERTEX_BUFFER | STORAGE_BUFFER | ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_KHR |
  SHADER_DEVICE_ADDRESS`).
- Web/WebGPU is out of scope for v1. WebGPU keeps vertex-shader skinning. Compute skinning is
  a Vulkan-first feature, consistent with the 2nd-UV/8-bone work that is Vulkan-only by design.

### Requirements

- Skin each *distinct pose* exactly once per frame, before any pass that consumes it.
- All raster passes (main, shadow, outline, onion-skin) read the pre-skinned vertex buffer
  with identity skinning, so no pass re-deforms.
- The RT BLAS for a skinned mesh is built/refit from the same pre-skinned buffer each frame.
- Bind-pose meshes and non-skinned meshes are untouched.
- **Reuse:** when many instances share a rig and play the same clip at the same frame (the
  common "a couple rigs, reused" case, instanced crowds, synced background characters), skin
  the shared pose once and let every matching instance read it.
- **Scale:** hundreds to thousands of *bespoke* rig+animation characters, each in a unique
  pose, must be tractable. Their deformed output cannot be shared (unique pose = unique
  vertices), so the design must make per-instance skinning cheap in aggregate (few dispatches,
  one pooled output arena, LOD and culling gating the work) rather than relying on sharing.

### The sharing boundary (why this matters)

Deformed vertices are a function of `(mesh, pose)`, where `pose = (skeleton, clip(s), time,
blend params, IK)`. Two instances can share a deformed buffer only when both the mesh and the
full pose are identical. That splits the problem into two regimes the design must serve
differently:

- **Shared-pose regime** (reuse): instances deliberately synced — instanced crowds, the same
  canned loop, quantized-to-frame playback. Skin once per unique pose, share the result. Best
  served by pose dedup and/or baked animation frames.
- **Unique-pose regime** (scale): bespoke characters with their own clip phase, blend, and IK.
  Nothing to share. Served by making aggregate per-instance skinning cheap: a pooled output
  arena, one indirect compute over all live instances, bone matrices in one indexed SSBO, and
  LOD/cull so off-screen and distant characters cost little or nothing.

## Decision

Move skinning out of the vertex shader and into a dedicated compute pass that runs once per
frame per skinned mesh, writing deformed vertices to a per-entity GPU output buffer. Every
downstream consumer reads that buffer:

1. **Skinning compute shader** (`skinning.comp`), dispatched in batch. One *logical* skin job
   per distinct pose, all submitted as a small number of dispatches (ideally one indirect
   dispatch over a job list) rather than one CPU-issued dispatch per character. Inputs: the
   bind-pose vertex buffer (read from the existing geometry), a bone-matrix SSBO indexed by
   *pose id* (the CPU uploads one matrix set per distinct pose from
   `animator.GetSkinningMatrices()`), and the influence data already in the 136-byte vertex.
   Output: deformed vertices written into the pooled arena at the job's suballocation offset,
   same stride, positions/normals/tangents transformed to world space, weights/indices left
   as-is. The 8-bone transform is the math currently inlined in `triangle.vert`, moved verbatim.

2. **Pooled skinned-vertex arena, suballocated per distinct pose.** One (or a few) large GPU
   buffers, not a tiny buffer per entity, so thousands of instances are thousands of offsets in
   a handful of allocations and the whole skinning step is a few dispatches. Each live skin job
   gets a `(offset, vertexCount)` slice. The arena carries combined vertex + storage +
   AS-build-input usage so the same memory feeds raster and RT. Per-entity render data holds an
   offset into the arena, not its own buffer. This is the `MergedGeometryBuffer` idea applied to
   skinned output, and it is what makes the unique-pose (scale) regime tractable.

3. **Pose dedup for the shared-pose regime.** A skin job is keyed by
   `hash(meshId, skeletonId, clipIds, quantizedTime, blendParams)`. Instances with an equal key
   resolve to one arena slice and are skinned once; every matching instance reads that slice.
   Synced crowds and reused canned loops collapse to one job. Bespoke characters each produce a
   unique key, so they each get their own job (correctly). The key also drives the baked tier
   below. Quantizing time to the clip's frame set is what turns "different phase" crowd
   instances into shareable jobs when exactness is not required.

4. **Baked-frame tier for large crowds (vertex animation cache).** For clips that are pure
   playback (no per-instance blend or IK), sample the animation to a fixed frame set once and
   store the deformed frames in a shared read-only buffer/texture keyed by `(clip, frame)`.
   Instances then sample by `(clip, frame)` with *no* per-instance skinning dispatch at all,
   which is how the thousands-of-reused-characters case stays cheap. Bespoke hero characters
   that need live blending/IK stay on the live compute path (items 1-3) and remain fully
   dynamic and RT-visible. The two tiers coexist; an instance picks a tier from its animation
   needs, not a global switch.

5. **Raster passes read pre-skinned vertices.** `triangle.vert` / `shadow.vert` /
   `outline.vert` gain a "pre-skinned" path that skips the bone transform (the vertex is
   already deformed) and just applies view/projection. Driven by an existing-style flag
   (`FLAG_SKINNED` already exists; add `FLAG_PRESKINNED` or reuse the skinned flag to mean
   "vertices are already in world space," which is already how skinned meshes use an identity
   model matrix today — see RenderSystem.cpp:8481).

6. **RT BLAS reads the skinned arena.** The acceleration-structure manager builds (first
   frame) and refits (subsequent frames) one BLAS per distinct live pose from that pose's arena
   slice. Deduped/shared poses share a BLAS; baked-tier crowds reference the BLAS for their
   current `(clip, frame)`. Skinned instances enter the TLAS as instances pointing at the right
   BLAS, making them visible to all RT effects. Refit (not rebuild) is the per-frame cost for
   live poses, standard for deforming geometry.

7. **LOD and culling gate skinning work.** A skinned instance that is culled (off-screen, and
   not a shadow/RT caster that frame) produces no skin job. Distant instances can skin at a
   reduced bone count or drop to the baked tier. This, not buffer sharing, is the primary lever
   that keeps the unique-pose regime affordable at thousands of characters.

### Pipeline Order (per frame)

```
CPU: cull + LOD -> build skin-job list (dedup by pose key) -> per distinct pose:
     animator.GetSkinningMatrices -> upload bone SSBO[poseId] (one set per distinct pose)
GPU compute: indirect skinning.comp over the job list -> pooled arena slices
             (baked-tier instances skipped; they sample the (clip,frame) cache)
GPU (barrier: compute write -> vertex read + AS build read)
GPU raster: main / shadow / outline / onion-skin  (read arena slice, identity skinning)
GPU RT: refit BLAS per live pose from its arena slice -> rebuild TLAS -> RT passes
```

One batched/indirect compute step replaces N per-pass vertex-shader skinning evaluations
across all instances, and deduped/baked poses are skinned once or not at all.

## Alternatives Considered

### Alternative 1: Keep vertex-shader skinning, add a separate skin-to-buffer pass only for RT

Skin in the vertex shader for raster (as today), and *additionally* run a compute skin only
to feed RT. Rejected: this skins the mesh twice (vertex shader for raster, compute for RT)
plus the per-pass raster re-skinning stays. It fixes RT visibility but keeps the redundant
raster work and reintroduces the cross-pass divergence the single-source buffer removes.

### Alternative 2: Transform-feedback / stream-out from the vertex shader

Capture the vertex-shader-skinned output via transform feedback into a buffer, then reuse it.
Rejected: transform feedback is awkward in Vulkan, poorly supported on some targets, and
couples the skinned buffer to a raster draw (it would have to run a throwaway draw to
produce the buffer). Compute is the modern, portable path and is what the engine's
GPU-driven groundwork already assumes.

### Alternative 3: CPU skinning to a buffer

Skin on the CPU, upload deformed vertices each frame. Rejected: bandwidth and CPU cost for
dense rigs, and it throws away the GPU we already use for skinning. Only kept as a
conceptual fallback for headless tools, not for the runtime.

### Alternative 4: One deformed buffer per entity (naive)

The first cut of this ADR. A separate small vertex buffer allocated per skinned entity.
Rejected for scale: thousands of tiny buffers means thousands of allocations, thousands of
descriptor updates, and one CPU-issued dispatch per character. The pooled arena +
suballocation (Decision 2) gives the same correctness with a handful of allocations and a
batched dispatch, which is the difference between tens and thousands of characters.

### Alternative 5: Bake everything (vertex animation textures only)

Bake all animation to textures and never skin live. Rejected as the *sole* approach: baked
frames cannot blend, run IK, or react per-instance, so bespoke hero characters lose what makes
them bespoke, and the bake memory for many unique clips is large. Baking is adopted as a
*tier* (Decision 4) for pure-playback crowds, alongside live compute skinning for dynamic
characters, rather than as the whole answer.

## Consequences

### Positive

- Skinning cost drops from O(passes) per mesh to O(distinct poses) per frame, batched into a
  few dispatches regardless of instance count.
- Reuse falls out for free: synced crowds and reused canned loops dedup to one skin job; pure
  playback can drop to the baked tier and cost no per-frame skinning at all.
- Scales to the unique-pose case: a pooled arena + LOD/cull keeps thousands of bespoke
  characters tractable without pretending their output can be shared.
- Skinned characters become visible to the full RT pipeline (shadows, reflections, AO, GI).
- The arena slice is the single source of truth for every pass, so shadow/outline/main can no
  longer diverge from each other on a skinned mesh.
- Lands on the same pooled/indirect model as `MergedGeometryBuffer`, so skinned geometry can
  later flow through the same GPU-driven draw path as static geometry.

### Negative

- Arena GPU memory scales with the number of *distinct live poses* × vertex count × 136 bytes,
  plus baked-frame cache memory for crowds. Two bespoke characters need two slices (they deform
  differently); that is irreducible. Dedup and baking bound it for the reuse case.
- More moving parts: a pooled allocator for the arena, a per-frame pose-key dedup pass, the
  baked-frame cache and its bake step, and LOD/cull gating of skin jobs.
- A new compute pipeline + barrier in the frame graph, plus per-pose BLAS refit bookkeeping.
- Two skinning code paths to keep in sync during migration (vertex-shader fallback and
  compute), until the fallback is retired on Vulkan.

### Risks

- **Barrier correctness.** The compute-write -> (vertex-read + AS-build-read) barrier must be
  exact, or passes read partially-skinned data (a new, subtler version of the desync class).
  Mitigation: one explicit barrier after all skinning dispatches, before any consumer.
- **BLAS refit vs rebuild.** Refit assumes topology is unchanged frame to frame (true for
  skinning). Morph targets / LOD switches need a rebuild. Mitigation: rebuild on
  topology/LOD change, refit otherwise.
- **Arena lifetime across resize/scene-clear.** The pooled arena and per-pose slices must be
  torn down/reset with the rest of `EntityRenderData` on scene clear (the existing path) and
  the RT AS invalidated (already done in `FlushSceneClear`). The suballocator must handle
  instances appearing/disappearing per frame without fragmenting (ring or free-list arena).
- **Dedup-key correctness.** Quantizing time to share poses can cause visible popping if the
  quantum is too coarse, or wrongly merge two characters whose keys collide. Mitigation: only
  quantize for the baked/crowd tier where exactness is not required; key bespoke characters on
  full pose state so they never merge; treat the key as a hint that must be confirmed by a
  full compare on hash collision.

## Performance Implications

- Per-pass win scales with pass count: a character in 4 shadow cascades + main + outline goes
  from ~6 skinning evaluations to 1. Heavier scenes (many lights) benefit most.
- Per-instance win scales with reuse: in the shared-pose regime, K synced instances go from K
  skin jobs to 1; baked-tier crowds go to 0 per-frame skinning.
- Compute skinning reads bind-pose and writes the arena once per distinct pose; bandwidth is
  ~2× the vertex data per distinct pose per frame, traded against (passes−1)× redundant
  vertex-shader ALU across every instance.
- The unique-pose regime is bounded by distinct visible poses, not total characters: LOD and
  culling drop off-screen/distant skin jobs, so on-screen detailed characters set the cost.
- RT BLAS refit is per distinct live pose per frame (shared/baked poses refit once), the
  standard cost of dynamic RT geometry, and only when RT is active.

## Migration Plan

Phased so each phase is shippable and the fancy tiers do not block the core win.

**Phase 1 — correctness (single source of truth).**
1. Add `skinning.comp` (8-bone transform lifted from `triangle.vert`), compile, regenerate
   `ShaderData.h`.
2. Allocate the pooled skinned arena and give each skinned entity a slice; wire the compute
   dispatch + barrier into the frame before raster. (Phase 1 may skip dedup — one slice per
   instance — to land the path; the arena makes adding dedup later non-structural.)
3. Add the pre-skinned path to `triangle.vert` / `shadow.vert` / `outline.vert`; recompile and
   regenerate `ShaderData.h` (shader workflow in CLAUDE.md).
4. Switch raster passes to read the arena slice for skinned meshes.
5. Keep vertex-shader skinning behind a default-off feature flag as the fallback.

**Phase 2 — RT visibility.**
6. BLAS build/refit per pose from the arena slice in the AS manager; include skinned instances
   in the TLAS.

**Phase 3 — reuse (shared-pose regime).**
7. Add the pose-key dedup pass so synced/reused instances collapse to one skin job + slice.

**Phase 4 — scale.**
8. Batched/indirect dispatch over the job list; bone matrices in one pose-indexed SSBO.
9. LOD/cull gating of skin jobs.
10. Baked-frame tier (vertex animation cache) for pure-playback crowds.

Retire the vertex-shader fallback on Vulkan once Phases 1-2 validate.

## Validation Criteria

- A skinned character renders identically (within tolerance) before and after, in main,
  shadow, and outline passes.
- The same character casts a correct RT shadow and appears in RT reflections, with bind-pose
  no longer showing in RT.
- Pausing animation freezes the arena slice; all passes and the RT BLAS reflect the frozen
  pose (regression guard for the desync class).
- A second *bespoke* instance animates independently (its own slice); a second *synced*
  instance dedups to one slice (verify the skin-job count, not just the visual).
- A baked-tier crowd of N instances issues zero per-frame skin dispatches and renders correct
  poses.
- Scene clear / play-stop / resize tear down the arena and invalidate the AS with no leak or
  use-after-free.

## Related Decisions

- [ADR-0001](adr-0001-vws-override-layers.md) — unrelated, listed for ADR index continuity.
- Shared-animator (leader/follower) fix in the FBX import session removed the clock-level
  cause of shadow desync; this ADR removes the structural cause (per-pass re-skinning) and is
  the prerequisite for skinned geometry in RT.
- Groundwork: `MergedGeometryBuffer`, `DeviceGeneratedCommands`, mesh shaders.

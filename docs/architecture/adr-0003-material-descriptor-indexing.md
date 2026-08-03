# ADR-0003: Stop Mid-Recording Descriptor Updates on the Main Set (Material Indexing + UPDATE_AFTER_BIND)

## Status

Proposed (not implemented). One sub-fix has already landed (material SSBO growth moved
out of the recording path, see "Already Done" below). The larger unification described
here is deliberately deferred to a focused session with a cross-scene test pass, because
it touches the core material path and the renderer's stability pillar ("120 FPS No Matter
What") outweighs closing a rare, intermittent freeze in a hurry.

## Date

2026-08-02

## Context

### Problem statement

The editor can intermittently freeze (a hard GPU-submit hang, not a crash). It reproduces
around Play Mode Stop and was seen once during preset/Play-Mode testing. Under Vulkan
validation the freeze correlates with a storm of `VUID-vkCmd*-commandBuffer-recording`
errors, all rooted in one message:

```
VkDescriptorSet 0x... was destroy or updated without UPDATE_AFTER_BIND
```

A descriptor set bound to the in-flight command buffer is being updated (or its buffer
recreated) mid-recording. That invalidates the command buffer; per spec every subsequent
command is illegal, and drivers that tolerate it most of the time eventually hang at
submit. This is the exact hazard already called out in `CLAUDE.md`:

> `RenderSystem::FlushPendingChanges` is the ONLY safe home for destroying/recreating GPU
> resources or updating descriptor sets ... destroying/updating anything bound in the
> recording command buffer invalidates it and the driver access-violates at submit.

The editor frame order makes this easy to hit: the offscreen Scene-view pass is recorded
FIRST (binding set 0), `m_SkipMainPassRendering` is set, then `World::Update` runs. Any
descriptor write that happens after those binds but before submit is the bug.

### Where the mid-recording writes come from

All write to the per-frame main descriptor set (set 0) during `RenderToTarget` recording:

| Source | Binding | Type | Notes |
|---|---|---|---|
| Material SSBO growth (`BuildMaterialSSBO`) | 2 | STORAGE_BUFFER_DYNAMIC | Capacity-gated, intermittent. **Fixed** (see below). |
| `UpdateBoneDescriptor` | 7 | STORAGE_BUFFER | Per skinned entity, every frame. Not bindless-covered. |
| `UpdateMorphDescriptor` | 20 | STORAGE_BUFFER | Per morph entity, every frame. Not bindless-covered. |
| `UpdateNormalMapDescriptor` / `UpdateTextureDescriptor` | 3, 6 | COMBINED_IMAGE_SAMPLER | Lit 2D sprites, mid-recording. |
| `UpdateEntityTextureDescriptors` | 3/5/6/8/9/18 | COMBINED_IMAGE_SAMPLER | Dormant on this hardware (early-returns when bindless is active). |

Bindless is enabled on the target box, so the texture family is mostly dormant. The live
drivers of the freeze are `UpdateBoneDescriptor` (binding 7) and `UpdateMorphDescriptor`
(binding 20), which fire every frame for animated skinned characters. That is why the
freeze clusters in Play Mode: gameplay animates skinned rigs.

The material-SSBO growth is a separate, rarer trigger (only when the scene outgrows the
buffer, e.g. after a Play Mode Stop scene reload) and is already fixed.

### Why the material path is tangled

There are effectively three material sources today, split by draw path:

- **Direct draws:** core material via **push constant**; extended material (SSS,
  transmission, IOR, thickness, bindless texture indices) via the **binding-2 SSBO**
  selected by a **dynamic offset** (`dynOffset = materialIndex * stride`).
- **Indirect draws (>= 32 shadow casters):** core material via **ObjectData SSBO**
  (binding 13), indexed by `firstInstance` -> `gl_InstanceIndex` -> the `v_ObjectIndex`
  varying (`triangle.frag:836`).

So the engine already has a proven "index per-draw data by `firstInstance`" mechanism in
the indirect path, and already uses `UPDATE_AFTER_BIND` for bindless (set 1). The direct
path is the legacy holdout still using a dynamic offset plus mid-recording descriptor
writes.

## Decision (proposed)

Finish the migration the renderer already started. Two coordinated changes:

1. **Index the material SSBO by `firstInstance`, not a dynamic offset.**
   - Direct draws set `firstInstance = GetMaterialIndex(entity)` (currently 0).
   - `triangle.vert` forwards `gl_InstanceIndex` to the fragment shader as a new `flat`
     varying (location 11 is free).
   - `triangle.frag` declares the binding-2 SSBO as an array and reads
     `materials[thatIndex]` instead of the single dynamic-offset instance.
   - Binding 2 changes from `STORAGE_BUFFER_DYNAMIC` to `STORAGE_BUFFER`; its descriptor
     write range becomes the whole buffer; all `vkCmdBindDescriptorSets` drop the dynamic
     offset (`dynamicOffsetCount = 0`).
   - No push-constant growth. (The rejected alternative below needed a 132-byte push
     constant over the 128-byte portable minimum.)

2. **Mark set 0's updated-after-bind bindings `UPDATE_AFTER_BIND`.**
   - Once binding 2 is no longer dynamic, `VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT`
     plus per-binding `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT` become legal on the
     sampled-image and storage-buffer bindings (features `descriptorBindingSampledImageUpdateAfterBind`
     and `descriptorBindingStorageBufferUpdateAfterBind` are already enabled in
     `VulkanContext.cpp`). Add the matching `VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT`
     to `m_DescriptorPool`.
   - This legalizes the remaining mid-recording writes (bone binding 7, morph binding 20,
     sprite textures) without hoisting them out of the draw loop. The freeze goes away.

The material-index-by-`firstInstance` change is the prerequisite that unblocks UAB; UAB is
forbidden on any set containing a dynamic descriptor (`VUID-VkDescriptorSetLayoutCreateInfo-descriptorType-03001`).

### Is this "bleeding edge but stable"?

- The technique (per-draw indexing by base instance + descriptor indexing / UAB) is
  standard modern GPU-driven rendering, not exotic. The engine already uses both patterns.
- It moves the direct path toward the same model as the indirect path, reducing the
  three-source material split rather than adding to it.
- It is the spec-correct way to update descriptors after bind, which is strictly more
  stable than the current update-mid-recording-and-hope pattern.

## Alternatives considered

1. **UPDATE_AFTER_BIND only, keep the dynamic material binding.** Rejected: illegal.
   VUID-03001 forbids UAB on a set that contains any `STORAGE_BUFFER_DYNAMIC` binding, and
   binding 2 is dynamic. Attempted and reverted.
2. **Make the material binding non-dynamic, pass the index via a push constant.** Rejected:
   the push-constant block is already exactly 128 bytes (the Vulkan minimum) and its
   material fields are shared with the grass/tree/sprite_lit shaders, so nothing can be
   removed. Adding a 4-byte index yields 132 bytes, over the portable minimum. Fine on the
   RTX target, but a portability regression for a future mobile/Metal port, which the
   engine's cross-platform goal does not want to bake in.
3. **Separate UAB descriptor set (set 2) for bone/morph only.** Viable but adds a set,
   changes shader set indices, and touches the pipeline layout and recompile anyway,
   without unifying the material path. More churn for less benefit.
4. **Consolidate bone/morph into dynamic-offset buffers (like materials).** Would remove
   the per-entity bone/morph descriptor writes without UAB, but it is a skinning-system
   rework and leaves the material dynamic-offset pattern in place. Does not address the
   sprite-texture writes.
5. **Do nothing / hoist each write pre-recording.** Bone and morph buffers are inherently
   per-entity-during-draw; pre-binding them all before recording is itself a large change
   (per-entity descriptor sets or an indexed buffer), i.e. it collapses into option 1/4.

## Consequences

- Positive: eliminates the whole `commandBuffer-recording` error class and the
  intermittent freeze; brings the direct path in line with the indirect path; keeps the
  push constant at 128 bytes; uses only already-enabled device features.
- Cost/risk: it is a core material-path change with a shader recompile. Regressions would
  show as wrong materials on specific scene types, which is worse and subtler than a rare
  freeze. Must be validated across the matrix below before commit.
- Indirect-path note: in indirect mode `gl_InstanceIndex` is the object index, so
  `materials[gl_InstanceIndex]` assumes the material array aligns with the object index, or
  that indirect draws keep sourcing extended material as they do today. This alignment must
  be verified or the indirect path left on its current source explicitly. The current
  indirect path already shares/fixes the extended material via a single dynamic offset, so
  it is not made worse, but the new indexing must not silently mis-map it.

## Already done (landed, keep)

`RenderSystem::EnsureMaterialSSBOCapacity()` now performs the material SSBO growth (buffer
recreate + binding-2 descriptor rebind) inside `FlushPendingChanges`, the safe
pre-recording window, instead of inside `BuildMaterialSSBO` during recording. It also grows
**all** frame-in-flight buffers (the old code grew only the current frame's buffer while
setting the shared capacity, so other frames' buffers could overflow). `BuildMaterialSSBO`
now clamps defensively if capacity is somehow short and never recreates buffers or
descriptors mid-recording. This removes one real (intermittent) freeze source and a latent
per-frame-buffer bug. It is orthogonal to the indexing change and should stay regardless.

## Test plan (run before committing the indexing + UAB change)

Launch under `_validate.ps1` (forces validation on the Release build) and confirm zero
`commandBuffer-recording` and zero `destroy or updated` descriptor errors, plus correct
visuals, on each of:

- [ ] Static textured 3D meshes (base color, normal, metallic-roughness, emissive).
- [ ] Skinned animated character in Play Mode (drives binding 7); enter and Stop several
      times, including a Stop where an entity is destroyed during play (full-reload path).
- [ ] Morph-target mesh (drives binding 20).
- [ ] Lit 2D sprites with normal maps (drives bindings 3/6).
- [ ] A scene with >= 32 shadow casters (forces the indirect path) to confirm material is
      not mis-mapped by the new `gl_InstanceIndex` material indexing.
- [ ] Transparent / alpha-cutout materials (dynamic offset previously selected per-entity;
      confirm sorting and per-entity material still correct).
- [ ] SSS / transmission material (extended SSBO fields) to confirm the array read matches
      the old dynamic-offset read.
- [ ] Multi-material sub-meshes (`MaterialSlotsComponent`) so per-sub-mesh `firstInstance`
      indices are correct.
- [ ] Scene that outgrows the material SSBO capacity (many entities) to exercise the
      already-landed growth path together with the new indexing.

Shader workflow reminder: edit GLSL, `glslangValidator -V`, then `python _gen_all.py` to
regenerate `ShaderData.h`, then rebuild the engine, editor, AND the `EnjinPlayer` target
(exported games ship a prebuilt player).

## References

- `CLAUDE.md` -> Frame Safety, Renderer (binding 2 dynamic-offset rule).
- `Engine/src/Renderer/Vulkan/VulkanPipeline.cpp` -> `CreateDescriptorSetLayout` (set 0).
- `Engine/src/ECS/Systems/RenderSystem.cpp` -> `BuildMaterialSSBO`,
  `EnsureMaterialSSBOCapacity`, `UpdateBoneDescriptor`, `UpdateMorphDescriptor`, the main
  descriptor pool (`m_DescriptorPool`), and the direct-draw bind/`vkCmdDrawIndexed` sites.
- `Engine/shaders/triangle.vert` / `triangle.frag` -> `gl_InstanceIndex`, `v_ObjectIndex`,
  ObjectData (binding 13), MaterialSSBO (binding 2).
- ADR-0002 (GPU-driven compute skinning) is the adjacent migration this completes on the
  material side.

# The RenderSystem backend split

`Engine/src/ECS/Systems/RenderSystem.cpp` is 20,558 lines under one
`#if ENJIN_RENDERER_WEBGPU` / `#else` / `#endif`:

| Region | Lines | Contents |
|---|---|---|
| 58 – 4332 | 4,274 | WebGPU implementation |
| 4332 – 20367 | 16,035 | Vulkan implementation |
| 20367 – 20558 | 191 | shared |

A backend only compiles its own half, so a change in one is invisible to the
other and the compiler cannot tell you.

## The drift surface

50 methods are defined in both halves. 8 exist only on web, 165 only on Vulkan.

The twinned 50 are 3,337 lines of web against 2,851 of Vulkan. Almost all of it
is four methods:

| Method | Web | Vulkan |
|---|---|---|
| `Update` | 2,346 | 1,463 |
| `Initialize` | 730 | 506 |
| `Shutdown` | 102 | 200 |
| `FlushPendingChanges` | 3 | 310 |

`Update` alone is 3,809 lines across the two copies. Every feature that has to
appear in both backends is written twice inside it, and nothing checks the two
against each other.

## What this costs, measured

Bugs found on 2026-09-05, all of the same shape:

- Snow reached the PBR shader and not the vegetation shaders.
- Geometry outlines existed on Vulkan and not on web, while
  `SceneRenderSettings` wrote the settings into both.
- `TickHighlightTime` was called in the web `Update` and read only by the Vulkan
  outline pass, so hover highlights animated on neither.
- `m_WebGrassPipeline` and `m_WebTreePipeline` were built and logged every boot
  and never drawn; their shaders were edited that morning with no effect.
- The same per-frame buffer-and-bind-group idiom was reinvented at six sites.

## Why the obvious move is wrong

Deleting the `#else` and calling a backend interface everywhere fails on the
part that is genuinely different. The web half opens render passes with raw
`wgpuRenderPassEncoder*` calls, keeps its own bind-group layouts, and drives a
pass-hook the player owns; the Vulkan half records into a `VkCommandBuffer`,
carries bindless descriptors, a geometry pool, a shadow atlas, RT and clustered
lighting. `IRenderBackend` covers buffers, textures, pipelines and bind groups.
It does not cover pass structure, and pass structure is most of what these two
copies disagree about.

## The order that works

**1. Stop new drift.** A feature landing on one backend and not the other is the
recurring bug. `tools/shader_parity.py` already does this for shaders — it
compares the two backends' effect lists and exits non-zero on divergence, and it
has caught real cases. The same check over `RenderSystem` methods: parse both
regions, and fail when a method appears in one and not the other without an
explicit allow-list entry saying why. Cheap, and it makes the next drift a build
failure instead of a bug report.

**2. Move the shared logic out of `Update`.** Most of those 3,809 lines are not
backend work — they are LOD selection, culling, sorting, animator resolution,
draw-order decisions, per-entity flag packing. That is backend-agnostic and can
move into shared methods above the `#if`, leaving each half with only the pass
recording. This is the step that shrinks the drift surface, and it can be done
one method at a time with the suite green between each.

**3. Only then consider a pass abstraction.** With shared logic hoisted, what
remains in each half is genuinely backend-shaped. Whether that deserves an
`IRenderPass` seam is a question worth answering with the two halves already
small, not with them at 3,800 lines.

## Scope

Step 1 is a day. Step 2 is weeks, but it is incremental and every increment is
independently shippable. Step 3 should not be scheduled until step 2 is done.

None of it belongs next to a release. The value of writing it down now is that
step 1 stops the bleeding for the cost of a day, and it can be done at any time.

# Recreating the desktop effects on web, cheaply

> Measured 2026-09-05. `build-web` had **no CMAKE_BUILD_TYPE**, so every web
> build shipped unoptimised. Setting Release took the wasm from 12.0 MB to
> **11.25 MB** - only 6%, far less than the 3-5 MB that was predicted, because
> the binary is dominated by what is LINKED, not by codegen. `assimp` (FBX,
> Collada, glTF parsers) and `imgui` are both live in the player wasm; strings
> for `Deformer.Fbx`, `Collada: Found unsupported <bind>` and `imgui_log.txt`
> are all in the shipped binary. A player that loads pre-packed assets needs
> neither. That, and `-Os` over `-O3`, are the real size levers. The Release
> switch is still worth keeping for the runtime speed.

`python tools/shader_parity.py` reports the split:

```
GLSL (desktop / Vulkan) : 60 effects, 74 stage files
WGSL (web / WebGPU)     : 13 effects, 25 stages
compute shaders         : desktop 36, web 0
```

Fifty-one effects have no web implementation (fifty-two before `outline`
landed). The instinct is to port them. That is the wrong instinct, and this
document is the argument for why plus what to do instead.

## The reframe

Most of the 52 exist to make **physically based rendering fast at scale**: ray
traced GI, visibility buffers, GPU culling of a hundred thousand draws, virtual
texturing, variable rate shading. They are answers to problems a stylised game
does not have.

Whistland packs to **130 KB**. Its scene is 79 entities, 3,162 triangles, flat
shaded vertex colour, one directional light. Porting ReSTIR to that is not an
optimisation, it is a category error.

Thirty-six of the desktop shaders are compute. WebGPU compute exists but the web
renderer has none, and the effects that need it are exactly the ones a cel-shaded
game does not want. So the useful question is not "how do we port these" but
**"which of these is the art direction actually asking for, and what is the
cheapest thing that reads the same?"**

The answer is about nine of them, and every one is fragment or vertex only.

---

## Bucket 1 — art direction critical

These change how the game LOOKS. Missing them is why web would not match.

### `outline` -> inverted hull. SHIPPED
The single most important one. A Wind Waker read needs outlines and there was no
web implementation at all.

Worse than missing: `SceneRenderSettings::ApplyToRuntime` already wrote
`geometryOutlinesEnabled` / `Width` / `Color` into the web RenderSystem, and the
scene file already round-tripped them. Nothing on web read them. A cel-shaded
project exported to the browser lost its outlines and no runtime said a word.

What shipped is the inverted hull: `OUTLINE_WGSL` draws the mesh again with
front faces culled, vertices pushed along their object-space normal by the
outline width, in flat colour. Vertex shader only, no postprocess pass, no depth
prepass, no new render target, and it follows the skeleton so an animated
character's outline does not stay behind in the bind pose. It reuses the main
pass's frame and object bind group layouts by repurposing `ObjectData.baseColor`
as the outline colour and `.metallic` as the width, which is the same trick
outline.vert plays with push constants on Vulkan, so the two paths cannot drift
apart in what they read. Consecutive entities sharing a mesh instance into one
draw, so an outlined crowd costs one call, not one per body.

Priority order matches RenderOutlinePass exactly: per-material, then a cel
ArtStyle override, then the global setting, with a hover highlight beating all
three. That last one means `HoverHighlightComponent` now works on web.

The other route, **edge detect** in the postprocess pass, is still open and
still worth it later: it catches the interior edges a hull misses. It needs a
normal target, which the web MRT pass does not have yet.

### `skinning` -> vertex shader skinning. ALREADY TRUE
Listed here first time round as missing. It is not. `PBR_WGSL` already does the
four-weight matrix palette in the vertex shader, gated on FLAG_SKINNED, and the
web vertex layout carries `boneWeights` / `boneIndices`.

What the parity report counts is `skinning.comp`, and that is a compute shader
with no web path (`--compute-skinning` is a desktop flag). The report is
counting files, so a desktop effect implemented inline on web reads as a gap.
Worth remembering when reading the 52: some of them are already answered under a
different name, which is the same trap the 2026-08-28 audit hit.

Vulkan skins with eight weights (two sets of four), web with four. That is a
real difference and it will show on a mesh authored past four influences.

### `oit_composite` -> weighted blended OIT. WIRED
Sails, water, foam and spray all need transparency that does not pop when it
sorts wrong. Two blended surfaces that intersect have no correct per-object
order, so the order flips as the camera moves and the image jumps. Weighted
blended OIT (McGuire/Bavoil) accumulates instead of sorting, and cannot pop
because there is no order to get wrong.

PBR_WGSL's fragment entry point was five hundred lines with the shading inline.
It is now `shadeSurface()`, with `fs_main` a one-line delegate and `fs_oit`
the second entry point, so a transparent surface is lit by the same code as an
opaque one and the two cannot drift.

The frame gained two passes after the scene pass. Accumulation writes
premultiplied colour times the paper's depth weight into an RGBA16Float target
(additive) and alpha into an R8Unorm revealage target (dst *= 1 - src, cleared
to 1). The composite resolves the pair back over the opaque scene with
src = OneMinusSrcAlpha, dst = SrcAlpha, which is the paper's formula evaluated
by the blender. Depth is loaded and never written, so transparency is hidden by
opaque geometry in front of it and never hides other transparency.

Three things worth knowing:

- **It is lazy.** The targets and pipelines are built the first frame a scene
  actually contains a blended entity. A fully opaque scene allocates nothing
  and renders exactly as before. If the targets fail to build, the draw loop
  keeps drawing blended entities the old sorted way rather than dropping them.
- **Pass order is load-bearing.** It runs after the scene pass, which is after
  the sky. The sky is a z = 1 fullscreen triangle, so transparency drawn before
  it is painted over.
- **`GPURenderPipelineDesc` had to grow.** It hardcoded `fs_main` and carried
  one `colorFormat` and one `blendState`, and OIT's two targets blend
  differently by definition. It now takes a `fragmentEntryPoint` and a list of
  extra colour targets. Note for whoever touches the pipeline manager next: a
  `WGPUColorTargetState` holds a POINTER to its blend state, so the vector of
  blend states is reserved up front - a reallocation there would leave every
  target pointing at freed memory.

**Unverified.** It compiles, it boots, and no demo scene has been checked with
real transparency in front of a human yet.

---

## Bucket 2 — the perf levers

### `upscale_*` -> render scale + CAS. ALREADY SHIPPED
The biggest web performance win available, and it was already in the tree before
this document was written. `RenderSystem::ApplyRenderScale` clamps to 0.5 - 1.0,
sizes the web scene target through `WebScaledDim`, and turns the sharpener on
below 1.0; `sharpenCAS` in POSTPROCESS_WGSL is a four tap contrast adaptive
sharpen with a clamp to the local min/max, so it does not halo.

EASU is a complex gather and is still not worth porting. At 0.7x you draw half
the pixels, which is why nothing else on this list comes close.

### `cull`, `cull_hiz`, `hiz_generate`, `light_cluster_*` -> CPU cull
GPU culling exists so a hundred thousand draws can be culled in a compute pass.
Web scenes will not have a hundred thousand draws. A CPU frustum cull over a
coarse spatial grid costs microseconds at these counts and needs no shader at
all. Light clustering matters past a few dozen lights; until then, a plain loop.

### `taa_resolve` -> FXAA, or supersample
TAA needs motion vectors, a history buffer and a reprojection pass, and it ghosts
badly on high contrast stylised content, which is exactly what a cel look is.
Use FXAA (one fragment shader) or, where there is headroom, render scale above
1.0 and downsample. The second is better looking and simpler.

---

## Bucket 3 — mood, with analytic stand ins

### `volumetric_fog` -> analytic height fog + radial blur
A raymarched froxel volume is compute. Analytic exponential height fog is a few
lines in the fragment shader and **is already there**: PBR_WGSL reads
`fogParams` and applies height-based distance fog, matching Vulkan.

What is missing is the god rays. A radial blur from the projected sun position
over a bright pass, fragment only, and for open water it reads better than a
froxel grid anyway.

### The GI stack -> hemisphere ambient
`ddgi_probe_update`, `ddgi_sample`, `restir_initial/spatial/temporal`,
`surfel_lookup/placement/update`, `radiance_cache_read/update`,
`svgf_atrous/temporal/variance`, `rt_composite`, `rt_hybrid_apply`,
`rt_temporal_reuse`, `adaptive_ray_budget`, `adaptive_ray_variance`.

**Seventeen of the fifty-two are one feature**: ray traced global illumination
with denoising. A cel shaded game does not want it. Physically correct bounce
light fights flat colour and hard shading bands; it is the opposite of the look.

Replaced with **hemisphere ambient**, already shipped: PBR_WGSL builds a sky
dome irradiance from the scene's configured sky and mixes it into the flat
ambient term by the surface normal's Y. It mixes rather than replaces, anchored
to the scene's tuned ambient level, because replacing it made sky-configured
builds read dimmer than flat-ambient ones. No probes, no rays, no denoiser. If
more is wanted later, bake spherical harmonic probes on the CPU at load time.

### `weather_particle` -> instanced billboards
Rain and snow through the existing particle system, which already runs on web
(`WebGPUParticleSystem`, max 65,536 particles). No new shader.

---

## Bucket 4 — do not port

`gpu_voxelize`, `vt_feedback`, `vt_resolve`, `vrs_generate`, `dgc_generate`,
`material_resolve`, `visibility`, `splat`, and everything already folded into the
GI item above.

Virtual texturing, visibility buffers, variable rate shading and device generated
commands are answers to scale problems. They cost more to maintain on a second
backend than they can possibly return on a game that fits in 130 KB.

---

## Order of work

1. ~~**Render scale + CAS.**~~ DONE, and it was done before this document was
   written. `RenderSystem::ApplyRenderScale` sizes the web scene target and
   turns the sharpener on below 1.0; `sharpenCAS` is in POSTPROCESS_WGSL.
2. ~~**Outline (inverted hull).**~~ DONE. See the bucket 1 entry.
3. ~~**Hemisphere ambient.**~~ DONE, also before this document. PBR_WGSL mixes a
   sky-dome irradiance into the flat ambient term when the scene has a
   configured sky.
4. ~~**Weighted blended OIT.**~~ WIRED. See the bucket 1 entry.
5. ~~**Vertex skinning.**~~ ALREADY TRUE, see above. Eight-weight parity is the
   only piece left and no content needs it yet.
6. ~~**Analytic fog.**~~ DONE, height-based distance fog is in PBR_WGSL.
   **Radial god rays** are not, and are still worth one fragment pass.
7. **CPU cull.** Only when draw counts justify it.

So the list was three items long, not seven, and all three are closed.
Check what the web renderer already does before adding to it: the audit habit of
grepping for the FEATURE rather than the SHADER NAME would have caught three of
these before they were written down as work.

Every one is fragment or vertex only. None needs compute. Together they are
perhaps a tenth of the work of porting the 52, and they get closer to the
intended look than the 52 would, because the 52 are built for a different look.

## The web renderer never freed GPU memory

Found 2026-09-05, chasing a live demo that froze in Firefox with:

```
Uncaptured WebGPU error: Not enough memory left.
Uncaptured WebGPU error: In a set_bind_group command, caused by: BindGroup with '' label is invalid
Uncaptured WebGPU error: Buffer with '' label is invalid
```

`WebGPURenderer::DestroyBuffer` called `wgpuBufferRelease` and
`DestroyTexture` called `wgpuTextureRelease`. Neither `wgpuBufferDestroy` nor
`wgpuTextureDestroy` appeared anywhere in the engine. Release only drops the
JavaScript reference; the GPU allocation behind it survives until the garbage
collector runs, and the collector is looking at a tiny wrapper object rather
than the megabytes it owns, so it is in no hurry.

A web frame creates on the order of a hundred transient buffers, one per shadow
caster and one per draw batch, each with a bind group. The GPU budget is gone
long before anything triggers a collection, allocations start failing, and
every bind group built on a failed buffer is invalid. That is the cascade
above, in order.

The fix cannot destroy in place. Measured against Dawn:

```
ok     destroy AFTER submit
ERROR  destroy BEFORE submit: [Buffer (unlabeled)] used in submit while destroyed.
```

and the transient buffers are destroyed mid-frame by design. So destroys are
queued and drained at the top of the next frame, once the submit that used them
has happened. `RenderSystem`'s web `Update` now also logs live and created
buffer counts once a second, because the report arrived with no way to tell a
leak from ordinary churn: a flat live count next to a large total is recycling
working as intended, a climbing live count is a leak.

Not yet confirmed against the original failure. The mechanism is proven and the
fix is deployed; whether it is the whole of what froze that session is open
until someone runs it again.

### The other half: not allocating in the first place

Reclaiming properly still left the frame creating and destroying roughly a
hundred buffers and a hundred bind groups. `m_WebObjectArrayBuf` /
`m_WebObjectArrayBG` / `m_WebObjectArrayGen` were sitting in the header with a
comment describing exactly the design that removes it, alongside
`EntityRenderData::objBoneBindGroup` and `objBoneBindGroupGen` for the skinned
case, and not one of the five was referenced anywhere but shutdown. The
optimisation had been written, documented, and then lost.

It is back. The frame's ObjectData is packed into one persistent storage
buffer, grown on demand, uploaded once, bound once. Records are reordered into
draw order first so an instanced batch is contiguous, and each draw addresses
its slice with `firstInstance`. Skinned entities are the one case that cannot
share the bind group, because binding 1 is their own bone buffer; theirs is
cached on the entity and rebuilt only when the shared buffer moves, which is
what the generation counter is for.

Two things this rests on, both measured against Dawn rather than assumed:

```
ok    firstInstance=0 -> instance_index=0
ok    firstInstance=3 -> instance_index=3
ok    firstInstance=7 -> instance_index=7
```

and the destroy-ordering result above. If `instance_index` had been 0-based per
draw, every entity would have read row zero and the whole scene would have
collapsed onto one transform.

The padding had to go with it. ObjectData was stride-256 because each entity
had its own uniform binding and 256 is `minUniformBufferOffsetAlignment`. A
storage array's stride is the struct size, so 256 would have made every index
read the wrong row.

The shadow passes went the same way. They were the worse offender: a buffer
and a bind group per caster for the directional pass, again per caster for
every spot light, and six more times per caster for a point light. Fifty-six
casters and one point light is over four hundred of each, per frame.
SHADOW_WGSL now reads a storage array of model matrices - the depth pass only
ever wanted the model matrix, so it carries matrices rather than a copy of the
full ObjectData - and the caster list is identical and identically ordered in
all three passes, so one upload serves every one of them and the row is just
the caster's index.

**Unverified.** This one is behaviour-visible: get `firstInstance` wrong and
every object renders with another object's transform. Dawn confirms the
semantics but nothing here has rendered a frame of it.

## The depth buffer, which unblocked five more effects

The roadmap carried web depth-accurate post-processing as BLOCKED: "web scene
depth is 4x MSAA and the renderer bind abstraction can't bind a multisampled or
unfilterable-float texture; needs a depth pre-pass or abstraction work."

That was true when it was written and stopped being true on 2026-09-03, when
MSAA came off the web scene target. The depth buffer has been a plain
single-sample texture ever since, and the bind abstraction has had a
`DepthTexture` binding type all along - the shadow sampler uses it. What
actually remained was one usage flag and a binding.

It now carries `TextureBinding` alongside `RenderAttachment`, plus a second view
of the same texture with `aspect = DepthOnly`, because a depth-stencil format
has to be viewed one aspect at a time to be read in a shader. The post-process
pass binds it as `texture_depth_2d` and reads it with `textureLoad`, so it needs
no sampler at all.

Two effects went in on top of it:

- **SSAO is now real.** It shipped as a LUMINANCE approximation - darken
  whatever sits in a local brightness valley - which grounded objects but also
  darkened any dark patch of texture whether or not anything was occluding it.
  It now asks the geometry: a neighbour nearer than this pixel is an occluder.
- **Cel outline**, Sobel on linear depth. This is the edge-detect route this
  document left open beside the inverted hull: the hull draws the silhouette,
  this catches the interior edges a hull cannot. It runs off the `celOutline*`
  fields that were already in `SceneRenderSettings` and inert on web, so it
  needed no new setting.

Depth is linearised against the camera's near and far planes, fed per frame. A
wrong pair does not make the effects inaccurate, it makes every depth
comparison meaningless, which is why they are passed rather than assumed.

**DoF, god rays and tilt-shift are now unblocked and unbuilt.**

## Keep the parity report honest

`python tools/shader_parity.py --strict` exits non-zero when a shared effect has
a stage on one backend and not the other. Run it before hand writing another
WGSL shader. The twelve shared effects are maintained twice by hand and nothing
else in the build compares them; two real divergences were found on 2026-09-04
and neither was deliberate.

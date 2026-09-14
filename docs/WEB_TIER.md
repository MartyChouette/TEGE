# The web tier

**Web is a different tier, not a reduced desktop.**

That is the standing position and it is the right one, but until now it lived as
a sentence rather than as a list, which meant "runs on web" was a claim with no
defined content. A maker could not find out what their game would look like in a
browser without shipping it and looking.

This document is the contract. For every capability it says one of three things:

| | Meaning |
|---|---|
| **Same** | Works on web the way it works on desktop. |
| **Substituted** | Web gets a *different, deliberate* implementation. The substitute is named. This is not a downgrade, it is the web-native way to get the same result. |
| **Absent** | Not on web. Authoring it will silently do nothing in a browser build. |

**Absent is the row that matters.** A maker who knows a feature is absent designs
around it in ten seconds. A maker who finds out at the end has built on sand.

Last verified against the tree 2026-09-14.

---

## Rendering

| Capability | Web | Notes |
|---|---|---|
| PBR materials, lit meshes | Same | |
| Shadows | Same | |
| Skybox, procedural sky, clouds | Same | Draw order differs: the sky is a fullscreen triangle at z=1 that must be drawn AFTER opaque and BEFORE anything alpha-blended. |
| GPU particles | Same | `WebGPUParticleSystem.cpp`, compute-driven, verified in a browser. |
| Vegetation, wind | Same | `WebGPUVegetationSystem.cpp`. |
| Tilemaps | Same | Fixed 2026-09-13. It had never worked: mesh generation lived only in the Vulkan `RenderSystem::Update` body. |
| Sprites, 2D | Same | |
| Compute | Same | |
| CPU ParticleRenderer visuals | **Absent** | |
| Decals, trails, line renderers | **Absent** | |
| 3D text | **Absent** | Use a UI canvas label positioned in screen space. |
| Terrain auto-mesh | **Absent** | Author the mesh and ship it. |
| Morph targets | **Absent** | |
| Custom shader graphs | **Absent** | |
| Reflection systems (probes, planar, SSR) | **Absent** | |
| Ray tracing, path tracing, DDGI | **Absent** | Not a gap: no browser exposes the hardware. Use the substitutions below. |

### Substituted, and why the substitute is the right answer

| Desktop | Web | Why |
|---|---|---|
| DDGI / realtime GI | Baked radiosity normal mapping (`surfaceParam1` band 600) | Baked light is authored light. It looks *better* than realtime GI for a fixed scene and costs nothing per frame. |
| RT reflections | Light cookies, flipped-floor and matcap reflection styles | Hand-crafted reflections do not break the fiction at the frame edge, which is why they are preferred on desktop too. |
| Realtime lit set-dressing | Pre-rendered backgrounds with a depth plate | Fixed-camera scenes get more image quality this way on any platform. |
| Colour grading LUTs at cost | Palette cycling and indexed palettes | Cheaper AND a distinct look, rather than a compromise. |

**This table is the point of the whole document.** Four of the five things a
browser cannot do have a web-native answer that is not a worse version of the
desktop one. Anything added to the Absent list should get a row here or an
explicit "no substitute exists".

---

## Materials

Roughly a quarter of `MaterialGPU`'s feature set is honoured on web. The
material struct is 144 bytes and shared, so a field always *serializes*; whether
the web shader reads it is the question, and today most do not.

Known-working on web: base colour, metallic/roughness, normal maps, emissive,
alpha modes, vertex snapping, dither modes, palette-indexed, lightmapped.

**Everything else on a material should be assumed absent on web until it has a
row in this table.** That is deliberately pessimistic, because the failure is
silent: the field saves, the scene loads, the look is wrong, and nothing says so.

---

## Runtime

| Capability | Web | Notes |
|---|---|---|
| AngelScript, all bindings | Same | Bindings must use the `ENJIN_AS_*` macros; raw `asFUNCTION` fails on WASM. |
| Physics, 2D and 3D | Same | |
| Audio | Same | Browsers require a user gesture before any sound. |
| Save system | Same | IndexedDB-backed `/saves/`, synced after each write. |
| Input, touch, rebinding | Same | Browser key events land between frames; the web path latches edges. |
| Level streaming | Same | Lazy pak filesystem. |
| Texture filtering settings | **Absent, silently** | Discarded on web; pixel art blurs in a browser. This one is a bug, not a tier decision. |

---

## Rules for anyone adding a feature

1. **Check the two-Update-bodies shape first.** `RenderSystem::Update` exists
   twice, once per backend, under one `#if`. Most "web gaps" have been a call
   site nobody added rather than a port anyone had to write. Tilemaps were three
   lines and had been missing for months.
2. **If it cannot work on web, say so in the Absent table** in the same change.
   An undocumented absence is a silent failure with a long fuse.
3. **Prefer a substitution to a gap.** The four rows above are all cases where
   the web answer turned out to be the better answer.
4. **Verify in a browser**, not in a green build. WGSL is compiled by the
   browser: `cd tools && npm install && node check_wgsl.mjs` compiles all 13
   shaders through Dawn, and `tools/web_capture.mjs` renders a build in headless
   Chrome.

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

Last verified against the tree 2026-09-15.

---

## Rendering

| Capability | Web | Notes |
|---|---|---|
| PBR materials, lit meshes | Same | |
| Shadows | Same | |
| Skybox, procedural sky, clouds | Same | Fixed 2026-09-15 and verified by capture. It was silently broken for the life of the web player: the boot scene loads BEFORE the RenderSystem is constructed, so `SetSkybox` hit its own null guard and every browser scene drew the built-in default. A scene authoring a red zenith rendered byte-identical to one authoring blue. |
| GPU particles | Same | `WebGPUParticleSystem.cpp`, compute-driven, verified in a browser. |
| Vegetation, wind | Same | Verified by capture 2026-09-15: renders, sways, and casts shadows. |
| Tilemaps | Same | Fixed 2026-09-13. It had never worked: mesh generation lived only in the Vulkan `RenderSystem::Update` body. |
| Sprites, 2D | Same | |
| Compute | Same | |
| CPU particle emitters | Same | `ParticleEmitterComponent` renders through the web particle pipeline (`RenderSystem.cpp`, the web `Update` body), capped at 8192 instances shared between emitters. The Vulkan `ParticleRenderer` CLASS is absent, which is what this row used to say -- and it read as the capability being gone, which it is not. What IS missing is the emitter `texturePath`: web particles are untextured coloured billboards. Use a GPU emitter for textured particles. |
| Decals, trails, line renderers | **Absent** | |
| 3D text | **Absent** | Use a UI canvas label positioned in screen space. |
| Terrain auto-mesh | **Absent** | Author the mesh and ship it. |
| Morph targets | **Absent** | |
| Custom shader graphs | **Absent** | |
| Reflection systems (probes, planar, SSR) | **Absent** | Includes the water-surface reflection on `WaterVolumeComponent` and Water3D's Reflective/Refractive styles: the mirror is real geometry redrawn through the Vulkan ghost path, so a browser draws none of it. The surface itself is unaffected. |
| Water volumes (surface, waves, translucency) | Same | Verified in a browser 2026-09-15 by capture, not by reading guards. Surface draws, an authored opacity blends over the bed, and the Gerstner-lite wave displacement runs. The waves needed fixing to get there: PBR_WGSL had carried the displacement all along but nothing on the web path set `FLAG_WATER_SURFACE`, so frames 60 and 400 of the same scene were byte-identical while desktop differed. Bit 5 means the same thing on both backends; bits 6 and 7 do NOT. |
| Water shore foam | Same | Shipped 2026-09-15 and verified in a browser. It needed both halves: the three parameters (`shoreWidth`, `foamIntensity`, `foamScale`) are `surfaceParam1/2/3` push constants on Vulkan and had no home in `WebObjectDataUBO` at all, so the struct grew 144 -> 160 bytes in lockstep with both WGSL `ObjectData` declarations (now guarded by a `static_assert`). Gated on the parameters rather than a flag bit, because the web flags word does not share bits 6 and 7 with Vulkan. |
| Shadows in a static scene | Same | Fixed 2026-09-15. The web shadow map caches and redraws only when its caster signature moves. Entity GPU buffers are created lazily in the main draw loop, which runs AFTER the shadow pass, so on the first frame every caster was skipped for having no buffers yet -- and the empty map was then cached. A scene where nothing moves kept it for the life of the process. It only looked fine in ShadowCheck because the player moves and dirtied the signature a frame later. |
| 2D scene water | Same | Shipped 2026-09-16, ported from `water2d.frag` and verified in a browser: wavy foam line, depth tint, caustics, and it animates. It needed three things, not one: the shader, a config path (the web player never called `SetWater2D`, and the member was declared in the Vulkan-only half so there was nowhere to put it), and an orthographic camera. |
| Baked fluid playback | Same | Shipped 2026-09-17. `FluidPlaybackComponent` decodes a recorded take and hands it to the simulation, so the existing web billboard path draws it with no change. A recording is a file the pak carries like any other asset; resolution costs nothing at runtime here, which makes a browser the tier that benefits MOST from baking. |
| Fluid simulation and rendering | Same | Shipped 2026-09-16. The simulation was not running here at all -- it was deliberately left unwired while `SetFluidSimulation` was a no-op stub, since ticking a sim nothing can draw is pure cost. Cells are camera-facing billboards, so the existing web sprite pipeline draws them; no new shader or pipeline. Cell selection mirrors `FluidRenderer::Render` (same density threshold, same sizing) so both backends pick the same cells, capped at 20,000 per frame. |
| Orthographic cameras | Same | Fixed 2026-09-16. The web player only ever called `SetPerspective` and never looked at `projectionType`, so EVERY 2D scene was drawn through a 60-degree frustum: sprites took perspective foreshortening, and anything reconstructing world position from the projection got a half-height of 0.577 where the author asked for 9. |
| Frustum culling | Same | CPU, shared with desktop since 2026-09-14. Desktop ALSO has GPU culling (compute + indirect draw) for very large object counts; web has the test, not the dispatch. |
| Mesh LOD | Same | Shared since 2026-09-14. Web previously used plain camera distance and had neither `useScreenSize`, `lodBias` nor `forceLowestLOD`. |
| Animation LOD | Same | Shared since 2026-09-14. Web previously refreshed every animator every frame; the flag was declared inside `#if !ENJIN_RENDERER_WEBGPU` and did not exist in a web build. |
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
| Audio | Same | With one timing rule that has no desktop equivalent: **no clip loads until the page has seen a user gesture.** Before one, `Audio_GetLength` returns -1, `Audio_IsPlaying` is false and `Audio_Seek` returns false, so a game that queries or seeks audio in its opening seconds silently gets nothing. After a click the clip registers immediately. `Audio_Seek` itself works (verified 2026-09-16), landing about 31 ms short of the target where desktop is exact -- Web Audio buffer granularity. |
| Save system | Same | IndexedDB-backed `/saves/`, synced after each write. |
| Input, touch, rebinding | Same | Browser key events land between frames; the web path latches edges. |
| Level streaming | Same | Lazy pak filesystem. |
| Multiplayer / networking | **Absent** | A browser build has no network transport at all: `TransportFactory` returns `nullptr` on web, because the WebSocket transport the enum and headers describe is not implemented. Desktop multiplayer is UDP on a LAN or a port-forwarded direct IP. Planned in adr-0007 (hosted relay + room-code matchmaking); until then, do not design a web game around multiplayer. |
| Local / couch co-op (one machine) | Same | Multiple gamepads work: the Emscripten HTML5 Gamepad API is wired in `Core/src/Platform/Input.cpp` and sampled every frame. A shared-screen co-op game needs no network and no server on web. |
| Splitscreen | **Absent** | `RenderSplitscreen` is defined only in the Vulkan half of `RenderSystem.cpp` and appears nowhere in the web half. Shared-screen local co-op works; split views do not. |
| Texture filtering settings | **Absent, silently** | Discarded on web; pixel art blurs in a browser. This one is a bug, not a tier decision. |
| Particle textures | **Absent** | Web particles are procedural soft circles: PARTICLE_WGSL's fragment stage computes a radial falloff and samples no texture at all, so an emitter's `texturePath` -- and with it the sprite sheet -- does nothing in a browser. Desktop binds the texture and animates the sheet per particle. Tint, size, lifetime and motion are the same on both. |
| Hand IK (`HandIKComponent`) | **Absent** | `SolveHandIK` and every read of the component are in the Vulkan half of `RenderSystem.cpp` only, so a character set up to plant its hands on a weapon or a ledge simply does not in a browser -- the animation plays unmodified. Found 2026-09-18 by `tools/backend_parity.py`, which had drifted 27 methods because nothing ran it. |

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
   browser: `cd tools && npm install && node check_wgsl.mjs` compiles all 16
   shaders through Dawn, and `tools/web_capture.mjs` renders a build in headless
   Chrome.

# Competitive Strategy: MegaLights, Nanite, Chaos

**Date:** 2026-06-18

Honest assessment of where Enjin stands against the three UE5 headline systems, and
what to build to compete. Grounded in a code audit, not aspiration. The framing
matters: Enjin is a small open-source engine, so the goal is not to clone these
multi-year, large-team systems but to reach "good enough that the gap is not the
reason someone picks UE5," and to win where the architecture already has an edge.

---

## 1. Lighting vs MegaLights

MegaLights renders thousands of dynamic, shadow-casting lights cheaply on raster
hardware via stochastic per-pixel light selection (ReSTIR-style) plus denoising,
decoupling shadow cost from light count.

**What Enjin already has (real):**
- Clustered forward culling, wired and running in editor and player (the player path
  was just fixed by embedding the compute SPIR-V). 16x9x24 = 3456 clusters, cluster
  light buffer sized for 1024.
- ReSTIR is genuinely wired, not orphaned: `ReSTIR.cpp` builds the initial/temporal/
  spatial compute pipelines, allocates per-pixel reservoir buffers, and dispatches
  before RT shadows/GI. `rt_shadow.rgen` reads the reservoir and modulates the shadow.
- A large RT light store: the NEE light buffer holds up to ~32K lights, populated from
  the scene lights, under the full RT pipeline.
- Importance-scored shadow selection (intensity / distance squared, top-N).

**The real limits:**
- The raster forward path is capped at 64 point + 32 spot lights in a fixed UBO. The
  1024 cluster buffer is a culling index over that small set, not a true many-light
  store. So off the RT path the effective ceiling is ~96 lights.
- ReSTIR and the 32K light buffer only apply under the RT pipeline, which auto-activates
  only on RT-capable hardware. ReSTIR also defaults off (editor checkbox only) and is
  compiled out of the web build.
- Shadowed lights are hard-capped at 4 point + 4 spot. There is no stochastic, denoised
  shadow path for the many-light case off RT.

**The gap, honestly:** Enjin has the right primitives but siloed. The many-light store
and ReSTIR live only under RT; the raster path most users hit is classic 64/32 clustered
forward with 4+4 shadows. So today it is "ReSTIR-capable on RT hardware, classic
clustered everywhere else," not "thousands of shadowed lights cheaply on any GPU."

**Path to compete (ranked):**
1. Move point/spot light data into the SSBO-backed cluster buffer (already sized 1024)
   and have `triangle.frag` read light records from the SSBO via cluster indices instead
   of the fixed UBO arrays. Lifts the forward cap from ~96 to 1024+ with no new system.
2. Wire the existing ReSTIR reservoir into the raster/clustered pass, not just RT. Run
   the compute over the clustered light set and shade the reservoir-selected light per
   pixel. The reservoir math and buffers already exist; the missing piece is a non-RT
   consumer.
3. Add a stochastic shadow + denoise path for the selected light (reuse the RT denoisers
   that already exist), replacing the 4+4 shadow cap with "shadow whichever light the
   reservoir picked."
4. Build and consume the `LightBVH` (the header exists, the param is already plumbed into
   ReSTIR Dispatch) so importance sampling is proportional, which is what keeps the cost
   flat at thousands of lights.
5. Enable ReSTIR by default under a quality preset, gated on a light-count threshold.

This is the surface where Enjin is closest to parity. Items 1 and 2 are the high-value
moves and reuse code that already exists.

---

## 2. Geometry vs Nanite

Nanite is virtualized micropolygon geometry: meshlet hierarchies with per-cluster GPU
LOD and culling, a visibility buffer, software raster for sub-pixel triangles, and
streaming of geometry pages.

**What Enjin already has (real):**
- Solid discrete LOD: 5 levels, per-level distance thresholds, directional hysteresis,
  optional screen-space-size selection, now driven by one `ECS::SelectLOD` shared by both
  render paths. Production-quality traditional LOD (UE4-era class).
- A real edge-collapse mesh simplifier (`MeshSimplifier`) for auto-LOD. It works, but the
  cost metric is plain edge length, not Quadric Error Metrics. Fine for organic shapes,
  visibly distorts hard-surface meshes and swims UVs at higher reductions. Skips meshes
  over 100k verts.
- A fully written GPU frustum + two-phase Hi-Z occlusion culler with indirect draw and
  device-generated-commands support.

**The catch:** the GPU culler is gated OFF in both editor and player. The gate
(`!m_IsEditorMode && !m_PlayerMode`) means it runs in neither real configuration, because
built games did not ship the `cull.comp` SPIR-V, so indirect buffers came back empty and
entities were marked drawn-but-never-rendered. All the machinery is written but unreachable.
A meshlet builder exists too but has zero call sites; the visibility buffer and virtual
texturing are gated off as non-functional.

**The gap, honestly:** categorical, not incremental. Enjin has traditional discrete LOD
plus written-but-disabled per-object culling. It has isolated fragments of two Nanite
pillars (a dead meshlet builder, a gated visibility-buffer flag) and none of the
connective tissue. Competing head-on with Nanite is not realistic for this project and
probably not worth it. Nanite pays off for film-grade static-geometry density that most
games built in Enjin will not ship.

**Path to compete (ranked):**
1. Un-gate GPU culling. The single biggest real win available, and low effort. The compute
   path is fully written; the only blocker is the missing shipped shader. Embed `cull.comp`
   and `cull_hiz.comp` exactly the way the cluster compute shaders were just embedded for
   parity (`ClusterComputeShaderData.h`), then relax the player half of the gate. This turns
   dead code into a working frustum + Hi-Z occlusion culler. **This is the same fix pattern
   we just used for Gap C, so the path is proven.**
2. Upgrade `MeshSimplifier` to QEM with boundary/UV-seam preservation. Self-contained in one
   file, and it improves the LOD that already ships.
3. Nanite-lite (large but bounded): wire the meshlet builder into the asset pipeline, build a
   meshlet SSBO, enable cone culling, add a mesh-shader pipeline behind the existing capability
   check, and drive per-cluster LOD off projected meshlet bounds. Reuse the GPU-culling
   frustum/Hi-Z per meshlet. A multi-month feature, not a clone.
4. Full Nanite (visibility buffer + software raster + page streaming): not recommended. Out of
   proportion to the project.

The headline here: item 1 is cheap, proven, and turns a written subsystem back on. That is the
move.

---

## 3. Physics vs Chaos

Chaos bundles rigid body plus destruction/fracture, cloth, soft body, fields, ragdoll, and
vehicles.

**What Enjin already has (real):**
- Rigid body via Jolt v5.2.0, now test-covered (falls and rests, collision filtering,
  world-space colliders, character grounding). Jolt is AAA-grade (Horizon Forbidden West), so
  this tier matches or beats Chaos for rigid-body sim.
- Six joint types wired to Jolt constraints (distance, hinge with motor, ball-socket with cone
  limit, spring, fixed, slider with motor).
- Jolt `CharacterVirtual` controller, raycasts, sensors. All tested.
- Real Voronoi mesh fracture: true 3D plane-clipping into convex cells, four fracture patterns,
  debris with velocity/lifetime, chain destruction, pre-fracture released on impact. A working
  destruction layer, self-built.
- Gravity zones / force fields (box/sphere, directional and planetary point gravity).

**What is stubbed or missing:**
- Ragdoll is a scaffold. It generates bone joints and runs a blend timer but never creates
  per-bone Jolt bodies; the pose readback is a no-op. No physics-driven ragdoll.
- Vehicles are arcade kinematic (hand-integrated speed/steer, transform written directly). No
  Jolt `VehicleConstraint`, wheels, or suspension.
- Cloth and soft body are absent. Jolt v5 has a built-in soft-body solver; it is not wired.

**The gap, honestly:** the rigid-body and constraint core is on par with or ahead of Chaos
because it is Jolt and it is now tested. Destruction is a genuine strength Enjin already has.
The gaps are the simulation features Chaos bundles: physics ragdoll, wheeled-vehicle sim, and
cloth/soft body.

**Path to compete (ranked):** the good news is most of these are "expose existing Jolt," not
"build a solver."
1. Finish ragdoll. The hard parts (skeleton-to-joint generation, cone limits, blend) exist.
   Jolt ships a `Ragdoll`/`RagdollSettings` API and the constraint types are already integrated.
   Create per-bone capsule bodies + constraints in `ActivateRagdoll`, fill the no-op readback.
   Highest value-to-effort.
2. Vehicles: add a Jolt `VehicleConstraint` path (wheeled, suspension, tire friction) alongside
   the arcade controller. Well-trodden Jolt territory.
3. Cloth / soft body: wire Jolt v5's soft-body solver as new ECS components. Larger surface
   (rendering skinned cloth, pinning) but no novel physics.
4. Force fields: generalize gravity zones into a field family (radial, vortex, drag, wind). Cheap.

Caveat already on record: collider friction/bounciness bindings set ECS values but Jolt only
reads them at body creation, so any live-property UX needs body-recreate plumbing, which the
ragdoll/vehicle work would also need.

---

## Prioritized roadmap (cross-cutting)

Ordered by value-to-effort, mixing the three surfaces:

1. **Un-gate GPU culling** (geometry). Cheap, proven fix pattern, turns written code back on.
2. **SSBO light list + raster ReSTIR** (lighting, items 1-2). Breaks the 64/32 cap and brings
   MegaLights-class many-light shading to the raster path using code that exists.
3. **Finish Jolt ragdoll** (physics). Mostly exposing an existing Jolt API; high gameplay payoff.
4. **QEM mesh simplifier** (geometry). Self-contained quality win on shipping LOD.
5. **Stochastic shadows + denoise for selected lights** (lighting). The real MegaLights payoff.
6. **Jolt vehicles, then soft-body cloth** (physics). Bigger surfaces, still "expose Jolt."
7. **Nanite-lite meshlet pipeline** (geometry). The realistic ceiling; multi-month.

Where Enjin already wins or ties: rigid-body physics (Jolt), destruction (real Voronoi), and
ReSTIR-on-RT lighting. Where it is behind and should invest: the raster many-light path, GPU
culling activation, and physics simulation features (ragdoll/vehicle/cloth) that are one Jolt
integration away.

A stress-test scene (`stresstest` template) was added to exercise the clustered-lighting and
Jolt rigid-body paths together for profiling these changes.

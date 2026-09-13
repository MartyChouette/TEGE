# Hair

Status: proposed, nothing built.

Every claim below about what the engine currently has was checked against the
source and carries a file reference. Where something does not exist, this says
so plainly rather than describing it in the present tense.

## What the problem actually is

Hair reads as hair because of three things, in this order of importance:

1. **An anisotropic highlight.** Light scattering off a cylinder produces a
   specular band running perpendicular to the strand, not a round dot. Real hair
   shows two of them, offset along the strand and differently coloured: a sharp
   white one from the cuticle surface, and a softer tinted one from light that
   entered the fibre, bounced inside, and came back out. This is the single cue
   that separates hair from plastic ribbon, and it survives at any distance and
   any strand count.
2. **Depth through many thin overlapping things.** Hair is hundreds of
   translucent surfaces stacked along the view ray. Sorting them wrongly reads
   as a flicker; not sorting them at all reads as a flat decal.
3. **Motion with the right mass.** Hair lags the head, swings, settles, and does
   not stretch. It is a constraint problem, not an animation curve.

A hair system that solves 2 and 3 and skips 1 produces something that moves
correctly and looks wrong in every still frame. That ordering is the main
argument for the staging below.

## What the engine already has

**Strand simulation, working, and not currently used for hair.**
`RopeComponent` (`Engine/include/Enjin/ECS/Components/Rope.h`) is a Verlet point
chain with exactly the fields a strand needs: `segments`, `iterations`,
`damping`, `invMass` per point (0 pins it), `collide` against Box/Sphere/Capsule
colliders, `collisionSkin`, `friction`, constant `wind` plus live weather wind,
and `pinBottom` for attaching the tip. `ClothSystem::Update`
(`Engine/include/Enjin/Gameplay/ClothSystem.h`) drives it. This is a solved
problem in the codebase and a hair system should use it rather than write a
second integrator.

**Masked transparency and shaped shadows.** `MaterialComponent::alphaMode` is
`{ Opaque, Mask, Blend }` with an `alphaCutoff`
(`Engine/include/Enjin/ECS/Components/Material.h:37`). Masked materials already
cast correctly shaped shadows through the dedicated `shadow_mask.vert/frag`
pipeline, so an alpha-cut hair card will not cast a rectangle.

**Order-independent transparency.** `OITManager`
(`Engine/src/Renderer/OITManager.cpp`) exists and is wired.

**Skinning.** `SkeletonComponent`
(`Engine/include/Enjin/ECS/Components/Skeleton.h`), so cards can ride a head
bone.

## What is missing

**There is no anisotropic specular anywhere in the engine.** Confirmed by
searching every shader in `Engine/shaders/` for anisotropy, Kajiya-Kay and
Marschner: no hits. The BRDF is isotropic GGX. This is the largest gap and the
one that decides whether the result looks like hair.

**There is no way to render many strands cheaply.** `RopeComponent` is one
entity per rope, and `RenderSystem` iterates them individually
(`Engine/src/ECS/Systems/RenderSystem.cpp:7233`). That is correct for a dozen
ropes and wrong for two thousand strands. Hair needs one entity owning many
strands in one mesh.

**There is no grooming tool.** Per the golden rule in CLAUDE.md, a component
with a system and no authoring surface is not shipped.

## A constraint worth stating before anyone starts

`MaterialGPU` is **full**. It is 144 bytes, the `flags` word is out of bits
("bit 3 is the last free flag bit" in Material.h), and bits 24-28 of
`vertexSnapResolution` are taken. The house encoding for a new material mode is
a numeric band on `surfaceParam1`: 100 dither gradient, 200 dithered
transparency, 300 elemental, 400 surface noise, 500 palette-indexed, 600
radiosity normal mapping. **Hair gets band 700.**

Anything hair needs beyond that band (a shift value, a second highlight colour)
either packs into the existing `surfaceParam` floats or costs a new std430 row,
and a new row must be changed in lockstep across three places: the C++
`MaterialGPU` struct, the `MaterialEntry` struct in `triangle.frag`, and the
`static_assert` in TestMaterial. There are also **three** push-constant builders
that need the new mode, not one: `RenderToTarget` (editor viewport),
`RenderSplitscreen`, and `RenderEntity`, and that last one is what a built game
takes. Missing it ships a feature that works only in the editor.

## Staging

### Stage 1: the highlight

A Kajiya-Kay anisotropic term on `surfaceParam1` band 700, taking the strand
tangent from the mesh tangent attribute.

Two lobes: a primary shifted toward the root, white, tight; a secondary shifted
toward the tip, tinted by the base colour, broad. The shift and the two
exponents are the controls a person actually wants.

This stage is deliberately first and deliberately independent of everything
else. It applies to an ordinary alpha-masked card mesh imported from any DCC
tool, which means it is useful on its own before a single line of strand code
exists, and it is the stage that decides whether the rest is worth doing.

**Acceptance:** a flat card with a hair texture, lit by one moving light, shows
a highlight band that runs across the strands and travels along them as the
light moves. Turning the feature off makes it a round dot.

**Web parity:** the term is arithmetic in the fragment shader with no new
texture, no new bind, and no new pass, so WGSL gets the same thing rather than a
substitute. Run `tools/check_wgsl.mjs` after touching `WebShaderData.h`, because
WGSL is compiled by the browser and a green web build proves nothing about it.

### Stage 2: cards on a skeleton

`HairCardComponent`: a mesh, a material in band 700, a bone to follow, and an
alpha mode fixed to `Mask`.

Nothing new in the renderer. This is the shipping path for stylised characters
and it should stay usable on its own forever, because it is the only version
that runs well on the web tier and on low-end hardware.

**Acceptance:** a head model with imported hair cards renders and animates with
the skeleton, casts a shaped shadow, and costs one draw.

### Stage 3: strands

`HairStrandGroupComponent`: N Verlet chains owned by ONE entity, built into one
mesh, simulated by `ClothSystem` using the existing chain solver.

The component holds what a groom needs and nothing else: root positions on the
scalp mesh, strand count, segments per strand, length with variation, stiffness,
damping, and a collision capsule list (head and shoulders).

The work here is not the simulation, which exists. It is the batching: one
vertex buffer for the whole group, rebuilt when the points move, with the same
`meshDirty` / `topologyDirty` split `RopeComponent` already uses.

**Acceptance:** 2,000 strands on a moving head at 60 fps with no stretch
visible, and hair that settles rather than stopping dead. The strand count is
part of the acceptance criterion because a system that only works at 200 is a
different system.

**Web parity:** strands are a desktop tier feature. The web substitute is Stage
2 cards, chosen per the standing rule that parity means the right web-centric
alternative per capability rather than porting the desktop path.

### Stage 4: grooming

An Entity menu entry, an inspector, and a viewport brush that paints strand
density and direction onto the scalp.

Without this the feature is reachable only by people who know the component
exists, which fails the golden rule regardless of how good stages 1 to 3 are.

## Explicit non-goals

- **Marschner.** The full R / TT / TRT model with a measured longitudinal and
  azimuthal split is correct and expensive, and Kajiya-Kay with two shifted
  lobes gets most of the read. Revisit only if stage 1 ships and still looks
  wrong.
- **Physically accurate strand collision between strands.** Hair-hair collision
  is the expensive part of every real hair solver. Strand-to-body collision plus
  damping gets the motion; strand-to-strand does not earn its cost here.
- **A hair-specific transparency pass.** `OITManager` exists. If masked cards
  are not enough, use it.

## Order of doing

Stage 1 alone, shipped and looked at, before anything else is scoped. It is
cheap, it is independent, it has a clear acceptance test, and it answers the
only question that matters early: whether hair in this engine can be made to
look like hair. If stage 1 disappoints, stages 2 to 4 are all wasted on top of
it.

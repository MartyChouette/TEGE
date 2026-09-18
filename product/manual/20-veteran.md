# Player One: has shipped games before

**Status: framed, not written.** The anchors below are verified against the
engine. The prose is not drafted yet.

They do not need to be taught what an entity is. The question their journey has
to answer is narrower and harder: **how many times does this engine make me
leave it?**

Every round trip to another program is a context switch, a re-export, a
re-import, and a chance for the thing you fixed to come back wrong. An engine
that keeps you inside is not a convenience feature, it is the difference between
a change costing two minutes and costing twenty.

## What the journey has to prove

### The scripting API is genuinely at hand

Verified anchors:

- `TegeBehavior` is the base class, auto-injected into every module, with
  transform shortcuts on it so the first script is short.
- `[Property]` on a field puts it in the inspector, which is the bridge between
  scripting and the components-only path.
- Lifecycle is `OnStart`, `OnUpdate(dt)`, `OnFixedUpdate(fixedDt)`,
  `OnLateUpdate`. On fixed-timestep projects `OnFixedUpdate` lands exactly in
  step with physics.
- **A script does not tick on the frame it starts.** `OnUpdate` begins the frame
  after `OnStart`, deliberately, because the first frame's delta covers time
  before the script existed and anything integrating by `dt` would teleport.
  Worth a box in the finished chapter, because it is the kind of thing a veteran
  will otherwise diagnose as a bug in their own code.

### The debugger is worth opening

Anchors to cover: the console on backtick, Game Debug on F1, Debug Workstation on
F2, the command palette on Ctrl+P, `PlayModeDiff` showing what play mode changed,
and the Debug Recorder.

The one worth leading with: **double-clicking a console error opens a script
window at the offending line.** An error you can click is a different class of
error from one you have to go and find.

### A broken texture or animation is fixed here

This is the strongest part of the story and it is under-sold everywhere else.

`PixelEditor` is a real raster editor inside the editor: layers with visibility
and opacity, named palettes, retro resolution presets, and Pencil, Eraser, Fill,
Line, Rect, Ellipse, Eyedropper, Select and PolygonEdit.

Around it, confirmed present: `AnimationGraphEditor`, `VectorDrawingEditor`,
`ShaderGraph`, `SpriteSheetImporter`, `SpriteColliderGenerator`, `TerrainBrush`,
and a Simplify button with a keep-percentage slider on the Mesh component.

The chapter should be built as a timed comparison against the round trip, because
that is the argument: open the texture, fix the seam, see it on the model, keep
working, versus export, edit elsewhere, save, re-import, re-assign, look again.

### The things that will bite them specifically

A veteran's failure mode is assuming this engine works like the last one. The
chapter should carry the traps that punish transferred knowledge:

- **Collider sizes are world space.** Neither Jolt nor Box2D multiplies by
  transform scale. A box collider of 50 is 50 units no matter what the entity
  scale says.
- **Capsule height excludes the hemispheres.** Total height is
  `height + 2 * radius`.
- **Structural ECS mutation is main-thread only.** Reads are lock-free precisely
  because add, remove, create and destroy only ever run on the owner thread.
  Worker threads may read and nothing else.
- **`DestroyEntity` is deferred** to the start of the next `World::Update`, and
  entity IDs are generational, so a recycled slot never equals the old handle.
- **2D and 3D physics never mix.** Box2D for 2D scenes, Jolt for 3D, and the
  controllers have separate ground and wall checks per side.

## The shape to write it in

Not a tour. A single real task, timed, with the round trip as the control: import
a model whose texture has a visible seam and whose walk cycle has a foot slide,
fix both without leaving the engine, and count the context switches.

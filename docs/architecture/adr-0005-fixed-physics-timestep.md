# ADR-0005: Fixed Physics Timestep with Render Interpolation

## Status

Accepted; core implemented 2026-08-28 (SimulationClock + settings + all three
runtimes + interpolation + unit tests). Deliberate v1 deviations from the text
below, each revisitable:

- Character controllers stay frame-paced (they mostly write velocities that
  the fixed steps then integrate); moving them to the tick requires input-edge
  care and is deferred.
- `OnFixedUpdate` script hook not yet added.
- 2D bodies step fixed but render at tick pose (no interpolation) — fine at
  60Hz, revisit if 144Hz 2D stutter is reported.
- New projects do NOT yet default the setting on; it is opt-in for everyone
  via Settings > Project > Fixed Physics Timestep.
- Replay v2 SHIPPED (522a630) - but as CONFIG-CARRYING, not tick-indexing:
  the recorded dt stream already removes all timing variance from playback, so
  indexing inputs by tick bought nothing. What was actually missing was the
  replay forcing the sim-clock config it was recorded with. With that, a
  fixed-timestep replay is deterministic same-build across machines up to
  floating-point differences; float hardening is the remaining frontier.

## Date

2026-08-28

## Context

Every runtime (editor PlayMode, desktop Player, web player) steps physics once
per rendered frame with the frame's variable delta time:

- `JoltBackend::Update(deltaTime)` clamps to 50ms and calls
  `m_PhysicsSystem->Update(deltaTime, 1, ...)` — one step of whatever length
  the frame happened to be (JoltBackend.cpp:292-332).
- `Box2DBackend::Update(deltaTime)` calls `b2World_Step(m_WorldId, deltaTime,
  subStepCount)` (Box2DBackend.cpp:137). Box2D v3's documentation states the
  engine is tuned for a fixed step and that variable steps degrade solver
  stability.
- Scripts (`OnUpdate`), tweens, animation, and controllers also receive the
  raw frame dt. `Time_GetFixedDeltaTime()` exists as a script binding but
  returns a constant 1/60 that nothing steps at.

Consequences of the current model, in order of how much they hurt:

1. **No determinism.** The same inputs on machines with different frame rates
   produce different physics. This blocks the cross-machine replay goal (the
   .tegereplay format records an input+dt stream; playback on other hardware
   diverges). Seed-sync sharing cannot work without a fixed tick.
2. **Frame-rate-dependent gameplay.** Jump heights, contact resolution, and
   character controller feel differ at 30 vs 144 fps. A game tuned in the
   editor at 240 fps plays differently in an exported build on a laptop.
3. **Solver stability.** Long frames (loading hitch, background tab) deliver
   one big step; both Jolt and Box2D handle small fixed steps far better than
   occasional large ones. The 50ms/100ms clamps bound the damage but do not
   remove it.
4. **Time scale interaction.** `Time_SetScale` (2026-08-28) multiplies the
   frame dt; with variable stepping, slow motion changes solver behavior, not
   just speed.

The professional-engine model (Unity fixedDeltaTime, Unreal substepping, Godot
physics_ticks_per_second) is: simulation steps at a fixed tick driven by an
accumulator; rendering runs at its own rate and interpolates.

## Decision

Introduce an engine-level fixed-timestep accumulator that owns physics
stepping, with transform interpolation for rendering. The three runtimes share
one implementation.

### The accumulator

A small `Gameplay::SimulationClock` (new, Core or Gameplay module):

```
accumulator += min(frameDt, kMaxFrameDt) * timeScale;
steps = 0;
while (accumulator >= fixedDt && steps < kMaxStepsPerFrame) {
    StepPhysics(fixedDt);          // Jolt + Box2D + CharacterVirtual updates
    accumulator -= fixedDt;
    ++steps;
}
if (steps == kMaxStepsPerFrame) accumulator = 0;   // spiral-of-death clamp: drop time
alpha = accumulator / fixedDt;                      // 0..1 interpolation factor
```

- `fixedDt` defaults to 1/60, project-configurable (`GameFrameSettings` gains
  `physicsTicksPerSecond`, serialized in the .enjinproject and carried through
  the build manifest like targetFrameRate).
- `kMaxStepsPerFrame` = 4. When exceeded, remaining accumulated time is
  DROPPED (game slows down rather than death-spiraling).
- `Time_SetScale` multiplies the time added to the accumulator. The tick size
  never changes, so slow motion is more solver steps of identical quality, not
  smaller steps. `Time_GetFixedDeltaTime()` starts returning the real tick.

### What steps at the fixed tick vs the frame

Fixed tick: Jolt world, Box2D world, character controllers (`CheckGround`,
`CharacterVirtual`), buoyancy forces, hazard overlap checks. Everything that
integrates forces or resolves contacts.

Frame rate (unchanged): scripts (`OnUpdate(frameDt)` keeps its meaning),
tweens, animation, particles, cameras, UI, water surface meshes, audio. A
`TegeBehavior::OnFixedUpdate(float)` hook is added for gameplay that needs
tick-locked logic (custom forces), called once per physics step.

### Interpolation

Physics-driven entities render at a pose interpolated between the previous and
current tick states, using `alpha`. Implementation: the physics backends
already write transforms back to `TransformComponent` after stepping; the
accumulator keeps a copy of the pre-step transform per dynamic body and the
render path lerps position / slerps rotation. The interpolated value is
WRITTEN to `TransformComponent` after the step loop each frame (so every
consumer — renderer, scripts reading positions, audio — sees the smooth pose),
and the raw tick pose is kept internally for the next step. This matches how
the entity pipeline already flows and avoids a renderer-side special path.

Interpolation OFF for: kinematic bodies moved by scripts (they are already
frame-paced), and anything with `RigidbodyComponent.bodyType == Static`.

### Rollout

Behind a project setting `fixedTimestep` (default ON for NEW projects, OFF for
existing projects on first load, with a one-time editor prompt). The variable
path remains for one release as the fallback; removal happens once the golden
scenes, the platformer/topdown templates, and the replay tests pass on the
fixed path.

## Alternatives considered

- **Jolt-only substepping (`Update(dt, nSteps, ...)`)**: splits a frame's dt
  into n equal sub-steps. Improves stability but the step size still varies
  frame to frame, so determinism is not achieved and Box2D is not covered.
- **Fixed tick without interpolation**: visible 60Hz stutter on 144Hz
  monitors; every engine that tried this added interpolation later.
- **Extrapolation instead of interpolation**: no added latency, but
  mispredicts on impacts (visual pops through walls). Interpolation's one-tick
  visual latency (~16ms) is the standard trade.
- **Run scripts on the fixed tick too**: maximum determinism, but breaks the
  existing `OnUpdate(dt)` contract for every shipped script and couples input
  sampling to the tick. The `OnFixedUpdate` opt-in gets determinism where it
  matters without the migration cost.

## Consequences

- Replays become machine-independent for physics outcomes once input playback
  is also tick-indexed (follow-up: record inputs against tick numbers instead
  of the dt stream — the .tegereplay format gains a v2 mode).
- Physics behavior changes subtly on the fixed path (that is the point).
  Content tuned against variable stepping may need re-tuning; hence the
  per-project opt-in and the one-release fallback.
- One tick of visual latency on physics-driven objects from interpolation.
- `kMaxStepsPerFrame` capping means heavy scenes on slow machines run in slow
  motion instead of spiraling — same trade Unity and Godot make.
- Memory: previous-pose copy per dynamic body (two Vector3 + Quaternion, tens
  of bytes each; negligible).

## Test plan

- Unit: accumulator math (carry-over, clamp, alpha range, timeScale
  interaction) as a pure test, no physics needed.
- Determinism: step a seeded scene 600 ticks twice with different simulated
  frame-dt sequences (16ms fixed vs jittered 5-50ms); assert identical final
  body transforms on the fixed path, and document the divergence on the
  variable path as the baseline.
- Regression: golden captures of Buoyancy, WaterShowcase, and the platformer
  template on both paths; the variable-path captures must be byte-identical to
  pre-ADR.
- Feel: Marty eyeball on the character controller templates at 30 / 60 / 144
  fps caps (the editor limiter makes this a dropdown exercise).

## References

- docs/FPS_ARCHITECTURE.md (2026-08-28 audit that motivated this)
- JoltBackend.cpp:292-332, Box2DBackend.cpp:122-153 (current stepping)
- Glenn Fiedler, "Fix Your Timestep!" (the canonical accumulator+interpolation
  writeup)
- Box2D v3 docs on fixed stepping; Unity FixedUpdate; Godot
  physics_ticks_per_second

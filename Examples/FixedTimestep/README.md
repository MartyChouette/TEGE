# Fixed Timestep

A crate pyramid, a heavy ball dropping on it, and a time-scale remote control.
Open `FixedTimestep.enjinproject` and press Play.

## What to try

- **1 / 2 / 3** switch the global time scale: slow-mo (0.25x), hitstop (0.05x),
  normal. Physics slows smoothly because slow motion runs MORE fixed ticks per
  second of game time, never smaller ones - solver quality is identical.
- **E** kicks the crates. **R** restarts the scene (and resets the time scale).
- Change the editor FPS cap (Settings > Editor > Frame Rate Limit) between 30 /
  60 / 144 and kick the pile again: with the fixed timestep the crates behave
  the same at every frame rate. Un-check Settings > Project > Fixed Physics
  Timestep and repeat to feel the classic variable-step difference.

## How it works

The project sets `fixedTimestep: true` (60 ticks/second) in its frame settings.
Physics steps at that constant tick through an accumulator; dynamic bodies
render interpolated between the last two ticks, so motion stays smooth on
high-refresh monitors. The script's `OnFixedUpdate` runs exactly once per
physics tick; `OnUpdate` stays frame-paced. See
`docs/architecture/adr-0005-fixed-physics-timestep.md`.

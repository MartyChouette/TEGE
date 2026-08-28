# Frame Pacing / FPS Architecture

Audited 2026-08-28. This is the map of who controls frame rate where, and what
the master clock is.

## The three clocks

Professional engines (Unity, Unreal, Godot) separate three clocks with a strict
hierarchy: a fixed simulation tick for physics and gameplay determinism, a
variable render rate mastered by vsync, and an author-set target fps that acts
as a cap in the shipped game. The editor is deliberately decoupled and throttles
itself when unfocused or idle.

## What Enjin has today

### Delta time (all runtimes)
One source: `Application::RunOneFrame()` (Core/src/Core/Application.cpp:203).
Wall-clock elapsed via `high_resolution_clock`, clamped to 0.1s max so a pause
or minimize cannot explode physics. No smoothing. The web player gets its pacing
from the browser's requestAnimationFrame; same clamp applies.

### Target fps cap (present, working)
`Application::LimitFrameRate` (Application.cpp:252) is a hybrid sleep + 2ms
spin-lock limiter with 1ms Windows timer granularity. Who feeds it:

- Editor: `EditorSettings.editorFrameRateLimit` (Uncapped/30/60/120/144/240),
  plus `reduceFrameRateWhenUnfocused` (default ON, 15 fps) and an opt-in idle
  throttle (`reduceFrameRateWhenIdle`, default 30s timeout, 30 fps).
  Editor/src/main.cpp:92-139.
- Exported game: `GameFrameSettings` in the project manifest (targetFrameRate,
  vSync, backgroundBehavior Pause/ReduceTo30/RunNormally).
  Player/src/main.cpp:207-223.
- Web: no cap, browser rAF owns pacing. Correct for the platform.

### Vsync (present)
VulkanSwapchain.cpp:178-225. VSync on forces FIFO. VSync off prefers MAILBOX,
falls back to IMMEDIATE. The player can change it at runtime through a deferred
swapchain recreate (`RequestVSyncChange`).

### Game view vs scene view (editor)
Both render every editor frame, but the game view has its own fps dropdown and
skips frames on an accumulated-time check. During play the editor loop uncaps.

## The verdict: who is master

- Presentation master: vsync (FIFO) when enabled, else the target-fps limiter,
  else uncapped.
- Simulation master: none. This is the real gap. Physics, scripts, animation,
  and tweens all step once per frame with the frame's variable delta time.
  JoltBackend clamps to 50ms per step (JoltBackend.cpp:300) but there is no
  accumulator, no fixed tick, no interpolation.

## Gaps vs the professional model

1. No fixed physics timestep. Variable dt feeds Jolt and Box2D directly.
   Consequences: physics behavior varies with frame rate, long frames produce
   different contact results than short ones, and determinism (the replay and
   seed-sync work) cannot be guaranteed across machines with different frame
   rates. Box2D v3 explicitly recommends a fixed step.
   Fix shape: accumulator loop stepping physics at a fixed tick (default 60Hz,
   project-configurable), clamp accumulated steps (max 3-4 per frame) to avoid
   the spiral of death, interpolate transforms for rendering. This is the one
   structural change worth an ADR. It also unblocks cross-machine replay
   determinism.
2. No time scale. Pause is a state flag (gameplay systems get Update(0)), which
   works, but slow-mo/hitstop effects need a Time_SetScale binding that scales
   dt before consumers see it. Cheap to add once dt flows through one place.
3. Vsync/cap interplay quirk: the player computes
   `useVSync = (m_TargetFPS != 0) && m_VSync` (main.cpp:202), so an uncapped
   game ignores the vsync flag. Probably unintended. Vsync should be honored
   independently of the cap.
4. Scene-view fps control: not needed as a separate knob. The existing
   editor-wide cap + unfocused/idle throttles cover it, matching how Unity and
   Unreal handle it (throttle, not a per-viewport rate). The game view already
   has its own dropdown.

## Recommended order

1. Fix the vsync/cap interplay (one line, verify with the player).
2. Fixed physics tick with interpolation (ADR + implementation, touches
   PlayMode, Player, web, and both physics backends).
3. Time_SetScale / Time_GetScale script bindings riding the same dt pipe.
4. Consider turning on the editor idle throttle by default.

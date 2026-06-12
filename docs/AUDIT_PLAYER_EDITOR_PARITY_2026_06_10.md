# Player / Editor Parity Audit — 2026-06-10

**Method.** Compared per-frame system updates and member sets across the three
runtime paths: `EditorLayer` (editor viewport), `PlayMode` (play-in-editor), and
`Player/src/main.cpp` (standalone build). Cross-checked renderer gating on
`m_PlayerMode`.

**Result.** Three real gaps. Everything else (Physics, StreamingManager,
SimpleAudio, AudioGraphRuntime, ObjectPool, AlternativeInput, AudioIndicators,
Announcer) is present in both paths.

---

## Gap A — `ElementalSystem` absent from the Player

**Evidence.** 0 references to `ElementalSystem` in `Player/src/main.cpp`. The
instance lives in `EditorLayer` (`EditorLayer.h:1012`) and updates at
`EditorLayer.cpp:2110`. `PlayMode` only holds a borrowed pointer set from the
editor's instance, so play-in-editor gets elemental via `EditorLayer`. The
standalone Player has nothing.

**Impact.** In a shipped game: no fire/water/earth/air simulation, no elemental
particle rendering (`RenderElementalParticles` never called), and the fire
lights never emit. The `AddTransientPointLight` API is compiled into the engine
and available in the Player, but nothing feeds it.

**Fix path.** Add an `ElementalSystem` member to the Player app, `Initialize` it
against the Player's wind/weather, call `Update` in the game loop, add the fire-
light glue (`BuildFireLights` -> `ClearTransientPointLights`/`AddTransientPointLight`),
and call `RenderElementalParticles`. Mirror of `EditorLayer.cpp:2100-2123` plus
the fire-light glue.

## Gap B — `AudioReactiveSystem` absent from the Player

**Evidence.** 0 references in `Player/src/main.cpp`. Updated in PlayMode at
`PlayMode.cpp:595`.

**Impact.** Audio-reactive effects are dead in builds. Unrelated to the effects
work; smaller surface.

**Fix path.** Add member + one `Update` call in the Player loop.

## Gap C — Compute-shader systems disabled in builds

**Evidence.** `Player/src/main.cpp:219` sets `SetPlayerMode(true)` with the
comment "Skip GPU compute shaders not embedded in builds." That flag gates:
- `RenderSystem.cpp:3831` — GPU culling
- `RenderSystem.cpp:3912` — clustered lighting `AssignLights`

Root cause: `ClusteredLighting.cpp:299-339` loads its compute SPIR-V from loose
`.spv` files on disk, which are not packaged into a shipped `.enjpak`. Graphics
shaders are embedded as headers (`ShaderData.h` / `.hex`); the compute ones are
not.

**Impact.** Shipped games quietly lose clustered lighting and GPU culling.
Volumetric fog still runs (`:3993` not gated) but its in-scatter loop reads the
clustered light grid, never populated in player mode, so fog only scatters the
sun. The fire-glow-through-smoke payoff will not appear in builds until C is
fixed.

**Fix path.** Embed the compute `.spv` as headers (same pipeline as graphics
shaders, `_gen_all.py`) or pack into the `.enjpak` and load via the asset reader,
then drop the `m_PlayerMode` gates at `:3831` and `:3912`. Verify `VolumetricFog`
and `GPUParticleSystem` compute shaders get the same treatment. Build-pipeline
work, not a loop edit.

---

## Status (2026-06-10)

All three scheduled for fixing. The fire-light feature is correct and
shipping-ready on the editor path; Gaps A and C are what stand between it and a
built game.

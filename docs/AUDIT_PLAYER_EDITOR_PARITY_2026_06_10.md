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

---

## Re-check (2026-06-18)

Re-audited the three runtime paths after a session of shared-engine changes
(LOD unification, MaterialGPU size fix) and an editor-only feature (drag-and-drop
import + its enable/disable setting). All three targets (`EnjinEditor`,
`EnjinPlayer`) build against the current engine.

**Gap A — ElementalSystem in the Player: FIXED.** `Player/src/main.cpp` now
initializes it (`:398`), updates it (`:930`), and feeds the fire lights into the
renderer (`BuildFireLights` -> `ClearTransientPointLights`/`AddTransientPointLight`,
`:932-935`).

**Gap B — AudioReactiveSystem in the Player: FIXED.** Set up at `:400-402`,
updated at `:924`.

**Gap C — compute-shader systems disabled in builds: STILL OPEN.**
`Player/src/main.cpp:221` still `SetPlayerMode(true)` with "Skip GPU compute
shaders not embedded in builds", so shipped games lose GPU culling and clustered
lighting (and fog only scatters the sun). Unchanged from 2026-06-10; needs the
build-pipeline fix (embed compute `.spv` as headers via `_gen_all.py`, or pack
into the `.enjpak` and load via the asset reader, then drop the `m_PlayerMode`
gates at `RenderSystem.cpp` ~3831/3912).

**This session's changes — parity-safe:**
- *LOD unification.* The two divergent LOD selectors in `RenderSystem.cpp` were
  replaced by one `ECS::SelectLOD`. The Player runs the same `RenderSystem::Update`
  (LOD site is reached before the player-mode GPU-culling gate), so editor and
  player now select LOD identically. No new divergence.
- *MaterialGPU 80 -> 112 size fix.* Engine-wide struct; both apps build against it.
- *Drag-and-drop import + setting.* Editor-only by design (the Player has no file-
  drop import path), so not a parity surface.

**Installer (`installer/EnjinSetup.iss`) — complete for editor + player.** Packages
`EnjinEditor.exe` (fixed component), `EnjinPlayer.exe` (full type), `Engine/shaders/*.spv`,
scripts, and docs; associates `.enjinproject` and `.enjin` with the editor and
cleans up the stale `.enjscene` key. Note: the loose `.spv` it ships are for the
editor install dir; they do not resolve Gap C for shipped `.enjpak` games. Minor:
`AppVersion` is still `0.9.6`; `API_REFERENCE.md` is listed `skipifsourcedoesntexist`
(the repo ships `SCRIPTING_API.md` instead).

**Bottom line.** Editor/player runtime parity is now down to the single
pre-existing Gap C (compute shaders in shipped builds). Nothing from this session
introduced a new gap.

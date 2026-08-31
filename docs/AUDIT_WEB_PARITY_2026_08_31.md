# Web/Desktop Parity Audit — 2026-08-31

Trigger: live browser testing of the Playground web export surfaced "no rain, no
weather, no bullet time" after a round of web fixes. Five parallel code audits
(script bindings, system ticks, render features, component behavior, live
feature traces) plus a headless-Chrome console capture. Every load-bearing
claim below was verified against source; audit-agent claims that failed
verification were dropped.

## The root cause, and the rule that comes from it

The web player's browser console (captured headlessly: Chrome
`--headless=new --enable-unsafe-webgpu --use-webgpu-adapter=swiftshader
--enable-logging=stderr`) showed:

```
[ERROR] scripts/Playground.as (24, 131): No matching symbol 'Render_SetRainActive'
```

One missing symbol fails the WHOLE AngelScript module, so every script-driven
feature died together — weather, bullet time, subtitles, colorblind toggle.
None of their engine paths were broken; the script calling them was never
alive. `WebStubs.cpp` had stubbed `RegisterRenderBindings` and
`RegisterAudioGraphBindings` to empty on web, so no `Render_*` /
`AudioEventGraph_*` symbol existed there.

**RULE: every script-visible symbol must be REGISTERED on every platform.**
Wrappers no-op (null-guarded host pointer) where a backend lacks the feature;
they never disappear. Stubbing a `Register*` function is never the right
web-exclusion mechanism.

Fixed in 3ffed3c0: `ScriptBindings_Render.cpp` and `ScriptBindings_AudioGraph.cpp`
now compile on web (CMake un-exclusion; PostProcessing compile-only stand-in;
web RenderSystem accessor stubs). Verified live: `Module 'scripts_Playground'
compiled successfully` in the web console.

## Fixed during this audit (commits 5f65566e, 3ffed3c0, + Wire2D follow-up)

| Gap | Fix |
|---|---|
| Script bindings absent on web (root cause above) | Real binding TUs compile on web; SetBindingsRenderSystem + SetBindingsDialogueSystem wired in web_main |
| ClothSystem never ticked on web | Ticked after controllers; web render path learned the cloth/rope dirty protocol (topologyDirty=rebuild buffers, meshDirty=vertex re-upload) |
| RenderWeatherParticles empty stub on web | WeatherSystem CPU particles drawn via sprite pipeline after the sky (rain streaks / snow flakes) |
| No material uvScroll on web (frozen waterfall) | uvScrollU/V in web ObjectData (packed into former padding), subtract×wind-clock in pbr vertex |
| Pause menu blanked the scene | Camera sync extracted to SyncCameraToWorld(), called in the paused early-return too |
| Web sky horizon band | SKY_WGSL now uses the exact desktop cubemap-bake ramp (10% horizon band, linear blends) |
| Web trees wrong (slivers, no rotation/wind) | TREE_WGSL ported to authored canopy size + per-instance rotation + wind (13664797) |
| Wire2DCollisionCallbacks never called on web | Wired in InitWebSceneRuntime with a long-lived m_DeferredDestroys member (2D sensors/triggers now reach visual scripts) |

## Script binding host wiring — remaining unwired setters on web

These functions are now REGISTERED on web (scripts compile) but silently no-op
because the host system doesn't exist there or isn't wired. Acceptable
degradation; wire them if/when the system lands on web:

`SetBindingsRenderView` (Physics_RaycastScreen→0), `SetBindingsDestructible`,
`SetBindingsStreaming`, `SetBindingsFlowAdvanceFlag` (startup-flow runner is
desktop-only), `SetBindingsPostProcessing` (no player-mode PP on web),
`SetBindingsDyslexiaFontCallback`, `SetBindingsAudioGraphRuntime`,
`SetBindingsMIDI`, `SetFlashShimSaveSystem`.

## Systems the desktop player ticks that web does not

From the update-loop side-by-side (main.cpp vs web_main.cpp). Missing entirely
on web: **SurfaceResponseSystem** (no footstep particles/sounds from surfaces),
**FlowerSystem**, **QuestFlow per-entity advance**, **SeasonalWeather**,
**StreamingManager** (no level streaming), **Water3D animated surfaces**,
**FluidSimulation + FluidTerrainCoupling**, **CurlNoiseSystem**,
**AudioReactiveSystem**, **AudioGraphRuntime init**, **PostProcessing (player
mode)**, **TieredSaveSystem autosave timer**, **NetworkSystem per-frame tick**,
**ResourceComponent regen loop**.

Ordering note: web ticks VisualScriptSystem BEFORE StateMachineSystem; desktop
does the reverse. FSMs on web see last frame's visual-script state. Low
priority, but flip it when touching that code.

## Render feature parity (Vulkan vs WebGPU) — summary

Web works: PBR meshes (baseColor/normal/MR textures), skinned meshes, LOD,
sprites, vegetation (grass/shrub/tree via hooks), 8 dir/point lights + 4 spots
with shadows (single cascade), fog, sky gradient + weather blend, bloom, FXAA,
ACES, colorblind modes, water surface waves (Gerstner flag), GPU particles
(via player hook), cloth/rope, weather particles, uvScroll.

Web missing (biggest first): tilemaps, 3D text, CPU ParticleRenderer visuals,
decals/trails/lines, terrain auto-mesh, reflection systems (probes, matcap,
scrolling-reflection, flipped-floor), morph targets, custom shaders, SSS /
transmission / parallax mapping / cel / dither / retro material features,
TAA/SSAO/SSR/motion blur, world curvature, cascade shadow blending.
Desktop triangle.frag ≈ 2000 lines vs web pbr.wgsl ≈ 450 — roughly 75% of
material features don't exist on web. That is a program of work, not a bug.

## Component behavior parity — verdict

Gameplay/behavior components are in GOOD shape on web: all 7 controller types,
full Jolt/Box2D physics + joints, health/damage/hazards/pickups, doors/ladders
/platforms/switches, audio (spatial, reverb, RTPC, beat sync, occlusion),
AI (BT/FSM/AISystem — navmesh falls back to A*), dialogue+quests, save (IDBFS),
cinematics/vcams/parallax, tweens, visual scripts, UI canvases. Confirmed gaps:
MIDI (no Web MIDI wiring), gamepad rumble (no vibrationActuator call), navmesh
baking (A* fallback), and the now-fixed 2D collision callbacks.

## How to verify web features from now on

Headless console capture (no human in the loop):
```
chrome --headless=new --disable-gpu-sandbox --enable-unsafe-webgpu \
  --use-webgpu-adapter=swiftshader --enable-logging=stderr --v=0 \
  --virtual-time-budget=30000 --user-data-dir=/tmp/x http://localhost:9090
```
stderr carries every console.log/ENJIN_LOG line, script compile errors, and
WGSL validation errors. This is how the root cause here was proven in one run.

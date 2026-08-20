# Feature Connection Audit — 2026-08-19

Systematic sweep for disconnected feature plumbing, triggered by the evening's finds
(dead vegetation texture fields, weather invisible in the game view, Player never
reading weather zones, texture alpha ignored by Mask/Blend, Cloth not removable).
Four audit passes: dead component fields, editor-vs-player parity, component
lifecycle parity (add/draw/remove/serialize), serializer field completeness.

Legend: FIXED = fixed today (uncommitted). OPEN = needs work, ranked within section.

---

## 1. Silent data loss on scene load (highest priority — authored work disappears)

| # | Finding | Where | Status |
|---|---------|-------|--------|
| 1.1 | `Material.vertexSnapResolution` load clamp `if (v <= 31)` discards ALL valid values — field range is 80–320, default 160, so every authored PS1 grid resolution reverts on load. (Author confused the raw field with the 5-bit packed shader value `(res/8) & 0x1F`.) | SceneSerializer.cpp:498 | FIXED (clamp removed; full u8 range accepted) |
| 1.2 | `WeatherZone.weatherType` load clamp was `<= 3` — Snow(4)/Fog(5)/Storm(6) zones silently reverted to Clear on every load, forever. | SceneSerializer.cpp:732 | FIXED (+ regression tests) |
| 1.3 | `SwarmComponent` is addable in the editor but serialized NOWHERE — vanishes on save. No inspector draw, no remove menu either. | EditorLayerInspector.cpp:368 (add entry); zero hits in SceneSerializer.cpp | FIXED (serialized in all chains + inspector draw/remove added) |
| 1.4 | Controller view-state fields not serialized: FirstPerson `pitch/yaw`, Vehicle `cameraPitch/cameraYaw`, ThirdPerson `frameSide/frameHorizontalBias`, TopDown2D `facingAngle`, TopDown3D `targetPosition/hasTarget/arrivalThreshold`, Platformer2D `groundCheckDistance/facingDirection`. Some are runtime state (fine to skip), but `frameSide`, `arrivalThreshold`, `groundCheckDistance` are authored tunables. | SceneSerializer.cpp:1698–1960 | FIXED (authored tunables serialized: frameSide, frameHorizontalBias, arrivalThreshold, groundCheckDistance; runtime view state intentionally skipped) |
| 1.5 | `AudioSource` save drops `volumeMin/volumeMax/clipVariations/noRepeat/usePooling/poolSize/audioDescription`. | SceneSerializer.cpp:2037–2075 | FIXED (all 7 fields serialized) |
| 1.6 | `Water3D.settings.position` not serialized. | SceneSerializer.cpp:795–838 | NOT A SERIALIZER BUG — settings.position is never written by anything; ripples ignore the entity transform (Water3D system fix, earmarked) |
| 1.7 | Stale-clamp risk on enum loads (fine today, will silently eat values when enums grow): `Material.parallaxMode <= 3`, `Material.shadowDitherPattern <= 7`, `Material.shadowDitherMode <= 3`, `Cloth.seams <= 3`. Consider a shared pattern that clamps to the enum's declared max. | SceneSerializer.cpp:489–498, 1266 | OPEN (hygiene) |
| 1.8 | `GPUParticleEmitter` excluded from `SerializeOneComponent`'s else-if chain (MSVC C1061 nesting limit) — per-component undo/redo silently skips it. Deserialize/Remove chains DO have it (they use flat ifs). | SceneSerializer.cpp:10976 comment | FIXED (gpuParticleEmitter + swarm in the early-return section above the chain) |

## 2. Editor works, exported game doesn't (feature ships broken)

| # | Finding | Where | Status |
|---|---------|-------|--------|
| 2.1 | Player never read WeatherZones, never wired weather fog, never rendered precipitation — the entire weather feature was editor-preview-only in shipped games. | Player/src/main.cpp | FIXED (UpdateWeatherZones port, incl. temperature sleet/snow logic; script-driven weather preserved when no zone) |
| 2.2 | Game-view weather particles bound the EDITOR camera's matrices (RenderToTarget restores main descriptor sets before the weather draw) — precipitation never visible in game view or golden captures. | RenderSystem::RenderWeatherParticles + EditorLayer game-view calls | FIXED (useOffscreenSets path) |
| 2.3 | `RenderElementalParticles` in the game view likely has the SAME wrong-camera bug as 2.2 (same call pattern, same restored sets). Fire/elemental visuals in game view suspect. | EditorLayer.cpp:2694/2700 | FIXED (useOffscreenSets path, both game-view call sites) |
| 2.4 | `SetRainActive` (drives water ripple shader during rain) — editor sets it, Player never does. Rain in builds = no ripples. | EditorLayer.cpp:2528; no Player call | FIXED (Player UpdateWeatherZones) |
| 2.5 | `TreeRenderer::SetSeasonState` — editor passes season to trees, Player computes seasons but never forwards them. Seasonal tree color/foliage frozen in builds. | EditorLayer.cpp:2520; no Player call | FIXED (Player UpdateWeatherZones; heat sources ported too) |
| 2.6 | Temperature-driven water freeze/thaw — editor-only loop; water never freezes in builds. | EditorLayer.cpp:2432–2471; no Player equivalent | FIXED (Player UpdateWeatherZones) |
| 2.7 | Web player: no weather zone logic at all, no SetRainActive, no season state (web precip rides the GPU particle pool off intensities only). | web_main.cpp | OPEN (web parity pass) |

## 3. Rendering plumbing gaps

| # | Finding | Where | Status |
|---|---------|-------|--------|
| 3.1 | Base color texture ALPHA was never sampled by the mesh shader — Mask/Blend tested only opacity × vertex color; any PNG with transparency rendered opaque. Engine-wide since forever. | triangle.frag (both alpha blocks) | FIXED (glTF semantics; Opaque unaffected) |
| 3.2 | Web PBR shader (WGSL) almost certainly has the same texture-alpha gap as 3.1. | Engine/shaders/*.wgsl / embedded | ALREADY CORRECT — web pbr.wgsl multiplies baseColorSample.a and applies the cutoff; web was ahead of desktop |
| 3.3 | Shadow pass ignores Mask cutout — masked quads (foliage cards, the goblin hair) cast full-rectangle shadows. | shadow.vert pipeline (no frag/alpha test) | FIXED 08-20 (shadow_mask.vert/frag cutout pipeline variant; per-draw switch in RenderEntityShadow for Mask+textured materials; wired into CSM + point + spot shadow passes incl. the parallel secondary-CB path; A/B probe-verified: opaque=solid rectangle, masked=hair silhouette) |
| 3.4 | Vegetation custom-texture fields were dead (serialized + inspector-edited, never rendered; RT even skipped textured volumes). | grass/shrub/tree shaders + renderers | FIXED (bindless set-1 sampling, cutout, RT keeps geometry) |
| 3.5 | Weather rain/snow had no texture support. | WeatherRenderer + new weather_particle.frag | FIXED (zone sprite pickers; note particle.frag is SHARED by ParticleRenderer/FluidRenderer — weather got its own shader after VUID-07988) |

## 4. Dead component fields (edit does nothing)

| # | Field | Reality | Status |
|---|-------|---------|--------|
| 4.1 | `TreeVolume.minHeightScale` / `maxHeightScale` | tree.vert hardcodes `hash*0.8+0.6` (0.6–1.4 — the defaults). Sliders inert. | FIXED (min/max packed into tree flags bits 16-29; shader, RT bake, and collider gen all honor them; tree density now capped at 65535) |
| 4.2 | `TreeVolume.canopyQuads` | BuildTree hardcodes 3 canopy quads. | FIXED (partial index draw: trunk 12 + canopyQuads*24; inspector DragInt added) |
| 4.3 | `ShrubVolume.quadsPerShrub` | BuildShrub hardcodes 3 quads. | FIXED (partial index draw: quadsPerShrub*15; inspector DragInt added) |
| 4.4 | `Terrain2D.autoColliders` | Checkbox consumed by nothing. | FIXED (dead checkbox replaced with a Generate Surface Colliders button: one static thin box per segment, tree-collider pattern; field kept for scene compat) |
| 4.5 | `GPUParticleEmitter.burstCount/burstNow` not serialized | Actually correct (one-shot runtime triggers) — noted to prevent re-flagging. | NOT A BUG |

## 5. Component lifecycle parity (inspector)

| # | Finding | Status |
|---|---------|--------|
| 5.1 | Cloth: no Remove Component menu. | FIXED |
| 5.2 | GPU Particle Emitter: no Remove Component menu. | FIXED |
| 5.3 | Swarm: addable, no inspector UI, no remove, no serialization (see 1.3). | FIXED (see 1.3) |
| 5.4 | VisualScript: serialized fine; verify it has inspector draw + remove (agent flagged, unconfirmed). | FIXED (VisualScript had NO inspector section at all — added enabled toggle, graph stats, remove menu) |

## 6. Known/pre-existing, re-confirmed today

- TestSaveSystem reads the REAL `%APPDATA%\enjin\saves` — FIXED: the disk-dependent test now uses a temp-dir LocalSaveBackend (SetBackend already existed).
- OITManager still carries 2 fossil baked shader arrays (from CLAUDE.md traps) — migrate before touching OIT shaders.

---

## Fix pass 2026-08-19 (same evening)

Everything above marked FIXED was done in the same session. Still OPEN: 1.7 stale-clamp hygiene (correct today, no data loss), 2.7 web parity pass, Water3D ripple position (was 1.6), OIT fossil arrays. (3.3 shadow-pass mask cutout FIXED 08-20.)

## Original suggested fix order

1. **1.1 vertexSnapResolution clamp** — one line, active data loss on every load.
2. **2.4 SetRainActive + 2.5 SetSeasonState in Player** — few lines each inside the new UpdateWeatherZones; makes rain ripples + seasons real in builds.
3. **1.3/5.3 Swarm** — decide: finish or de-list.
4. **2.3 elemental particles camera** — same fix shape as 2.2.
5. **2.6 water freeze in Player**, then **1.4/1.5 serializer field triage**, then **1.8 C1061 chain**, then 3.2/3.3 rendering follow-ups, then web parity (2.7) as its own pass.

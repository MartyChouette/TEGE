# Master Validation List

Created 2026-08-29 from a four-track engine audit (parity, differentiators,
stability, stress gaps), with every actionable claim re-verified against source
before inclusion. Supersedes FEATURE_AUDIT_CHECKLIST.md (2026-06-15) as the
release-readiness gate. Six agent findings were rejected as false positives
during verification; they are listed at the bottom so they don't resurface.

## 1. Where the engine stands

One line: TEGE has no peer in its niche — batteries-included indie engine for
PC + web with professional audio, deep accessibility, retro-authentic
rendering, and deterministic replay — and is structurally behind on platform
reach (no macOS/mobile/console) and ecosystem (solo developer).

vs Godot 4, verified against source:

- AHEAD: ray tracing (full pipeline + denoisers; Godot has none), FSR2
  upscaling, retro pipeline (PS1/N64 modes, 11 hardware-measured CRT models,
  palette locks), Jolt physics + 6 genre controllers, GPU skinning + motion
  matching, audio (Steam Audio HRTF, event graphs, MIDI, audio-reactive,
  physics audio — the single largest subsystem lead), gameplay batteries
  (save/quest/dialogue/BT/cinematics/rewind — 70+ components vs none),
  accessibility (only EAA-oriented engine), procgen (9 algorithms + editors),
  web export, play-mode diff, collaborative editing, MCP server.
- AT PAR: core PBR/shadows/post, 2D, Box2D, blend trees/IK, asset import,
  LAN networking, desktop platforms, core editor UX.
- BEHIND: visual-script node coverage (~261 nodes ≈ 25% of the ~1,023 script
  bindings), UI canvas flexibility (8 fixed widgets vs scene-tree
  composition), macOS/mobile/console (absent), docs freshness (3 overlapping
  manuals), community (bus factor = 1), engine maturity label (0.9.x).

Top 5 reasons someone picks TEGE (differentiator audit, source-verified):
1. Retro rendering as philosophy — dropdown-correct era looks, hand-crafted
   reflections, anti-screen-space stance.
2. Accessibility as compliance: switch access, dwell click, TTS announcer,
   colorblind GPU modes, dyslexia font — shipped in every exported game.
3. Shareable deterministic replays (.tegereplay, plain JSON, auto-bookmarks).
4. Time architecture: fixed tick + interpolation + Time_SetScale + per-entity
   bullet time as primitives.
5. Gameplay batteries + procgen: save/quest/dialogue/BT/cinematics/rewind and
   nine seeded procedural algorithms, all engine-core.

## 2. Stability actions (personally verified 2026-08-29)

The first draft of this section republished agent findings; line-by-line
source verification then rejected most of them. What follows is only what a
human-level read of the code confirms.

Verified real, worth doing:
- [ ] S1 (LOW) VulkanImage::CreateFromData leaks m_Image/m_Memory on the
      submit-failure cleanup path (the failure itself is handled correctly:
      checked submit, returns false, no view created). VulkanImage.cpp ~327.
- [ ] S2 (LOW-MED, perf) GetMaterialIndex is an unordered_map find per
      entity per draw; an EntityIndex-addressed vector is cheaper.
      RenderSystem.cpp:10009.
- [ ] S3 (TRIVIAL) One unchecked RegisterTexture at init (default white
      texture, RenderSystem.cpp:3172); the other five sites check. Add the
      check for symmetry.
- [ ] S4 (TEST) Play->stop->play skinned transition: mitigated historically,
      keep covered by stress test T4 rather than new code.

Verified already-handled (first draft claimed otherwise):
- VulkanImage UNDEFINED-layout-marked-valid: false — submit failure is
  checked (VK-C3 fix) and the image is never marked valid.
- adr-0004 storage races: mutation paths already call AssertOwnerThread()
  (World.h:103,118) — the proposed guard exists.
- Material SSBO "rebuilds every frame": false — a dirty-flag fast path
  re-uploads a cached staging buffer on clean frames (RenderSystem.cpp:9901),
  per-frame upload being required by frames-in-flight.
- Terrain splatmap overflow: already guarded (kMaxGridElements cap + size
  check, SceneSerializer.cpp:988-992).
- Text texture cache growth: bounded — clears on scene load, erases on
  entity destroy.

Honest bottom line: no verified crash bugs or data-loss paths are currently
open. The engine's stability posture is BETTER than the audit reports said.
The real risks are the untested limits in section 3, not known defects.

Documentation corrections (credibility, do with the next docs pass):
- [ ] D1 `.enjscene` -> `.enjin` in 4 doc locations.
- [ ] D2 BUILD.md still calls ray tracing "placeholder stubs" - it ships.
- [ ] D3 Three overlapping manuals; declare USER_MANUAL.md canonical.
- [ ] D4 Binding/test counts inconsistent between README and site copy.

## 3. Stress-test battery (build in this order)

Each becomes a CI-runnable target (lavapipe where GPU needed). Existing
coverage: StressTest.cpp (10K entities), TestStressFuzz, TestHardening,
120-frame exported smoke, golden capture, --play-cycle probe.

- [ ] T1 Entity scaling: 10K→500K tiers (transform-only / +collider /
      +rigidbody / deep hierarchy). Pass: graceful degradation, no crash
      ≥100K. (90% month-one hit likelihood)
- [ ] T2 Web limits: allocate to the 536MB WASM ceiling, big-pak load,
      IndexedDB quota. Pass: clean OOM behavior, no silent hang. (70%)
- [ ] T3 Draw/material explosion: 1K unique materials/textures, batching
      proof (draw calls << entities), VRAM ceiling probe. (75%)
- [ ] T4 Play/stop cycling: 1000 cycles via --play-cycle incl. skinned
      meshes + scene switches. Pass: bounded memory, no crash. (45%)
- [ ] T5 Physics storm: settle piles at 1K/5K/10K bodies, tumble-box churn,
      slow-mo under load (verify the 4-step clamp slows, never freezes),
      Jolt-vs-Box2D comparison. (60%)
- [ ] T6 Script load: 500 scripts compile, 1K coroutines, exception cascade,
      instruction-cap abort. (55%)
- [ ] T7 Asset extremes: 10M-vertex GLTF, 50MB scene JSON, 100-deep prefab
      hierarchy, 4096 atlas. (50%)
- [ ] T8 Editor endurance: 1K spawn/edit/undo cycles, 100 save/load cycles,
      simulated 8-hour session. Pass: sub-linear memory growth. (40%)
- [ ] T9 Rapid scene switching: deferred-request flood (every frame),
      10-scene cycle, request-during-load. (20%)
- [ ] T10 UI canvas flood: 500 elements, 50-deep nesting, re-layout thrash. (25%)
- [ ] T11 Audio: 500 concurrent sources, 3D spatialization cost. (30%)

## 4. Claim verification (rolling)

The June FEATURE_AUDIT_CHECKLIST procedure stands: every public claim gets
verified against the running build before it appears in README/site copy.
Additions from this audit needing a verification pass before marketing use:
- [ ] Collaborative editing: two-editor session (12 impl files verified;
      needs a live two-instance test).
- [ ] Motion matching: code exists but is REFERENCED NOWHERE outside its own
      files - currently unwired. Do not claim; wire it or park it.
- [ ] Ragdoll/vehicle/soft-body: label as scaffolded/arcade/stub — do NOT
      claim as features.
- [ ] GPU culling: ENABLED by default with CPU fallback (RenderSystem.h:1638)
      - the audit's "gated off" claim was false. Needs a correctness test,
      not enabling.

## 4b. Import & house-system art verification (Marty 2026-08-29)

Deep verification with REAL asset files, not synthetic data — a user's first
hour is importing their own art. Each row: run it, record result, fix or
document the limitation.

3D import + rigging:
- [ ] I1 GLTF/GLB: static mesh, multi-material submeshes, textures embedded +
      external, correct scale/orientation.
- [ ] I2 Rigged character end-to-end: skeleton import, skinning weights, GPU
      skinning correctness, multiple animation clips, blend tree on imported
      clips, retargeting between two rigs, morph targets.
- [ ] I3 FBX / OBJ / DAE via Assimp: same battery as I1 on each format;
      record which features survive each format (FBX units/axis quirks).
- [ ] I4 Failure modes: corrupt file, missing textures, >10M-vert mesh (cap
      message?), non-manifold geometry - clean errors, never crashes.

2D import:
- [ ] I5 Sprites: PNG with alpha, sprite sheet slicing, atlas packing round
      trip, 9-slice, pixel-perfect at retro resolutions.
- [ ] I6 Sprite animation from imported sheets (frame timing, flipping).

Procedural / exotic imports:
- [ ] I7 Gaussian splats: real .ply (INRIA) and .spz (Niantic) captures render
      and integrate with art styles.
- [ ] I9 VOX / PLY meshes if claimed - verify or strike from format list.

Custom art into house systems (the "my own grass" test):
- [ ] H1 Particle emitter with custom texture (the WindParticles leaf path) -
      author texture, tint, verify on desktop AND web.
- [ ] H2 Custom grass/shrub/tree appearance: what CAN a user author on
      vegetation volumes (textures? colors? meshes?) - verify each knob,
      document what is procedural-only.
- [ ] H3 Water: custom scrolling-reflection texture, matcap texture on
      materials, refl probe bake on user meshes.
- [ ] H4 Skybox: user cubemap faces load (6-image set), procedural fallback.
- [ ] H5 Terrain: user splat layer textures, heightmap import if claimed.
- [ ] H6 Custom fonts: TTF/OTF in Text components + UI + subtitles.
- [ ] H7 Audio: user WAV/OGG/MP3 in sources, footsteps, music channels.
- [ ] H8 Wind interaction with user art: custom-texture particles + user
      vegetation responding to WindSystem + Weather_SetWind from script.

## 5. Release gates

Beta gate: S1-S3 done; T1, T2, T4 running in CI green; D1-D2 fixed.
1.0 gate: all S-items done; T1-T8 green in CI; claim list §4 fully verified;
import battery §4b fully run (I1-I7, I9, H1-H8);
docs consolidated (D3); master checklist re-run against the release build.

## Rejected findings (do not re-report)

Verified false during this audit cycle: cgltf strcpy RCE (bounded since
April), per-frame water/parallax mesh regeneration, "--play-cycle probe
doesn't exist" (EditorLayer.cpp:877), "replay frames unbounded" (72,000-frame
cap), "text texture cache unbounded" (scene-clear + destroy-erase), undo/redo
unbounded (capped), VulkanImage UNDEFINED-marked-valid (submit checked, VK-C3),
adr-0004 guard missing (AssertOwnerThread exists on all mutation paths),
material SSBO rebuilt-every-frame (dirty fast path works), splatmap overflow
(kMaxGridElements guard), RegisterTexture unchecked at 3 sites (only the
init-time default is unchecked), GPU culling "gated off" (default ON with CPU
fallback). Motion matching downgraded from "experimental feature" to UNWIRED.
Final false-positive rate across the four reports: roughly a THIRD of
actionable findings. The rule stands and is now doubly earned: no agent
finding enters this document without a human-level read of the cited code.

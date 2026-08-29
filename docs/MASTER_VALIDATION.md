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
1. Flash revival: SWF import + AS2/AS3→AngelScript transpiler + timeline
   editor + one-click web export. No other engine has this path.
2. Retro rendering as philosophy — dropdown-correct era looks, hand-crafted
   reflections, anti-screen-space stance.
3. Accessibility as compliance: switch access, dwell click, TTS announcer,
   colorblind GPU modes, dyslexia font — shipped in every exported game.
4. Shareable deterministic replays (.tegereplay, plain JSON, auto-bookmarks).
5. Time architecture: fixed tick + interpolation + Time_SetScale + per-entity
   bullet time as primitives.

## 2. Stability actions (verified open, ranked)

Ship-blockers for 1.0 — none are active crashes in default paths.

- [ ] S1 (HIGH) VulkanImage layout stays UNDEFINED when the staging submit
      fails; image is then marked valid and later sampled. Mark the image
      invalid on submit failure. Renderer/Vulkan/VulkanImage.cpp ~155-360.
- [ ] S2 (HIGH) adr-0004 contract hardening: GetStorage/GetStorageMut read
      m_ComponentStorages unlocked; safe only while workers never mutate.
      Add a debug-build owner-thread assert on the storage-map insert path and
      a loud comment; flag any future job system as a redesign trigger.
- [ ] S3 (MED) RegisterTexture return not checked at 3 of 6 call sites
      (RenderSystem.cpp:3172, 9815, 12303 — the text/script-RT sites already
      check). 1M-handle pool makes exhaustion pathological; add the checks.
- [ ] S4 (MED) Material SSBO rebuilds on a coarse dirty flag; track
      per-entity dirtiness (perf, not correctness).
- [ ] S5 (MED) GetMaterialIndex is an unordered_map lookup per entity per
      frame; replace with an EntityIndex-addressed vector.
- [ ] S6 (LOW) Terrain splatmap size math unguarded (safe on 64-bit; add a
      cap or a comment). SceneSerializer.cpp:824.
- [ ] S7 (LOW) Play→stop→play skinned transition is mitigated but fragile;
      covered by the stress battery below (T4) rather than code change now.

Documentation corrections (credibility, do with the next docs pass):
- [ ] D1 `.enjscene` → `.enjin` in 4 doc locations.
- [ ] D2 BUILD.md still calls ray tracing "placeholder stubs" — it ships.
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
- [ ] Flash pipeline end-to-end: a real SWF through transpiler to playable web
      export (files verified present; full-path run not yet exercised).
- [ ] Collaborative editing: two-editor session (12 impl files verified;
      needs a live two-instance test).
- [ ] Motion matching: exercised in a scene (code present, maturity unknown).
- [ ] Ragdoll/vehicle/soft-body: label as scaffolded/arcade/stub — do NOT
      claim as features.
- [ ] GPU culling: written but gated off — either enable behind a flag and
      test, or exclude from claims.

## 5. Release gates

Beta gate: S1-S3 done; T1, T2, T4 running in CI green; D1-D2 fixed.
1.0 gate: all S-items done; T1-T8 green in CI; claim list §4 fully verified;
docs consolidated (D3); master checklist re-run against the release build.

## Rejected findings (do not re-report)

Verified false during this audit cycle: cgltf strcpy RCE (bounded since
April), per-frame water/parallax mesh regeneration (water displaces in
shader; parallax moves transforms), "--play-cycle probe doesn't exist"
(EditorLayer.cpp:877), "replay frames unbounded" (72,000-frame cap at the
recording site), "text texture cache unbounded" (clears on scene load,
erases on entity destroy), undo/redo unbounded (capped). Historic pattern
holds: agent audit findings require targeted re-verification — this cycle's
false-positive rate was ~15%.

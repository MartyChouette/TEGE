# Retrospective — VWS Override Layers + Hardening Sweep

**Window:** 2026-06-12 to 2026-06-16
**Scope:** VWS phases 0/1/2a (StableId, LayerStack, LayerSystem) plus a
whole-engine test/hardening/review pass.

## What shipped

| Commit | What |
|--------|------|
| 7cae530 | StableIdComponent — durable u64 entity identity, backfilled on legacy scenes |
| bae92a3 | LayerStack — JSON-level resolve of base + enabled layers, bottom-to-top |
| c42eb6a | LayerSystem — live-edit capture into the active layer (upsert, create, tombstone) |

All three local-only, three commits ahead of `main`. ADR-0001 written for the
decision. Test suite at 86/86 (up from 83/83; the 3 new VWS suites add 52 checks).

## What went well

- **Phased delivery held.** Phase 0 (id + tests), phase 1 (resolve), phase 2a
  (capture) each landed as a self-contained commit with its own integration
  test. No "test it later" debt carried forward.
- **The JSON-level decision paid off.** Resolution and capture both avoided
  touching any of the 80+ component systems. The whole feature is three new
  files plus serializer hooks.
- **Defensive parsing by default.** Every parse path returns the base unchanged
  on failure instead of corrupting state. That habit is why the hardening pass
  found risks rather than active bugs.
- **The test run caught my own mistake fast.** The "all 86 Not Run" scare was a
  missing `-C Release` on a multi-config generator, surfaced and corrected in
  one step. Worth pinning so it does not recur (see actions).

## What was rough

- **The parity commit (78f8fcd) was too big.** 25 files, ~11.8k insertions in
  one commit. Hard to review, hard to bisect. Several unrelated features
  (web FXAA, fire lights, accessibility settings, per-character UI colors) were
  bundled because they were sitting uncommitted together.
- **No design doc before the code.** The ADR was reverse-documented from the
  implementation. The decisions were sound, but the alternatives and risks only
  got written down after the fact.
- **Subagent reporting failed twice.** The security and QA audit agents burned
  ~290k tokens exploring and hit their turn budget mid-investigation without
  emitting a final report, and there was no way to resume them. The hardening
  findings had to be re-derived directly. Lesson for next time: give audit
  agents a hard "stop exploring at N calls and write the report" budget up
  front, and treat their output as advisory rather than a guaranteed artifact.
- **Stale doc claim found.** CLAUDE.md still says script `#include` paths are
  "not yet restricted" when they are. Docs drifted behind the code.

## Hardening findings from this sweep

Three to carry forward (full detail in ADR-0001 Risks and the sweep notes):

1. **Layer input has no count cap** (`LayerStack.cpp:151`). SceneSerializer caps
   verts/indices at 10M but only after the merge builds the full entity vector.
   A hostile layer file can OOM first. Add a per-layer cap before any
   third-party layer loading ships.
2. **JSON nesting depth is unbounded** (`nlohmann::json::parse`, all scene/layer
   parse sites). Add a depth guard.
3. **Script `#include` escape check is weak** (`ScriptEngine.cpp:1147` uses
   `find("..") == 0`, only catches leading `..`; `:157` uses the hand-rolled
   substring check the project's own rules ban). Move to
   `Platform::ResolveWithinRoot`.

None are active exploits and the adversarial test suites pass, so these are
hardening backlog items, not release blockers.

## Action items

- [ ] Pin the `ctest -C Release` invocation (alias, doc note, or a `_test.bat`
      that always passes `-C Release`) so "all Not Run" never reads as a failure
      again.
- [ ] Phase 2b: editor wiring (route inspector/gizmo edits through
      `RecordEdit`), layer panel UI, and persist `LayerStack` to disk on save.
      Layers are memory-only today, so captured edits are lost on close.
- [ ] Fix the three hardening findings before layers can load from any
      untrusted source.
- [ ] Update CLAUDE.md: script `#include` paths ARE restricted now; correct the
      stale line.
- [ ] Default to smaller commits. Split bundled feature work (the 78f8fcd
      pattern) into reviewable units.
- [ ] Write the design doc / ADR before the code on the next VWS phase, not
      after.

## Open questions for phase 2b and beyond

- Are layers per-scene or per-project? (Current: per-scene, in memory.)
- For shipped games: merge layers into the base at build time, or ship with
  layers and resolve at load?
- Collaboration: when two people edit the same entity offline, manual conflict
  resolution, or auto-pick by layer order?

# ADR-0001: VWS Override Layers — Non-Destructive Scene Authoring

## Status
Accepted (phases 1 and 2a implemented and tested; phase 2b editor wiring pending)

## Date
2026-06-16

## Context

### Problem Statement
Scene editing writes directly into the `.enjin` file. Every edit mutates the
one canonical document, so there is no way to keep a pristine base scene and
stack reversible changes over it. This blocks three things we want:

- Non-destructive authoring: try a set of edits, then drop them without hunting
  for what changed.
- Git-friendly collaboration: two people editing the same scene produce
  conflicting diffs on one large JSON file.
- A reliable PlayMode diff: today `PlayModeDiff` matches entities by name as a
  workaround because the runtime `Entity` handle is not durable across loads.

The Virtual Workspace (VWS) initiative is the "DAW for games" model: a base
scene plus a stack of layers, each layer holding sparse, reversible edits, the
way a DAW keeps the source audio and stacks non-destructive edits on tracks
above it.

### Constraints
- The engine has 80+ component types. Any design that requires per-component
  work does not scale and will rot as components are added.
- Must load through the existing `SceneSerializer` with no fork of the load
  path.
- Must not change the on-disk `.enjin` format for scenes that do not use layers
  (backward compatible).
- Layers must serialize in a form that produces clean Git history.

### Requirements
- Address entities durably across scene reloads.
- Capture an edit cheaply, without snapshotting the whole world per edit.
- Resolve a base plus N layers into a loadable scene at roughly the cost of one
  ordinary scene load.
- Edits must be fully reversible by disabling or deleting a layer; the base is
  never touched.

## Decision

Implement scene editing as a stack of sparse, non-destructive layers over a
pristine base scene, addressed by a persistent stable id, resolved at the JSON
level, and serialized one file per layer.

Three pieces:

1. **StableIdComponent** (`Engine/include/Enjin/ECS/Components/StableId.h`).
   A persistent `u64` id assigned once on entity creation, persisted in the
   `.enjin`, and backfilled on legacy scenes. This is the durable address that
   layers use, independent of the runtime generational `Entity` handle.

2. **LayerStack** (resolve side, `Engine/src/Scene/LayerStack.cpp`).
   Parses the base scene JSON, applies each enabled layer bottom-to-top by
   setting and erasing keys on the entity JSON objects, then hands the merged
   document to `SceneSerializer::LoadFromString`. Resolution is pure JSON
   manipulation with no per-component plumbing.

3. **LayerSystem** (capture side, `Engine/src/Scene/LayerSystem.cpp`).
   When the editor mutates a component, it calls `RecordEdit(entity, key)`.
   LayerSystem serializes only that one component and upserts the delta into the
   active layer. Creation captures all components; destruction either drops the
   delta (if this layer created the entity) or writes a tombstone.

### Architecture Diagram

```
            capture side                         resolve side
  ┌─────────────────────────────┐      ┌──────────────────────────────┐
  │ Editor edits live World      │      │ base.enjin (pristine JSON)    │
  │   inspector / gizmo          │      │            +                  │
  │        │                     │      │   Layer 0 ┐                   │
  │        ▼                     │      │   Layer 1 ├ enabled, bottom→top│
  │ LayerSystem.RecordEdit(e,k)  │      │   Layer N ┘                   │
  │   EnsureStableId(e)          │      │            │                  │
  │   SerializeOneComponent(e,k) │      │            ▼                  │
  │   upsert into active Layer   │      │   LayerStack.Resolve()        │
  │        │                     │      │   (JSON set/erase per key)    │
  │        ▼                     │      │            │                  │
  │ Layer (in memory)  ──────────┼─────▶│            ▼                  │
  │ Layer.ToJson() → layerN.json │      │   merged JSON                 │
  └─────────────────────────────┘      │            │                  │
                                        │            ▼                  │
                                        │   SceneSerializer.LoadFromString
                                        │            │                  │
                                        │            ▼                  │
                                        │     resolved World            │
                                        └──────────────────────────────┘
```

### Key Interfaces

```cpp
// Addressing
struct StableIdComponent { u64 id; };
u64 GenerateStableId();   // thread_local MT19937_64, never returns 0

// Data model
struct ComponentDelta { std::string key; std::string json; }; // empty json = remove
struct EntityDelta    { u64 stableId; bool created; bool destroyed;
                        std::vector<ComponentDelta> components; };
struct Layer          { std::string name; bool enabled; bool locked;
                        std::vector<EntityDelta> entities;
                        std::string ToJson(bool pretty) const;
                        static Layer FromJson(const std::string&); };

// Resolve
struct LayerStack {
    std::vector<Layer> layers;
    std::string Resolve(const std::string& baseSceneJson) const;
    DeserializationResult ResolveInto(ECS::World&, const std::string& baseSceneJson) const;
};

// Capture
class LayerSystem {
    void RecordEdit(Entity e, const std::string& key);
    void RecordRemoveComponent(Entity e, const std::string& key);
    void RecordCreate(Entity e);
    void RecordDestroy(Entity e);
    void ResolveIntoWorld();
};
```

## Alternatives Considered

### Alternative 1: Per-component capture/apply hooks in every system
- **Description**: Each system (serializer, physics, renderer, ...) exposes
  hooks so layers capture and reapply edits at the component level in native
  form.
- **Pros**: Type-aware; no JSON round-trip; could be faster per edit.
- **Cons**: Requires plumbing through all 80+ component types and every new one
  forever. High surface area, high rot risk.
- **Rejection Reason**: Does not scale with the component count. The whole point
  was to add layering without touching every system.

### Alternative 2: Whole-scene snapshot per edit
- **Description**: On each edit, snapshot the full world state and diff against
  the previous snapshot.
- **Pros**: Conceptually simple; captures everything.
- **Cons**: A JSON round-trip of the entire world on every edit. In prototyping
  this ran out of memory on heavy skeletal scenes.
- **Rejection Reason**: Cost scales with scene size per keystroke, not per edit.

### Alternative 3: Address entities by runtime generational handle
- **Description**: Layers reference entities by the ECS `Entity` handle.
- **Pros**: No new component; uses what the ECS already has.
- **Cons**: Handles are remapped on every scene load and recycled with a bumped
  generation, so a saved handle does not survive a reload.
- **Rejection Reason**: Not durable. A layer saved today would point at the
  wrong entity tomorrow.

### Alternative 4: Top-down layer stacking (later layers underneath)
- **Description**: Apply layers in reverse, later layers losing to earlier ones.
- **Pros**: None specific to this engine.
- **Cons**: Breaks the DAW mental model where the base is the lowest track and
  edits stack above. Resurrection of a tombstoned entity by a higher layer
  becomes unintuitive.
- **Rejection Reason**: Bottom-to-top matches the DAW model and gives clean
  tombstone/resurrection semantics.

## Consequences

### Positive
- Zero per-component work. Resolution and capture both operate on JSON, so new
  component types are supported for free.
- Resolve costs about one scene load. No new load path; the merged document
  goes through the existing `SceneSerializer`.
- Reversible by construction. Disabling or deleting a layer restores the base.
- Git-friendly. One file per layer means distinct commits and no merge
  conflicts on a single large scene file.
- Stable id also unblocks a more reliable `PlayModeDiff` (id match instead of
  name match).

### Negative
- Resolution serializes and re-parses component JSON. Acceptable at scene-load
  cadence, wasteful if ever called per-frame.
- Layers live in memory only right now. They are not yet persisted to disk on
  project save, so a session's captured edits are lost on close until phase 2b
  lands.
- No schema versioning on a per-component delta. If a component's fields change,
  an old layer can carry stale keys.

### Risks
- **Unbounded layer input (security).** `LayerStack::Resolve` appends entities
  from untrusted per-layer JSON with no count cap. The 10M vertex/index caps in
  `SceneSerializer` run only after the merge builds the full entity vector, so a
  corrupt or hostile layer file can exhaust memory before any cap fires.
  *Mitigation*: add a per-layer entity/delta count cap in `Resolve` and
  `Layer::FromJson` before this ships to any path that loads third-party layers.
- **JSON nesting depth.** `nlohmann::json::parse` is recursive with no depth
  limit; a deeply nested layer file can overflow the stack. *Mitigation*: parse
  untrusted layer JSON with an explicit depth guard (shared with the scene
  parse path).
- **No layerVersion validation.** `Layer::ToJson` writes `LAYER_FORMAT_VERSION`
  but `FromJson` never reads it. *Mitigation*: check the version on load and
  refuse or migrate unknown versions.
- **Stale deltas accumulate.** A layer can hold deltas for entities a lower
  layer removed. Resolve skips them correctly, but they are dead weight.
  *Mitigation*: a compaction pass when a layer is finalized.

## Performance Implications
- **CPU**: Resolve is O(base entities + total deltas) plus JSON parse of the
  base and serialize of the merged result. One-time at load, not per-frame.
- **Memory**: One working `std::vector<json>` of entities plus the merged
  string, roughly one extra copy of the scene during resolve. Unbounded if a
  layer declares many created entities (see Risks).
- **Load Time**: About one extra scene-sized JSON serialize on top of the normal
  load. Negligible for typical scenes, unmeasured at thousands of entities with
  many layers.
- **Network**: None directly. Per-layer files are intended to travel through Git,
  not the runtime network path.

## Migration Plan
- Legacy scenes without `StableIdComponent` are backfilled on first load and
  the ids persist on the next save. One-time churn in the `.enjin` diff, then
  stable.
- Existing scenes that never use layers are unaffected; the base load path is
  unchanged.
- Phase 2b: route editor inspector and gizmo mutations through
  `LayerSystem::RecordEdit`, add the layer panel UI (enable/disable, rename,
  delete, reorder, active-layer indicator), and persist the `LayerStack` to disk
  on project save.

## Validation Criteria
- Integration tests pass: `StableIdRoundTrip` (13 checks), `LayerResolveTest`
  (22 checks), `LayerCaptureTest` (17 checks). All green as of 2026-06-16
  (86/86 suite).
- An edit captured into a layer, then replayed into a fresh world via
  `ResolveIntoWorld`, reproduces the edited state.
- Disabling a layer returns the resolved scene to the base state.
- Before any third-party layer loading ships: a fuzz/adversarial test for
  oversized and deeply-nested layer files (covers the two security risks above).

## Related Decisions
- StableIdComponent (commit 7cae530)
- LayerStack resolve (commit bae92a3)
- LayerSystem capture (commit c42eb6a)
- Hardening follow-ups tracked from the 2026-06-16 sweep (layer input caps,
  JSON depth guard, script `#include` path check in `ScriptEngine.cpp:1147`)

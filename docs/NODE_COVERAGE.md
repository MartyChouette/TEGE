# VisualScript Node Coverage Audit

Generated 2026-08-14. Compares the VisualScript node catalog against the
AngelScript binding surface to find where a scripter can do something the
node graph cannot.

## Numbers

- **261 nodes** registered in `Engine/src/VisualScript/NodeRegistry.cpp`
- **~940 script bindings** across 36 `ScriptBindings_*.cpp` files (718 global
  functions matched by name plus object methods/properties)
- Node categories are cosmetic — the 90-entry "Gameplay" category is a
  catch-all that actually spans AI, Save, HUD, Particles, Water, Tween, UI,
  Dialogue and more. Coverage below is measured by **functional domain**, not
  by the category label a node happens to carry.

## Method & caveats

Node presence was matched to a binding domain by name (e.g. the "Advance
Dialogue" node covers `Dialogue_Advance`). This is a capability map, not a
1:1 signature diff — a node may take different parameters than the binding.
"Missing" means no node performs that operation at all. The huge
`ScriptBindings_Components.cpp` surface (276 funcs) exposes every field of
every component; nodes intentionally cover only the common ones, so it is
excluded from the gap ranking (a node for every component setter is not the
goal).

## Domain coverage

| Domain | Bindings | Nodes | Coverage |
|---|---|---|---|
| Math / Logic / Vector / Flow | (core) | 60 | **Full** — this is the node sweet spot |
| Transform / Entity spawn | ~12 | 17 | **Full** for pos/rot/scale/spawn/destroy |
| Physics (forces, raycast, overlap) | 27 | 15 | Good — **joints entirely missing** |
| Audio | 12 | 10 | Good — no positional play, no pitch |
| Particles / Elemental / Water | ~60 | ~18 | Good on the common verbs |
| Save | 15 | 9 | Partial — **only Float meta has nodes** |
| Dialogue | 10 | 3 | **Weak — no choices, no text read** |
| AI / BehaviorTree / Navmesh | 34 | 9 | **Weak — no tuning, no blackboard, no pathfind query** |
| Scene / Hierarchy / Tags | 33 | 17 | Partial — **no parent/child, no tags** |
| UI | 35 | ~8 | Partial — no read-state, no styling |
| HUD | 15 | 5 | Partial — no styling/binding |
| Weather | 14 | 3 | Partial — no reads, wind, lightning |
| Sprite | 13 | 4 | Partial — **no sprite animation nodes** |
| Text | 8 | 2 | Partial |
| Tween | 10 | 4 | Partial — no opacity/complete/stop |
| StateMachine (runtime API) | 20 | 2 | **Weak — get/set state only** |
| Networking | 20 | 6 | **Weak — no lobby/ownership/RPC-register** |
| Accessibility | 42 | 3 | **Weak — pillar feature, mostly script-only** |
| Rewind (time) | 11 | 0 | **None** |
| MIDI | 11 | 0 | **None** |
| InputAction (action mapping) | 21 | 0 | **None** (raw Input has 4 nodes) |

## Ranked gap list (what to add first)

Ranked by how likely a node-only game builder hits the wall. Each line is a
concrete "add these nodes" work item.

### Tier 1 — blocks common beginner games
1. **Dialogue choices & text** — `Dialogue_Choose`, `GetChoiceCount`,
   `GetChoiceText`, `GetCurrentSpeaker`, `GetCurrentText`, `Get/SetVariable`.
   Nodes can start/advance dialogue but cannot present a branching choice or
   read the current line. Branching dialogue is impossible in pure nodes today.
2. **Scene hierarchy** — `Entity_GetParent/SetParent/RemoveParent/GetChild/
   GetChildCount`. No way to reparent or walk children from a graph.
3. **Tags** — `Scene_AddTag/RemoveTag/HasTag/FindEntityByTag`. Tag-based
   queries are a staple ("find all enemies") and have zero node coverage.
4. **Sprite animation** — `SpriteAnim_Play/Stop/SetSpeed/IsPlaying/
   GetCurrentFrame`. 2D games can set a texture but can't drive frame anim.
5. **Save meta (non-float)** — `Meta_Get/SetBool/Int/String`. Only Float has
   nodes; a scripter can't persist a bool flag or a string via nodes.

### Tier 2 — needed for polish / core pillar
6. **Accessibility** — the 42-binding surface (font scale, contrast, reduced
   motion, screen reader, subtitles, colorblind strength) has 3 nodes. This is
   a stated engine pillar; node builders should be able to wire an in-game
   accessibility menu. Add Get/Set nodes for the settings + subtitle controls.
7. **AI tuning & navmesh query** — `AI_Set/GetMoveSpeed`, `DetectionRange`,
   `AttackRange`, `SetTargetPosition`, `Navmesh_FindPath/PathExists/
   GetPathWaypoint`. AI state nodes exist but you can't tune the agent or
   query a path.
8. **BehaviorTree blackboard** — `BT_Get/SetBlackboardBool/Float/Int/String`.
   BT enable/reset nodes exist but the blackboard (how BTs actually carry
   data) is script-only.
9. **Entity direction vectors** — `Entity_GetForward/Right/Up`. Common for
   "move forward" / "shoot ahead" logic; Look At exists but not the basis
   vectors.
10. **Tween completion & opacity** — `Tween_Opacity`, `SetOnComplete`,
    `GetValue`, `StopAll`. Without on-complete a graph can't sequence tweens.

### Tier 3 — domain-specific, lower frequency
11. **Physics joints** — `Physics_CreateHingeJoint/CreateDistanceJoint/
    DestroyJoint` + motor/limit/stress accessors. No joint nodes at all.
12. **UI read-state & styling** — checkbox/slider read, hover/press/focus
    query, per-char color, image path/alpha, tab order.
13. **HUD styling** — fill color, font size, position/size, world offset,
    field binding.
14. **Weather reads + wind/lightning** — get densities, `SetWind`,
    lightning interval / just-fired.
15. **Networking lobby & ownership** — player count/name/ready, role,
    `RequestOwnership`, `Register/UnregisterEntity`, `CallRPCAll`.
16. **StateMachine runtime construction** — add state/transition, params
    (bool/float/int), triggers. (May be intentional — nodes could model SMs
    graphically instead of via this API. Decide before adding.)
17. **Audio extras** — `Audio_PlayAtPosition` (positional one-shot),
    `SetPitch`, master volume get/set.

### Tier 4 — whole systems with zero nodes (decide: node-worthy?)
18. **Rewind / time** (11 bindings) — niche mechanic; add only if a template
    needs it.
19. **MIDI** (11 bindings) — music-tool territory; likely stays script-only.
20. **InputAction** (21 bindings) — the action-mapping abstraction over raw
    input. Raw key/mouse/gamepad nodes exist (4); the rebindable-action layer
    does not. Worth a few nodes if node games should support rebinding.

## Suggested next step

Tier 1 is ~20 nodes and would remove the sharpest "nodes can't do this"
walls (branching dialogue, hierarchy, tags, sprite anim, save flags). That is
the natural follow-up work item to this audit.

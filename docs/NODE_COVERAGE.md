# VisualScript Node Coverage Audit

Generated 2026-08-14. Compares the VisualScript node catalog against the
AngelScript binding surface to find where a scripter can do something the
node graph cannot.

## Status (2026-08-14)

**All of Tier 1 is DONE** (27 nodes added, tested in `TestVisualScript`):
- Hierarchy, tags, save-meta bool/int/string, sprite animation (20 nodes).
- Branching dialogue (7 nodes): Choose, Choice Count, Choice Text, Speaker,
  Text, Set/Get Variable. This one required threading a `DialogueSystem*`
  into `ExecutionContext` (wired through `VisualScriptSystem`, set at the
  PlayMode/Player/web call sites) — reads are pure component lookups, but
  Choose and the variable nodes use the runtime tree player on the system.

**Tier 2 is now essentially DONE too:**
- Accessibility (17 nodes): font scale, reduced motion, screen-shake, contrast,
  colorblind strength, subtitles, dyslexia font, screen reader get+set, Save.
  Forward through helpers in `ScriptBindings_Accessibility.cpp` that reuse the
  AngelScript settings pointer + apply/save callbacks.
- AI tuning (11 nodes): get/set move speed, detection range, attack range; set
  chase/flee speed, field of view, use-navmesh, target position.
- Entity direction vectors (3 nodes): Get Forward/Right/Up.
- BehaviorTree blackboard (8 nodes): Set/Get Blackboard Bool/Float/Int/String.

Remaining Tier-2 leftovers: **navmesh query nodes** (FindPath/PathExists/
GetPathWaypoint — need a pathfinder pointer or forwarders like accessibility)
and **tween completion/opacity** (Opacity/SetOnComplete/GetValue/StopAll).

## Numbers

- **327 nodes** registered in `Engine/src/VisualScript/NodeRegistry.cpp`
  (261 original + 27 tier-1 + 17 accessibility + 14 AI/direction + 8 BT blackboard)
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
1. ~~**Dialogue choices & text**~~ — **DONE** (Choose, Choice Count, Choice
   Text, Speaker, Text, Set/Get Variable). Branching dialogue now works in
   pure nodes.
2. ~~**Scene hierarchy**~~ — **DONE** (Set/Remove/Get Parent, Get Child Count,
   Get Child).
3. ~~**Tags**~~ — **DONE** (Add/Remove/Has Tag, Find Entity By Tag).
4. ~~**Sprite animation**~~ — **DONE** (Sprite Anim Play/Stop/Set Speed/Is
   Playing/Get Frame, over `AnimatedSprite2DComponent`).
5. ~~**Save meta (non-float)**~~ — **DONE** (Set/Get Meta Bool/Int/String).

### Tier 2 — needed for polish / core pillar
6. ~~**Accessibility**~~ — **DONE** (17 nodes: font scale, reduced motion,
   screen-shake, contrast, colorblind strength, subtitles, dyslexia font,
   screen reader get+set, plus Save). Node builders can now wire an in-game
   accessibility menu.
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

Tier 1 and the Tier-2 Accessibility set are shipped. Remaining Tier-2 items,
in rough value order:
- **AI tuning & navmesh query** (#7): `AI_Set/GetMoveSpeed`, detection/attack
  range, `SetTargetPosition`, `Navmesh_FindPath/PathExists/GetPathWaypoint`.
- **BehaviorTree blackboard** (#8): `BT_Get/SetBlackboard{Bool,Float,Int,String}`.
- **Entity direction vectors** (#9): `Entity_GetForward/Right/Up`.
- **Tween completion & opacity** (#10): `Tween_Opacity/SetOnComplete/GetValue/StopAll`.

Then Tier 3 (physics joints, UI read-state/styling, HUD styling, weather
reads, networking lobby) and Tier 4 (Rewind/MIDI/InputAction whole systems).

# Scripting Surfaces: Parity, Runtime Tradeoffs, and Readability

Enjin exposes three ways to build gameplay: **C++**, **AngelScript**, and **VisualScript nodes**. The goal is that a creator can build a whole game in any one of them and pick the surface that fits how they think and what they need at runtime. This document maps how close each surface is to that goal today, and lays out the runtime strengths, weaknesses, and readability of each so the choice is informed.

Compiled 2026-08-13 from a three-surface inventory audit.

## Coverage snapshot

The three surfaces form a tier, widest capability at the bottom:

| Surface | Reach | Rough size |
|---|---|---|
| **C++** | Everything. It IS the engine. ~100+ ECS component types, 50+ gameplay-facing systems, no sandbox. | The full API |
| **AngelScript** | Near-parity with C++ for gameplay. Every capability that has a binding. | ~1,023 bound functions across ~35 areas |
| **VisualScript nodes** | A large but partial slice of the AngelScript surface. | ~261 nodes across ~20 categories |

So AngelScript is close to C++ for the things a game author actually reaches for, and **the node surface is the bottleneck**: at ~261 nodes it covers roughly a quarter of the bound API by count, and several areas are thin or absent. A game authored purely in nodes hits walls that the same game in AngelScript or C++ would not.

## Where the node surface falls short of AngelScript

The node graph is strong in the areas a designer uses constantly and thin exactly where deeper systems live. Verified against the node registry (`Engine/*/VisualScript/NodeRegistry.*`):

**Solid in nodes** (comparable to script): Math/logic, Flow/events, Transform, Physics (queries + forces), Audio playback, Particles, Tween, Save/checkpoint, Quest, Dialogue, HUD, Light/Material/Camera basics.

**Thin in nodes** (script has much more):
- **AI**: ~4 nodes (set/get state + target) vs the script's ~34 (full state control, behavior-tree enable/reset, blackboard get/set, navmesh pathfinding: FindPath / PathExists / waypoints / cost). A nodes-only AI is very limited.
- **Behavior Trees**: ~2 nodes vs the script's enable/disable/reset + full blackboard.
- **Component access**: nodes can touch only a handful of component types directly (Health, Light, Material, a few more); AngelScript exposes accessors for ~40 component types (Rigidbody, colliders, Inventory, Timer, Interactable, Teleporter, Camera2D/3D, and so on). This is the single biggest gap.
- **UI**: ~6 nodes vs the script's ~35 (sliders, checkboxes, focus/tab order, char coloring, localization).
- **Input**: shallow vs the script's keyboard/mouse/gamepad + the rebindable InputAction layer.
- **Networking**: ~3-6 nodes vs the script's ~20 (host/join, RPCs, ownership, diagnostics).

**Absent from nodes entirely** (script has them): **Rewind** (~11 script fns, 0 nodes), **MIDI** (~11, 0), **Accessibility** runtime toggles (~42 script fns, a handful of nodes), **AudioReactive / BeatClock / Conductor**, **Elemental** depth, **Procedural** generators (partial), **Weather** depth.

## Where AngelScript falls short of C++

Much smaller. AngelScript is close to full parity for gameplay. The gaps are:
- **Unbound components/systems.** A capability is only reachable from script if a binding exists. The ~35 removed-from-review "~35 unbound components" earmark lives here: components with no accessor functions yet. New engine features default to C++-only until bound.
- **No direct memory / raw pointers / custom allocators / new engine systems.** Anything that needs to define a new component type, a new system, or touch the renderer/backend directly is C++ only.
- **The sandbox.** AngelScript runs under a 1M-instruction watchdog and cannot escape it. That is a feature (safety), but it caps what a single script tick can do.

## Runtime strengths and weaknesses

The honest tradeoffs, so the surface is chosen for the right reasons:

| Dimension | C++ | AngelScript | VisualScript nodes |
|---|---|---|---|
| **Raw runtime speed** | Fastest. Native, no interpreter. | Fast enough for gameplay. Compiled to bytecode, runs on a VM. Slower than C++ but rarely the bottleneck for game logic. | Slowest per-operation. Graph traversal has per-node overhead; poor for tight loops and heavy math, fine for event-driven logic. |
| **Iteration speed** | Slowest. Requires an engine/player rebuild (kill editor, recompile, relaunch). | Fast. Hot-reloadable text; edit and re-run without rebuilding the engine. | Fast. Edit the graph and play; no build step. |
| **Capability ceiling** | Total. Can add new components, systems, renderer features. | High. Everything with a binding (~near-full for gameplay). | Partial today (~25% of the bound surface). |
| **Safety** | None. A mistake can crash or corrupt the engine. | Sandboxed. Instruction cap, no raw memory, contained failures. | Sandboxed, same runtime as script. Hard to crash the engine from a graph. |
| **Skill floor** | High. You are an engine programmer. | Medium. C-like text programming, but forgiving and hot-reloaded. | Low. No syntax; discoverable, visual, designer-friendly. |
| **Version control / diff / merge** | Clean (text). | Clean (text), diffable, reviewable. | Poor. Graphs are data, not line-diffable; merges are painful. |
| **Readability at small scale** | Verbose but precise. | Concise and clear for anyone who reads code. | Excellent. The flow is literally drawn; non-programmers follow it. |
| **Readability at large scale** | Scales with normal code discipline. | Scales with normal code discipline. | Degrades. Large graphs become spaghetti; no functions/abstractions unless the node system provides them. |
| **Can build a whole game solo, today** | Yes (you are effectively building on the engine). | Yes for most games; a few unbound capabilities need C++. | Not yet. The node coverage gap forces dropping to script/C++ for AI depth, rewind, many component tweaks, etc. |

The short version: **C++ trades iteration and safety for total power; AngelScript is the balanced middle (safe, hot, near-full capability); nodes trade capability for approachability.** Nodes are the most readable at small scale and the least at large scale, and they are the only surface that cannot yet carry a whole game alone.

## The same behavior in all three surfaces

"When the player enters this trigger, play a hurt sound and deal 10 damage." This shows the readability difference at a glance.

**C++**
```cpp
void OnTriggerEnter(Entity self, Entity other) {
    if (auto* hp = world.GetComponent<HealthComponent>(other)) {
        hp->Damage(10.0f);
        audio.Play("hurt.wav");
    }
}
```

**AngelScript**
```angelscript
void OnTriggerEnter(uint64 other) {
    Health_Damage(other, 10.0f);
    Audio_Play("hurt.wav");
}
```

**VisualScript nodes**
```
[Event OnTriggerEnter]--exec-->[Health Damage (amount 10)]--exec-->[Audio Play ("hurt.wav")]
        \--(other entity)-------------^
```

All three do the same thing. The node graph is the fastest to read for a non-programmer and the two text forms are the fastest to diff, search, and refactor.

## Path to full parity

The work to make "build a whole game in any surface" true is, in priority order:

1. **Close the node-vs-script gap** (the dominant gap). Generate/author nodes to match the AngelScript surface, area by area, starting with the biggest holes: generic component accessors (get/set on the ~40 bound component types), AI + behavior-tree + navmesh, then Input/UI depth, then the absent areas (Rewind, MIDI, Accessibility, AudioReactive). Much of this can be code-generated from the same registration data the bindings use, since both ultimately call the same engine functions.
2. **Close the script-vs-C++ gap.** Bind the ~35 still-unbound components/systems so no gameplay capability is C++-only. New engine features should ship a binding in the same change.
3. **Node readability at scale.** Functions/subgraphs, collapse/reroute, and a comment/group system so large graphs stay legible (the thing that otherwise pushes teams off nodes).
4. **Keep the three in lockstep.** When a new capability lands in C++, it should get an AngelScript binding and a node in the same pass, or it silently becomes a parity gap again.

## References

- AngelScript bindings: `Engine/src/Scripting/ScriptBindings*.cpp` (~35 files); doc `docs/SCRIPTING_API.md`.
- VisualScript nodes: `Engine/*/VisualScript/NodeRegistry.*`, `NodeDefinition.h`.
- C++ capability baseline: `Engine/include/Enjin/ECS/Components/` (~100+ types), `Engine/include/Enjin/{Gameplay,AI,Animation,Audio,GUI,Scene}/` systems.

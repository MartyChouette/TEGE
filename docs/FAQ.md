# TEGE FAQ and Mental Model

This is the "how does the whole thing actually work" doc. It is written for every
level: someone who has never opened an engine, someone who lives in code, and
everyone in between. It explains the flow, the places you can inject your own
logic, and the handful of nuances that will save you a bad afternoon.

For exhaustive function lists, see `SCRIPTING_API.md`. For a first-hour walkthrough,
see `BEGINNERS_GUIDE.md`. For component details, see `USER_MANUAL.md`. This doc is
the map those three hang off of.

---

## What is TEGE

An opinionated, aesthetic-forward game engine that gets you building in three
clicks and gives greater accessibility to obsolete rendering styles. It loves
4th to 6th generation console looks (PS1, N64, Saturn, PS2, GameCube) that are
usually gatekept, and it wants you playing with those styles the way you would
play with a synth patch, not fighting a renderer to get them.

It is built from scratch in C++20. Multi-backend renderer (Vulkan on desktop,
WebGPU in the browser). Entity-Component-System core. A full editor. Everything
is open (BSL 1.1) and the file formats are plain JSON on purpose.

---

## The mental model

Five nouns and one verb. Learn these and the rest is detail.

- **Project** (`.enjinproject`). A JSON manifest: which scenes exist, which one
  starts, window and build settings. One project is one game.
- **Scene** (`.enjin`). A JSON file of entities. A level, a menu, a room. A
  project has as many as you want; exactly one is the start scene (`isStartScene`).
- **Entity**. A thing in a scene. It has no behavior on its own. It is an ID and
  a bag of components. Entities can be parented into hierarchies.
- **Component**. The behavior and data. A Transform (position/rotation/scale), a
  Mesh, a Light, a Camera, a Rigidbody, a Script, a Tilemap, and 140-plus others.
  You build a thing by stacking components on an entity.
- **System**. The engine-side code that makes components do their job each frame
  (rendering, physics, controllers, scripts). You rarely touch systems directly;
  you configure components and the systems read them.

The verb is **Play**. Press Play and the scene comes alive under the systems.
Press Stop and it goes back to how you authored it. Changes you make while
playing are a sandbox; they persist only if you explicitly keep them.

So the whole loop is: make a project, put entities in a scene, stack components on
them, press Play, then export when it is a game.

---

## How deep do you want to go: the layers of control

TEGE is built so you can stop at any layer. Each one is a real place to inject
your own logic. Most games mix several.

### Layer 0 — components and inspectors (no code)

Select an entity, add components, tune their fields in the Inspector. A moving
platform, a light that flickers, a character that walks, a full 2D or 3D scene
can be built without writing a line. Every component's Inspector panel carries a
self-documenting help block: what it does, how to use it, and a live "Connects to"
list showing which other components it expects and letting you add them. Press
Tab in the Inspector to flip an entity into a wiring board that shows its
components as modules with the connections drawn between them.

### Layer 1 — visual scripting (node graphs, still no text)

Open the Visual Script editor and wire logic as nodes: events, conditions, math,
component reads and writes, flow. Attach a `VisualScript` component to an entity
and the graph runs with it. This is the bridge for people who think in flow
diagrams rather than syntax. Around 347 node types.

### Layer 2 — AngelScript (real code)

Attach a `Script` component, point it at a `.as` file, write a class that extends
`TegeBehavior`. This is the main "inject your own code" surface and it is covered
in its own section below. AngelScript is C-like and sandboxed (a 1M instruction
ceiling, includes restricted to your script folder).

### Layer 3 — shaders (your own look)

The Shader Graph editor builds materials as node graphs and compiles them to real
GPU pipelines you can apply to an entity. If you want a look the art-style presets
do not cover, this is where you author it. The graph is saved with the scene so it
survives reload.

### Layer 4 — procedural generation (author by rule, not by hand)

A suite of components that build content from a seed: `DungeonGen` (rooms and
corridors into a Tilemap), `Terrain` (FBM heightfields with erosion, auto-splat),
`Scatter` (prefabs placed by Poisson/Voronoi/grid, can conform to terrain), `WFC`
(wave-function-collapse tiles in 2D or 3D), `RandomBag` (a controllable random
feeder, including Markov chains). Everything is seeded, so the same seed always
rebuilds the same world.

### Layer 5 — bring your own agent (MCP)

The editor can expose a local MCP server (Settings > System > MCP Server, off by
default). Point any MCP-capable agent at it and it can create entities, read and
write components, edit AngelScript, author node graphs, control play, and capture
the view. Your automation, your agent, no lock-in, no inference cost from us.

---

## Writing an AngelScript: the TegeBehavior lifecycle

Attach a `Script` component to an entity, point it at `scripts/MyThing.as`, and
write:

```angelscript
class MyThing : TegeBehavior {
    void OnStart() {
        Debug_Log("hello from " + Entity_GetName(entity));
    }

    void OnUpdate(float dt) {
        // runs every rendered frame; dt is seconds since last frame
    }
}
```

`TegeBehavior` is embedded in the engine and injected automatically, so you do not
`#include` it. Your class gets an `entity` member: the ID of the entity the script
is attached to. That ID is the key to everything else (see the next section).

You implement only the hooks you need. The engine calls the ones you define:

- `OnCreate()` — once, the moment the instance is made, before the scene is live.
- `OnEnable()` / `OnDisable()` — when the script is enabled or disabled.
- `OnStart()` — once, on the first frame of gameplay. Do setup here, not in
  OnCreate, because by OnStart the rest of the scene exists.
- `OnUpdate(float dt)` — every rendered frame. Your main loop. Frame time varies,
  so scale movement by `dt`.
- `OnFixedUpdate(float dt)` — on a fixed timestep, decoupled from framerate. Do
  physics-facing work here so it stays stable regardless of frame rate.
- `OnLateUpdate(float dt)` — every frame, after all OnUpdates. Good for camera
  and follow logic that must read everyone else's final position.
- `OnCollisionEnter/Stay/Exit(uint64 other)` — physical contact with another body.
  `other` is the entity you hit.
- `OnTriggerEnter/Exit(uint64 other)` — overlap with a trigger (a non-solid
  sensor collider). This is how pickups, doors, checkpoints, and zones fire.
- `OnAnimationEvent(string name)` — an animation reached a tagged frame (footstep,
  hit frame). Wire timing to logic without counting frames yourself.
- `OnDestroy()` — the entity is going away. Clean up.

Roughly, the order of a life is: OnCreate, OnEnable, then each frame OnStart (once)
into OnUpdate into OnFixedUpdate (as many as the timestep needs) into OnLateUpdate,
with collision and trigger callbacks firing from the physics step, and OnDestroy at
the end.

### Exposing fields to the Inspector

Annotate a script field and it shows up as a tunable in the Inspector, so a
designer can retune your script with no code:

```angelscript
class Spinner : TegeBehavior {
    [Property] float speed = 90.0;                 // a field in the Inspector
    [Property, Range(0, 360)] float startAngle = 0; // with a slider range

    void OnUpdate(float dt) {
        Vector3 r = Entity_GetRotation(entity);
        r.y += speed * dt;
        Entity_SetRotation(entity, r);
    }
}
```

This is the handoff between the code layer and the no-code layer. You write the
behavior once; whoever builds the level tunes it without opening the file.

---

## Manipulating components from code

Everything is keyed off an entity ID (`uint64`). You get IDs from `entity` (your
own), from `Scene_FindEntity("Name")`, from `Scene_FindEntityByTag("enemy")`, from
a collision/trigger callback's `other`, or from spawning with `Scene_Instantiate*`.

With an ID you read and write components through prefixed functions:

```angelscript
uint64 door = Scene_FindEntity("Door");
Entity_SetPosition(door, Vector3(0, 3, 0));   // transform
Light_SetIntensity(lamp, 2.5);                 // light
Material_SetBaseColor(sign, Vector3(1, 0, 0)); // material
Health_Damage(enemy, 10.0);                    // health
Physics_AddImpulse(ball, Vector3(0, 8, 0));    // rigidbody
Animator_CrossFade(hero, "run", 0.2);          // animation
```

The pattern is consistent: `Component_Verb(entity, ...)`. Ask before you touch if
you are not sure a component is there: `HasComponent_Health(e)`, `Camera_HasVCam(e)`,
and so on. The full catalog is in `SCRIPTING_API.md` (around 1,010 functions).

To make new things, spawn: `Scene_Instantiate()`, `Scene_InstantiateNamed("Bullet")`,
`Scene_InstantiateAt(pos)`. To remove: `Scene_DestroyEntity(e)` (deferred to a safe
point in the frame). To relate: `Entity_SetParent(child, parent)`.

---

## Triggers, collisions, events, and loops: how logic fires

There are four ways a piece of your code runs, and picking the right one is most of
"thinking in the engine."

- **A loop** runs your code every frame or every fixed step: `OnUpdate` /
  `OnFixedUpdate`. Use it for continuous things (movement, timers, watching input).
- **A trigger** fires when something enters or leaves a sensor volume:
  `OnTriggerEnter/Exit`. Use it for "when the player steps here" (pickups, doors,
  damage zones, checkpoints). A trigger collider is a collider with its sensor flag
  on; it detects overlap but does not physically block.
- **A collision** fires on solid contact: `OnCollisionEnter/Stay/Exit`. Use it for
  "when these two things actually hit" (a projectile striking a wall, two bodies
  bouncing).
- **An event** is a named message on the event bus. `Events_Listen("boss_died",
  callback)` registers a function to run whenever anyone sends that event, and you
  broadcast your own with `Events_Send`. Inside the callback you read the payload
  with `Events_CurrentFloat/Int/String(key)`. Use events to decouple systems that
  should not know about each other directly (UI reacting to gameplay, an achievement
  reacting to a quest).

Input, coroutines, tweens, and timers layer on top: poll input in a loop
(`Input_GetKeyDown(...)`), yield across frames with coroutines, animate a value over
time with `Tween`, and schedule with `Timer` (embedded helper scripts). Time itself
is scriptable: `Time_SetTimeScale(0.5)` for slow motion, `Time_GetDeltaTime()` for
frame-independent math.

---

## Nuances everyone should understand

These are the things that are not wrong, just non-obvious, and they cut across all
levels.

- **2D and 3D physics never mix.** 2D scenes use Box2D, 3D scenes use Jolt, and
  they are separate worlds. A 2D scene uses the 2D controllers and 2D colliders; a
  3D scene uses the 3D ones. Do not put a 3D rigidbody in a 2D scene expecting them
  to talk.
- **Collider sizes are world space.** A box collider of size 50 is 50 world units,
  regardless of the entity's scale. The physics engines do not multiply collider
  size by transform scale. If a collider looks wrong, check its own size, not the
  entity scale.
- **The camera has one owner.** In 3D, if you use Virtual Cameras, the Camera
  Director is the single writer of the real camera. You drive vcams (priority,
  targets, offsets) and it does the blending. You never fight it for the transform.
  If a scene has no vcams, the Director is dormant and your controller or cinematic
  camera runs as normal. It is opt-in per scene.
- **Play mode is a sandbox.** Changes made while playing revert on Stop unless you
  keep them. This is a feature: experiment freely. The flip side is do not expect
  play-time tweaks to save themselves.
- **Script files live in your project's `scripts/` folder** and includes are
  restricted to that folder for safety. A script's module name is derived from its
  folder and filename (`scripts/Foo.as` becomes `scripts_Foo`), which matters if you
  spawn instances by name.
- **The start scene is authoritative via `isStartScene`.** Build order and inclusion
  are a separate setting. If your game boots the wrong scene, check which scene has
  `isStartScene`.
- **Determinism is real but same-machine.** Seeded procedural generation and the
  replay system reproduce exactly on the same build and machine. Floating point
  across different machines can diverge; treat cross-machine determinism as not
  guaranteed unless stated.
- **Exported games boot to their title screen.** Gameplay (and therefore your
  scripts) starts when the player chooses New Game or Continue, not the instant the
  window opens. If you are testing headless, the player's `--frames N` flag runs N
  frames and auto-starts past the title screen.
- **Accessibility is not a bolt-on.** Colorblind modes, text scaling, input
  remapping, reduced motion, subtitles, and screen-reader-facing hooks are built in
  and meant to be used from the start, not retrofitted.

---

## Where things live

- Editor: build scenes, tune components, write scripts and graphs, press Play,
  export.
- Project files: `.enjinproject` (manifest) and `.enjin` (scenes), both plain JSON
  you can read and diff. Scripts are loose `.as` files in `scripts/`.
- Exported game: a player executable plus a `.enjpak` (packed assets) plus loose
  `scripts/` and `assets/`. Ships for Windows, Linux (including Steam Deck), and the
  web.

## Where to go next

- `BEGINNERS_GUIDE.md` — your first hour, step by step.
- `SCRIPTING_API.md` — every callable function, by category.
- `USER_MANUAL.md` — component-by-component detail.
- `ARCHITECTURE.md` — how the engine is put together, if you want to hack on it.
- `MCP.md` — connecting your own agent to the editor.
- In the editor itself: every component panel explains itself, and the command
  palette (Ctrl+P) is the fastest way to find anything.

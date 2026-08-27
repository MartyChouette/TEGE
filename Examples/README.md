# Enjin Examples

## The three-workflow demo

The same bite-size game — collect 3 orbs, win — built three ways. Pick the
workflow that fits you; they are deliberately mirrors of each other so you can
compare like for like:

| Workflow | Where | Code required |
|---|---|---|
| **Components only** | Editor: Project Hub template "Components Only" | none — every behavior is an Inspector component (Pickup, Damage, TriggerZone, GameOver, HUDWidget) |
| **Script only** | Editor: Project Hub template "Script Only" | one AngelScript file (`scripts/GameScript.as`) holds all game logic; hot-reloads while playing |
| **C++ only** | `OrbCollector/` in this folder | one C++ file against `Enjin::App`; you own the build |

Each editor template opens with the relevant panels visible and carries a
`-- Guide --` entity (Notes component) walking through how it works.

## Building the C++ examples

Enable examples in CMake, then build:

```
cmake -B build -DENJIN_BUILD_EXAMPLES=ON
cmake --build build --config Release --target ExampleOrbCollector
```

Output lands in `build/bin/Examples/`.

## Example list

- **OrbCollector** — the C++ tier of the three-workflow demo (above). Ground,
  lighting, three spinning collectibles, proximity collection, win state — in
  about 60 lines of `Enjin::App`.
- **Simple3D** — minimal 3D scene: plane, cubes, fly camera. The smallest
  possible `Enjin::App`.
- **Simple2D** — minimal 2D scene using `Enjin::App2D`: sprites and 2D camera.
- **Triangle** — the lowest level: raw `Enjin::Application` with your own
  renderer setup. For people who want to see the plumbing.

## Scene examples (no build — open in the editor)

- **WaterShowcase** — all five 3D water styles (Flat, Animated, VertexWave,
  Reflective, Refractive) side by side, with floating cubes so the reflective
  styles have something to mirror. Open `WaterShowcase/WaterShowcase.enjinproject`
  and press Play. See `WaterShowcase/README.md` for the layout.

## The C++ API in one breath

```cpp
#include "Enjin/App.h"

class MyGame : public Enjin::App {
    void OnStart() override   { /* build your scene: AddCube, LoadModel, lights */ }
    void OnUpdate(float dt) override { /* your logic */ }
};

ENJIN_SIMPLE_MAIN(MyGame)
```

`Enjin::App` boots the window, renderer, ECS world, and default systems.
`GetWorld()` / `GetRenderer()` / `GetCamera()` expose the full engine when the
convenience helpers run out. API stability note: the C++ surface is young —
expect breaking changes between 0.x releases.

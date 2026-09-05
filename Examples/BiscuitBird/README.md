# Biscuit Bird

A 3D one-button flyer for TEGE. You are a bird with a nest at the top of a park
tree. Dive at the people walking the loop below, shake the biscuits out of their
bags, catch the biscuits before they rot on the grass, and carry them home.

Open `BiscuitBird.enjinproject` in the editor and press Play.

## Controls

| | |
|---|---|
| Tap / click, `Space`, `W`, `Up` | flap |
| Hold left or right of screen centre | bank that way |
| `A` / `D` or arrow keys | bank that way |

One button is the whole game. Tapping while diving is a real recovery; tapping
while already climbing does not stack, so you cannot mash your way into orbit.

## The loop

1. Tap to leave the nest. Gravity is always on and you always drift forward.
2. Dive at a pedestrian — you need to be moving (7 m/s) and low (under 5 m).
   They yelp, crouch, and drop 2–4 biscuits. The faster the dive, the more
   they drop.
3. Biscuits tumble, bounce once, and rest on the ground for 26 seconds. They
   blink for the last four.
4. Fly through a biscuit to pick it up. You can hold four.
5. Fly back into the nest to bank them. 10 points each, +20 for a full beak.

A run is 120 seconds. When the clock hits zero the bird glides home and the
banner offers a retry.

Each pedestrian gets a 6.5-second cooldown after being startled, so camping one
person is worse than working the loop.

## Layout

```
BiscuitBird.enjinproject   project + start scene
scenes/Main.enjin          the whole park (generated)
scenes/PaintTest.enjin     texture-painting bench (generated)
scripts/BirdFlight.as      flight model, wings, chase camera
scripts/BiscuitGame.as     crowd, biscuits, carrying, score, clock, HUD
assets/*.wav               flap / pickup / deposit / yelp (generated)
assets/tex_*.png           paintable texture set (generated)
tools/gen_scene.py         rebuilds the scenes, the project file and the SFX
tools/gen_textures.py      rebuilds the texture set
```

`tools/gen_scene.py` is the source of truth for the scene. Run it from the
project root to rebuild everything (it calls `gen_textures.py` for you):

```
python tools/gen_scene.py
```

**Careful:** that regenerates `assets/tex_*.png` too, so it overwrites anything
you have painted. Once you start painting for real, comment out the
`gen_textures.generate_all()` call at the bottom of `gen_scene.py`.

Editing `scenes/Main.enjin` in the editor works fine, but a regeneration
overwrites it — move a change back into the generator if you want to keep it.

## How it is built

**No physics.** The bird, the crowd and the biscuits are all script kinematics.
Flight feel is then identical on every machine, nothing can knock the bird out
of the air, and there are no colliders to tune. Hits are distance checks.

**No runtime spawning.** `Person0..9`, `Biscuit0..47` and `Carry0..3` all exist
in the scene from the start; the game shows, hides and moves them. A run never
allocates and the whole cast is visible in the hierarchy.

**Four primitive meshes.** Cube, sphere, cylinder/cone and a swept feather panel,
reused and scaled by the entity transform. That is why a 282-entity park is a
1 MB scene file.

**Human scale throughout.** A pedestrian is 1.78 m, a bench seat is 44 cm off
the ground, a lamp post is 4 m, and the bird has a 2.2 m wingspan (it is built
oversized and worn at `BIRD_S = 0.45`, so the whole bird resizes from one
number). Walking is 1.35 m/s, cruising flight is 13 m/s.

## Tuning

Every number worth touching is a `[Property]` on the two scripts, so it is
editable in the inspector without a rebuild:

- `BirdFlight`: `gravity`, `flapImpulse`, `cruiseSpeed`, `diveSpeed`,
  `turnRate`, `maxHeight`, `parkRadius`, `camDistance`, `camHeight`
- `BiscuitGame`: `runTime`, `walkSpeed`, `scareRadius`, `scareSpeed`,
  `pickupRadius`, `nestRadius`, `biscuitLife`, `carryMax`

## Texture painting

Every asset worth painting now carries a texture. The greyscale maps are tinted
by the material's `baseColor`, so one fabric map serves all eight shirt colours
and one bark map serves trunk, branches and roots.

| texture | on | UV layout |
|---|---|---|
| `tex_wafer.png` | biscuits, carried biscuits | cylinder cap = a disc, so this maps as a circle on the wafer face |
| `tex_fabric.png` | shirts, trousers, arms | cube faces, 0–1 per face; the pale centre panel is a chest logo area |
| `tex_bark.png` | trunk, branches, roots | cylinder wall: u wraps, v runs up |
| `tex_straw.png` | nest bowl and twigs | same |
| `tex_paving.png` | paths, spokes, plaza | cube faces, 0–1 per face |
| `tex_feather.png` | wings, tail | u runs root→tip along the span, v across the chord |
| `tex_plumage.png` | body, belly, head | sphere lat/long: u wraps, v runs top→belly |
| `tex_uvchart.png` | the PaintTest scene only | reference chart |

**`scenes/PaintTest.enjin`** is the bench. Open it and you get, on a back board
and a row of pedestals, one of every primitive the game uses wearing a UV chart
(four coloured quadrants, checker, centre cross) plus a second row of the real
assets wearing the real maps. Paint a stroke on the chart and you can read
straight off each shape where that part of the UV square lands.

The round trip, from an object in the scene:

1. Select the object. In the Inspector's material section, **double-click the
   texture thumbnail or its path** (or right-click for a menu, or hit **Paint**).
   The Pixel Editor opens with that texture loaded.
2. Paint. Pencil, fill, line, rect, ellipse, eyedropper, layers, palettes, undo.
3. Hit **Save**. It writes straight back over the same file — no path to retype.
4. The viewport picks it up within a couple of seconds.

The Asset Browser does the same: double-click any image, or right-click it and
choose **Open in Pixel Editor**. **Edit in external app** opens the PNG in your
default image editor instead, with the same live round trip.

## Engine note

Three engine things this project had to route around:

**`Time_*` script bindings are dead.** `Time_GetTime()`, `Time_GetDeltaTime()`
and `Time_GetFrameCount()` return constants in every runtime — `s_DeltaTime`,
`s_TotalTime` and `s_FrameCount` in
`Engine/src/Scripting/ScriptBindings.cpp` are read but never written. Both
scripts keep their own `now` accumulated from the `OnUpdate(dt)` parameter
instead.

**Texture hot-reload never fired** — fixed in the engine on 2026-09-04 while
wiring up the painting flow. Two separate faults: `RenderSystem::LoadTexture`
registered the file watcher with the *unrooted* material path (the editor's CWD
is the exe directory, so it logged `FileWatcher: file does not exist` at load and
never fired again), and the watcher was polled from `RenderSystem::Update()`,
which the editor never calls at all. The poll now runs from
`FlushPendingChanges()` (both runtimes call it every frame) and the swap is
deferred there too, so a live texture is replaced at the documented safe point
with the old one parked in the existing texture graveyard.

**`TextComponent.sdfText` defaults to true**, and on that path `fontSize`,
`wrapWidth` and `textureWidth` do not size the sign — `worldHeight` does, and
the transform scale multiplies it per-axis, so a sign authored at scale
`(7, 3, 1)` gets glyphs stretched 7× wide and 3× tall. The SDF block also hangs
down-right from the entity origin, so `horizontalAlign: 1` centres lines within
the block but does not centre the block on the entity — offset the entity by
half the block width yourself. Both signs here use scale `(1, 1, 1)`, an
explicit `worldHeight`, and a hand-computed x offset. (`Examples/Playground`
authors its sign the stretched way.)

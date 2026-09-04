# Generated Geometry

Every exhibit in this project drives engine code that shipped complete and had
no way to reach it. The implementations were all already in `Engine/src` with
full feature sets behind them. What was missing was the wiring: no ECS
component, no inspector panel, no serializer entry, and no system tick in any
of the three runtimes.

## Scenes

| Scene | Shows |
|---|---|
| `Lab.enjin` | All four generators at once, plus a swarm. Start scene. |
| `Metaballs.enjin` | Seven blobs sharing one scalar field, plus a negative-strength blob that carves a hole in the surface. |
| `CellularAutomata.enjin` | Gosper glider gun in Game of Life, HighLife from a random seed, and a 3D stack meshed with marching cubes. |
| `Projection4D.enjin` | Tesseract, 16-cell and 24-cell, projected stereographically from 4D and tubed into real geometry. |
| `FourierMesh.enjin` | Star, square and heart contours rebuilt from a truncated DFT, with the term count ramping up. |
| `DifficultyAndFaces.enjin` | The dynamic difficulty system and the `Difficulty_*` script bindings. |
| `Dungeons.enjin` | All three dungeon algorithms side by side: drunkard walk, cellular caves, BSP rooms. |
| `TerrainAndScatter.enjin` | fBm terrain baked at play start, with a scatter conforming its instances to the surface. |
| `Scatter.enjin` | The four scatter distributions on flat ground: uniform, Poisson, jittered grid, Voronoi. |
| `RandomBag.enjin` | All five bag modes drawing at once: uniform, weighted, no-replace, deck, Markov. |

## Controls

- `WASD` fly the editor camera
- `Space` pause the orbiting metaball blobs
- In `DifficultyAndFaces`: `1` record a death, `2` a hit, `3` a shot, `R` reset

## What this project exercises

**Four CPU geometry generators.** `MetaballSystem`, `CellularAutomataGeometry`,
`Projection4D` and `FourierMeshDecomposition` between them are about 2,600 lines
of finished implementation that nothing called. They now have components in
`Engine/include/Enjin/ECS/Components/GeneratedGeometry.h`, inspector panels,
serializer entries, and a driver in `ECS::GeneratedGeometrySystem` that ticks in
the editor, the desktop player and the web build.

**One procedural mesh upload path.** Before this, every system that wrote mesh
data at runtime (terrain, terrain2d, jelly, cloth, rope) had its own hardcoded
dirty block in `RenderSystem`, duplicated across the Vulkan and WebGPU paths.
Adding a sixth generator would have meant two more copies. `ProceduralMeshComponent`
replaces that with one loop per path, and any future generator gets uploads for
free by attaching it.

**Two systems that were never ticked.** `DynamicDifficultySystem::Update` and
`FaceCardSystem::Update` were called by nothing, in any runtime, while both
components had inspector panels, serializers and user manual entries.

**The `Difficulty_*` script API.** `docs/SCRIPTING_API.md` documented twelve
functions that existed only as a comment block in `DynamicDifficultySystem.h`.
They are registered now, and `scripts/DifficultyProbe.as` calls them.

**Swarms outside editor play.** `SwarmSystem` ticked in `PlayMode` only, so a
swarm animated in the editor and froze in an exported game and on web.

## Procedural generation coverage

Every procgen component in the engine has a scene here. Four of the five that
already existed never ran on web at all, and the random bag was only ever reset
by the editor, so this project is also the parity proof:

| System | Before | Now |
|---|---|---|
| DungeonGenerator | editor + desktop | all three runtimes |
| TerrainGenerator | editor + desktop | all three runtimes |
| Scatter | editor + desktop | all three runtimes |
| WFC | editor + desktop | all three runtimes |
| RandomBag | editor only | all three runtimes |

## Traps this project hit while being built

All of these were real engine bugs, found by running the demo:

- **The merged geometry pool has no free list.** A mesh regenerating at 30 Hz
  exhausted a 1M-vertex pool in seconds and then silently stopped drawing
  (`MergedGeometryBuffer: vertex space exhausted`). Runtime-generated geometry
  is now excluded from the pool in `RenderSystem::IsPoolEligible`, alongside
  cloth, rope and the other dynamic meshes that were already excluded.

- **Prefab paths had no root to resolve against.** `ScatterSystem` and
  `WFCSystem` passed the component's relative path straight to `LoadPrefab`,
  which opened it against the working directory (the exe folder). Every scatter
  logged `could not load prefab` and placed nothing. `PrefabManager::SetAssetRoot`
  now exists and is set beside the script and audio roots in all three runtimes,
  with the same containment check the script include path uses.

- **Scatter ran before terrain generation.** A scatter with `conformToTerrain`
  sampled a heightmap its terrain had not baked yet, so every point fell "off
  the terrain" and was culled. `placed 0` was the only trace. Terrain now
  generates first in all three runtimes.

- **Scene files need `formatVersion`, not just `version`.** A scene with the
  string `"version": "1.0"` and no `formatVersion` loads, but logs
  `Loading legacy scene file` and runs the migration path. Both keys belong in
  a hand-authored scene.

## Verifying it

```
build/bin/Release/EnjinEditor.exe \
  <abs path>/Examples/GeneratedGeometry/GeneratedGeometry.enjinproject \
  --play --golden <abs path>/_gg_lab --golden-frames 600
```

Absolute paths are required: the editor resolves a launch project against its
own working directory, which is the exe folder, not the shell's.

Note that the editor opens the scene it last had open for a project rather than
the one flagged `isStartScene`, so capturing a specific scene means pointing it
at a manifest that lists only that scene.

## Known cosmetic issue

The generated terrain in `TerrainAndScatter.enjin` renders dark red rather than
the authored material colour. The scatter, the conform and the heightmap are all
correct (162 instances placed, following the surface); only the terrain surface
shading is wrong, and it is unrelated to the generators this project wired.

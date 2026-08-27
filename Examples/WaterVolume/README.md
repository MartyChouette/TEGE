# Water Volume (Lake / Ocean / River / Pond)

The full 3D water system. This scene is a **Lake** with shore foam and a small
island. Open `WaterVolume.enjinproject` and press Play.

`WaterVolumeComponent` is the "real" water in the engine: you attach it to an
entity, and its transform position is the water surface level. `halfExtents`
defines the horizontal area and the depth below the surface.

## The four presets (`Water Type` in the inspector)

- **Lake** — calm, gentle waves, shore foam at the edges (this scene).
- **Ocean** — larger waves, more reflective.
- **River** — flowing.
- **Pond** — small and still.

## Features

- **Shore foam** — `Enable Shore` + `Shore Width` / `Foam Intensity` / `Foam
  Scale`. The white foam band that hugs the shoreline and the island here.
- **Freeze / thaw** — driven at runtime by temperature zones. `Freeze Rate`,
  `Thaw Rate`, `Ice Color`, `Ice Opacity`. Water turns to ice and back.
- **Priority** — overlapping volumes; higher priority wins.

To try the other presets, select the **Lake** entity and change **Water Type**
in the inspector, or drop a new entity and add a **Water Volume** component.

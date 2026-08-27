# Water Showcase

A scene that puts all five 3D water styles side by side so you can compare them
directly. Open `WaterShowcase.enjinproject` in the editor and press Play, or
launch it with the player.

## Layout

Five pools in a row, left to right, each a `Water3D` component set to a
different `Style`. A bright cube floats above every pool so the reflective
styles have something to mirror.

| Pool | Style | What it does |
|---|---|---|
| 1 (red) | **Flat** | Solid colour surface. Very retro. |
| 2 (orange) | **Animated** | Scrolls its surface UVs. SNES-style flowing water. |
| 3 (yellow) | **VertexWave** | Displaces the mesh into rolling waves. PS1/N64. |
| 4 (green) | **Reflective** | Mirrors the scene above the surface. PS2/GameCube glassy water. |
| 5 (blue) | **Refractive** | Reflective, plus a fresnel-split refraction: looking straight down shows the wobbling refracted depth with an animated caustic shimmer, grazing angles keep the reflection. Late-PS2 water. |

The floating cubes reflect in pools 4 and 5 only; the first three pools leave
the water plain, which is the point of the comparison.

## What to look for

- **Reflective vs Refractive:** pool 4 is a clean mirror; pool 5 adds the
  animated caustic shimmer and reads the reflection/refraction blend from the
  view angle (Fresnel Power on the component controls where that split sits).
- **Reflection Strength / Fresnel Power** live in the Water3D inspector and only
  appear for the Reflective and Refractive styles.

The reflections are hand-crafted mirror geometry, not screen-space reflections —
deterministic and era-appropriate, the same look every frame.

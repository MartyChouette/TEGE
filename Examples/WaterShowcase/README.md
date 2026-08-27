# Water3D — the five styles

`Water3DComponent` is the lightweight water plane. It has five styles, one per
pool in this scene (left to right). Open `WaterShowcase.enjinproject` and press
**Play** — several styles only differ once they animate.

| Pool | Style | What it does | Opacity |
|---|---|---|---|
| 1 | **Flat** | Solid translucent surface, no motion. Very retro. | see-through |
| 2 | **Animated** | Scrolls its surface UVs (assign a texture to see it flow). | see-through |
| 3 | **VertexWave** | Displaces the mesh into visible rolling waves. | see-through |
| 4 | **Reflective** | Opaque mirror — the floating cube reflects in it. | mirror |
| 5 | **Refractive** | Mirror plus an animated caustic refraction shimmer. | mirror |

## Notes

- **See-through vs mirror.** The first three pools are translucent (drag their
  `Opacity` in the inspector and you'll see the bottom through them). The two
  mirror styles are near-opaque on purpose — a reflection reads best on a solid
  surface, and keeping them opaque avoids the reflected geometry poking through
  the waves.
- **Opacity now works.** Water3D used to be forced opaque; the `Opacity` slider
  is live now for every style.
- Water3D is the *lightweight* option. For shore foam, freeze, and the
  Lake/Ocean/River/Pond presets, use **WaterVolume** (see `../WaterVolume`).

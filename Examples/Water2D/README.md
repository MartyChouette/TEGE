# 2D Water

For side-scrollers and top-down 2D games. Open `Water2D.enjinproject`.

2D water is a **scene setting**, not a component: `Water2DConfig` on the scene
draws a full-screen overlay based on the active orthographic camera. Everything
below `waterLineY` gets tinted and darkened with depth; a wavy foam line and
caustic shimmer ride the surface.

Set it in **Scene Settings → 2D Water** (or the `water2d` block in the scene
file):

- `waterLineY` — world Y of the surface.
- `surfaceColor` / `deepColor` — tint just under the line vs far below.
- `opacity` + `depthFalloff` — how strong the tint gets and over how many units
  it reaches full depth colour.
- `waveAmplitude` / `waveLength` / `waveSpeed` — the surface line.
- `foamColor` / `foamWidth` — the foam band at the line.
- `causticStrength` — 0 = flat tint, 1 = strong shimmer.

Press Play to see the surface animate.

**Note:** this scene shows the water effect over a sky/background. Placing your
own 2D sprite art (terrain, characters, fish) below the line is the normal
workflow — they sit in the tinted zone and read as underwater.

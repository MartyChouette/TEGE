# Water FX

Cheap, good-looking, **authored** water motion (F1-F5): a curved waterfall,
base foam, rising mist, a ground stream, and rain runoff - all looped
animation riding the material UV primitive, zero runtime simulation.
Open `WaterFX.enjinproject` and press Play.

## What each piece is

- **Waterfall** - a curved sheet mesh with a tileable water texture scrolling
  downward (`uvScrollSpeed`), translucent, double-sided. The classic
  N64-era trick: the texture moves, the mesh never does.
- **Base foam** - a flat quad playing a 4-frame **flipbook**
  (`flipbookCols/Rows/Fps`) of churning blobs.
- **Mist** - a particle emitter with a soft droplet texture drifting upward.
- **Stream** - the same scroll trick flat on the ground.
- **Rain runoff (press R)** - scrolling strips under the roof edge that only
  show while it rains; the demo script toggles rain and their visibility.

## Making your own

Any material has the knobs: Material > UV Animation in the inspector
(scroll speed, flipbook grid + fps). Author a tileable texture, put it on
any mesh, set a scroll speed - waterfalls, conveyor belts, lava, clouds.
The textures here are tiny generated PNGs - replace them with painted ones.

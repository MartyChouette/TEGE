# Wind Particles

Particles that drift and gust with the **scene wind** — snow, dust, pollen,
embers, blowing leaves. This scene blows a field of motes across the ground on
the world's wind. Open `WindParticles.enjinproject` and press Play.

## How it works

Any `ParticleEmitter` now has **Use Scene Wind**. Turn it on and the world's wind
(the `WindSystem` — the same wind that sways vegetation and drives water) pushes
the particles, so they drift and gust with everything else instead of moving in a
straight line. **Wind Influence** controls how hard the wind pushes them.

The scene wind direction/strength comes from the Weather/Wind settings (default is
a light breeze along +X); scripts can change it live with `Weather_SetWind`.

## Robust settings (on the emitter)

Emission rate, lifetime (+ variance), start speed, size over life, color over life,
alpha over life, gravity, drag, emitter shape + radius (spawn area), max particles,
rotation, simulation space — plus **Use Scene Wind** / **Wind Influence**. Tune
these for anything from slow floating pollen to fast-driving snow.

Recipes:
- **Snow:** white, small, slight downward gravity, moderate wind, high emission.
- **Dust / pollen:** warm tint, near-zero gravity, low wind, long lifetime.
- **Embers:** orange→dark color-over-life, upward gravity, gusty wind.

## Art asset

Set the emitter's **Texture** to your own sprite (this scene uses a small orange
leaf) and the particles render that image, tinted by the per-particle colour. With
no texture they fall back to a built-in soft dot (great for snow/dust). One texture
per pass covers the common single-emitter case; many distinct textures at once would
need an atlas (like sprites).

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

## Known limitation

The particle renderer currently draws soft round motes (a built-in soft-dot shape)
and does not yet sample a custom **texture** per particle — so a bespoke leaf/art
sprite won't show its own image yet (the shape is the soft dot; use the color
settings to tint the look). Wiring per-particle texture + colour into the particle
shader is a follow-up (same fix already done for the fluid renderer). The wind
drift, and all the settings above, work today.

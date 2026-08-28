# Buoyancy — things float in water

Drop a dynamic rigidbody into a water volume and it floats. This scene drops four
crates into a lake; they fall, hit the water, and bob at the surface. Open
`Buoyancy.enjinproject` and press Play.

Buoyancy is **on by default** for every water surface (WaterVolume and Water3D),
so "objects float" works out of the box — no extra component. It's driven by the
physics backend each step: any dynamic body below the water surface and inside its
footprint gets pushed up, with water drag so it settles instead of bobbing forever.

## Making something float

1. Add a **Water Volume** (or Water3D) — buoyancy is already enabled on it.
2. Give the object a **Rigidbody** (Dynamic) and a **Collider**.
3. Play — it floats.

## Tuning (on the water component)

- **Enable Buoyancy** — toggle it off for a purely visual water.
- **Buoyancy Strength** — 1 is neutral; higher floats things higher. ~1.6 sits
  low in the water (dense crate), ~3 rides high like a cork.
- **Buoyancy Drag** — water resistance. Higher settles faster and kills sideways
  drift; lower lets things slosh and bob.

Only **Dynamic** rigidbodies float; static/kinematic bodies are ignored. Overlapping
water volumes resolve by `Priority`.

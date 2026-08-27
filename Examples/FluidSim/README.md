# Fluid Simulation

A grid-based (stable-fluids) simulation you can dial in for very different looks
and budgets. This scene runs three plumes: a 2D smoke, a **3D** steam, and a 2D
fire. Open `FluidSim.enjinproject` and press Play.

Add one with **Add Component → Fluid Volume**. The volume emits from its source
each frame; buoyant types rise and billow, dense types pool and spread.

## Tuning it for your game

Everything is exposed in the Fluid Volume inspector. The knobs that matter for
fitting it to a scene:

| Goal | Knob |
|------|------|
| Pick the look | **Fluid Type** — Water, Lava, Gas, Smoke, Steam (each sets sensible defaults) |
| 2D vs 3D | **Dimension** — 2D is a flat billboard field (cheap); 3D is a volumetric grid |
| **LOD / resolution** | **Grid Size** — 8–128 in 2D, 8–48 in 3D. Lower = coarser + much cheaper; raise it for dense, high-pixel scenes |
| **Quality vs performance** | **Solver Iterations** — 1–40. Fewer = faster and looser; more = tighter, more accurate flow |
| How much renders | **Opacity**, **Density Threshold** (cells below this are skipped — raise it to draw fewer cells), **Render Enabled** |
| Colour | **Color** — the fluid's tint (now honoured per cell) |
| Emission | **Source Density / Radius / Vel Scale** — how much fluid, how wide, how fast it shoots |
| Motion feel | **Buoyancy** (upward push), **Viscosity**, **Diffusion**, **Dissipation** / **Vel Dissipation** (how fast it fades) |

Rules of thumb:
- **Mobile / lots of scenes:** 2D, Grid Size 48–64, Solver Iterations 8–12, higher
  Density Threshold.
- **Hero effect, high-end:** 3D, Grid Size 40–48, Solver Iterations 20–30.
- **Wispy smoke:** low Source Density, high Dissipation. **Thick lava:** high
  Viscosity, low Dissipation, opacity 1.

`Priority` breaks ties between overlapping volumes; `Half Extents` sets the area.

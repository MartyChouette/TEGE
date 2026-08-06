# Procedural Effects Art Direction
# TEGE Default Scene Visual Identity

**Scope:** Geometry, color, motion, and variation direction for all procedural
effect systems. These defaults are what every user sees on first launch. The
standard should be "intentional stylized nature" rather than "programmer test
scene." No textures required. No alpha-cutout dependence. All recommendations
stay within the current GPU-instanced, hash-derived-variation constraint.

---

## Shared Design Principles

**Cross-system palette anchor.** Every system draws from the same four base hues
so a default scene reads as a single art decision rather than six separate ones.

| Role        | sRGB (0-255)       | Linear (0-1)             | Used by                      |
|-------------|---------------------|--------------------------|------------------------------|
| Deep ground | (28, 45, 22)        | (0.11, 0.18, 0.09)       | Grass base, shrub base, tree trunk |
| Mid green   | (62, 95, 41)        | (0.24, 0.37, 0.16)       | Grass tip, shrub mid          |
| Light green | (108, 148, 68)      | (0.42, 0.58, 0.27)       | Shrub tip, canopy base        |
| Sky warm    | (190, 215, 240)     | (0.75, 0.84, 0.94)       | Horizon, water shallow, rain  |

**Variation budget.** Per-instance variation is hash-derived (no extra data
cost). Allow hue shift +/-8 degrees HSV, saturation +/-10%, value +/-12%.
Never let any single instance look fully saturated or fully desaturated. The
goal is the same species, different specimens.

**Mode invariance.** In flat/cel/retro mode the geometry silhouette carries all
legibility. In PBR/path-traced mode the color ramps and normal-implied shading
do the rest. Do not rely on alpha transparency for shape definition anywhere.

---

## 1. Grass

**Current state.** 7 verts, straight tapered blade, base-to-tip linear lerp,
wind with correct height-squared rolloff, player bend. Works. Reads flat.

### Silhouette (target: 9 verts, +2 from current)

Add one mid-blade bend vertex pair to introduce a natural forward arc. The blade
does not grow straight up in nature; it leans slightly in its planted direction.

```
Vertex layout (9 verts):

      8 (tip)
     /
    6   7
   / \ /
  4       5
 / \     / \
2           3    <- mid-low, unchanged
|           |
0           1    <- base, unchanged

New verts 6,7 shift X by +0.08 world units (a slight forward lean toward +Z
in local space). Bend is baked into the mesh; wind sway on top of it reads
as a living arc rather than a metronome stick.
```

Full vertex table (Y normalized 0-1, X at half-width 0.5):

| Vert | X     | Y    | Z    | U    | V    |
|------|-------|------|------|------|------|
| 0    | -0.50 | 0.00 | 0.00 | 0.00 | 0.00 |
| 1    |  0.50 | 0.00 | 0.00 | 1.00 | 0.00 |
| 2    | -0.35 | 0.33 | 0.00 | 0.15 | 0.33 |
| 3    |  0.35 | 0.33 | 0.00 | 0.85 | 0.33 |
| 4    | -0.20 | 0.60 | 0.04 | 0.30 | 0.60 |
| 5    |  0.20 | 0.60 | 0.04 | 0.70 | 0.60 |
| 6    | -0.10 | 0.80 | 0.08 | 0.38 | 0.80 |
| 7    |  0.10 | 0.80 | 0.08 | 0.62 | 0.80 |
| 8    |  0.02 | 1.00 | 0.10 | 0.50 | 1.00 |

Indices (7 tris): 0-1-2, 2-1-3, 2-3-4, 4-3-5, 4-5-6, 6-5-7, 6-7-8.

### Color ramp

- Base: deep ground (0.11, 0.18, 0.09)
- Tip: mid green (0.24, 0.37, 0.16)
- Lerp curve: use `heightFraction^0.7` instead of linear so the dark ground
  color holds for the lower 40% of the blade and color transitions faster near
  the tip. Reads better at distance.

Per-instance hue jitter: derive from `hash(instanceID * 11u)`, shift base hue
+/-7 degrees. Apply the same shift to both base and tip to keep the gradient
coherent per blade.

### Motion

Current wind is good. Two refinements:

1. Clamp `windOffset` magnitude to `0.35 * bladeHeight` regardless of
   `windSwayStrength`. Prevents absurd bending at high wind that breaks the
   silhouette.
2. Add a subtle tilt variation: `rotatedPos.z += hash(instanceID*5u) * 0.06 - 0.03`.
   Blades lean slightly in random directions at rest. Combine with the baked arc
   above; this reads as "different plants" not "copies."

### Do not do

- No alpha cutout for blade edges. The 9-vert silhouette is enough.
- Do not make tip color brighter than (0.50, 0.68, 0.32). Bright tips read as
  glow in path-traced mode.
- Do not increase blade count beyond 5000 default without a LOD fade strategy.

---

## 2. Shrub

**Current state.** 3 crossed vertical quads (12 verts, star pattern), uniform
rectangular shape, base-to-tip lerp. Reads as a flat green wall at most angles.
The geometry cross-section gives no overhead silhouette variety.

### Silhouette (target: 21 verts, +9 from current)

Replace the 3 uniform rectangles with 3 tapered dome quads. Each quad narrows
at the top so the intersection creates a lumpy crown silhouette rather than a
flat-topped box.

Per-quad (repeat for all 3 at 0, 60, 120 degrees):

| Vert | local X offset | Y    | note                 |
|------|----------------|------|----------------------|
| 0    | -0.50          | 0.00 | base left            |
| 1    |  0.50          | 0.00 | base right           |
| 2    | -0.42          | 0.35 | lower mid left       |
| 3    |  0.42          | 0.35 | lower mid right      |
| 4    | -0.28          | 0.65 | upper mid left       |
| 5    |  0.28          | 0.65 | upper mid right      |
| 6    |  0.00          | 1.00 | tip (collapsed)      |

Triangles per quad: 0-1-2, 2-1-3, 2-3-4, 4-3-5, 4-5-6.
Total: 21 verts (7 per quad x 3 quads), 15 tris.

Apply the same rotational spread as today. The tapered quads produce a distinct
berry-bush or fern silhouette from any angle.

### Color ramp

- Base: deep ground (0.11, 0.18, 0.09) -- same as grass base for ground unity
- Mid (0.35): mid green (0.24, 0.37, 0.16)
- Tip: light green (0.42, 0.58, 0.27)
- Lerp with `heightFraction^0.6` to push the darker tone toward the ground.

Per-instance variation: `hash(instanceID * 17u)` drives a +/-9 degree hue
shift plus +/-8% value shift. Some specimens should read slightly more yellow-
green, some more blue-green.

### Motion

Shrubs sway less than grass but move as a mass. Recommend:
- Wind phase: `dot(worldPos.xz, vec2(0.08, 0.12)) + windTime * 1.4`
- Displacement: `windDir * sin(windPhase) * heightFraction * windSway * 0.6`
- No secondary high-freq wave. Shrubs move slower and more deliberately.

### Do not do

- Do not make all 3 quads the same height per instance. Add
  `hash(instanceID*13u) * 0.25 - 0.05` to the Y scale of each individual quad
  within the instance (derive from `instanceID * 13u + quadIndex`). This costs
  nothing and breaks the triple-flat-top silhouette.

---

## 3. Tree

**Current state.** 2 crossed trunk quads (tapered, good) + 3 crossed canopy
quads (uniform rectangles). The canopy at 3 quads reads as a hexagonal slab.
Seasonal color exists and works.

### Silhouette (target: 32 verts, +12 from current)

Keep the trunk as-is (8 verts, good taper). Upgrade canopy from 3 flat quads
to 3 tapered diamond quads. Each diamond is widest at 35% height and tapers
at both top and bottom.

Per canopy quad (7 verts each, 3 quads = 21 canopy verts total, +12 from current 12):

| Vert | local X | local Y (relative to canopyBase) | note         |
|------|---------|----------------------------------|--------------|
| 0    |  0.00   | 0.00                             | bottom point |
| 1    | -0.35   | 0.25                             | lower left   |
| 2    |  0.35   | 0.25                             | lower right  |
| 3    | -0.50   | 0.50                             | widest left  |
| 4    |  0.50   | 0.50                             | widest right |
| 5    | -0.20   | 0.85                             | upper left   |
| 6    |  0.20   | 0.85                             | upper right  |
| 7    |  0.00   | 1.00                             | top point    |

(8 verts per quad but vert 7 = tip; 7 unique positions. Triangles: 1-2-0,
1-3-2 is wrong -- use: 0-1-2, 1-3-4, 1-4-2, 3-5-6, 3-6-4, 5-7-6.)
This diamond cross-section reads from any angle as a distinct conifer-style
or broadleaf crown depending on the instance scale.

### Color ramp

Trunk:
- Base (Y=0): (0.22, 0.15, 0.09) -- warm bark brown
- Top (Y=1): (0.30, 0.22, 0.13) -- slightly lighter bark

Canopy (summer defaults, seasonal overrides unchanged):
- Bottom point: deep ground (0.11, 0.18, 0.09) -- shadow inside canopy
- Mid: light green (0.42, 0.58, 0.27)
- Tip: (0.52, 0.68, 0.32) -- sun-catching tip, slightly brighter

In tree.frag, the canopy ramp should use the diamond vert V coordinate:
`canopyColor = mix(canopyShadow, canopyTip, heightFraction^0.5)`. The
square-root curve gives more visible mid-green surface area.

Per-instance variation: `hash(instanceID * 23u)` for a +/-12% size scalar
(applied at the volume-placement level, already exists in the hash-based
`sizeVar`). Additionally shift canopy hue +/-10 degrees so a grove reads
as multiple species rather than clones.

### Motion

Keep the existing wind sway. One addition: canopy quads should lag the trunk.
Since all motion is in the vertex shader with no per-quad state, approximate
this by multiplying `windOffset` by `0.85` for verts with `V >= 0.5` (canopy).
The slight reduction creates a visual mass-lag that reads as weight.

### Do not do

- Do not increase tree density above 100 per volume without testing the
  shadow and collider generation paths (GenerateColliders spawns one entity
  per tree trunk).
- Do not let canopy tip color exceed (0.60, 0.75, 0.38) in any season. Bright
  tips become halos in path-traced bounce lighting.

---

## 4. Water

**Current state.** CPU sine-wave displaced grid, shallow/deep color lerp,
edge foam encoded in vertex alpha. Regenerates the mesh CPU-side every frame.

### Silhouette

Water is a flat plane at most resolutions. No vertex budget increase needed.
The visual upgrade is entirely in color and normal-implied shading.

Add a second sine pass to normal generation with perpendicular direction and
0.4x amplitude. Currently the finite-difference normals are single-direction
sine normals which produce parallel ridges. Two crossed passes produce
checkerboard ripple normals that read as water under PBR specular.

Code change is in `Water3D::BuildEntityMesh` normal computation:
```
// Current: one directional pair
// Add: cross-direction pair at 70% amplitude
f32 hL2 = GetWaveHeight2(positions[i].x, positions[i].z - eps);
f32 hR2 = GetWaveHeight2(positions[i].x, positions[i].z + eps);
// GetWaveHeight2 = wave1 only, perpendicular phase offset pi/2
// Blend normals: normal = normalize(n1 + n2)
```

Delegate the math implementation to technical-artist.

### Color ramp

- Shallow: (0.15, 0.42, 0.55) -- teal, not pure cyan
- Deep: (0.04, 0.12, 0.28) -- dark navy
- Lerp by vertex depth (edge distance alpha, already encoded)
- Shore foam: (0.82, 0.88, 0.90) -- off-white, not pure white

These values pull the water into the shared palette. The teal shallow reads
next to the green ground palette without competing.

### Motion

Wave character: primary wave at `frequency * 1.0` in X, secondary at
`frequency * 0.7` in Z with 1.3x speed (already exists). Third wave adds
diagonal movement. This is in place. The motion is good.

Foam timing: foam should pulse. Multiply `edgeDist` by
`0.6 + 0.4 * sin(waveTime * 1.5 + worldPos.x * 0.1)` before writing to
vertex alpha. The foam breathes in and out rather than sitting static at
the shore.

### Do not do

- Do not change tileSize below 0.5. Below that the CPU mesh exceeds reasonable
  vert counts and frame time climbs.
- Do not set `shallowColor` equal across R/G/B channels (grey water). Always
  keep B > G > R for water to read as water in retro palette mode.

---

## 5. Sky

**Current state.** 3-color gradient cubemap (topColor, horizonColor, bottomColor),
64x64 face, hard-baked at scene load. Fallback values: top = (0.1, 0.2, 0.6),
horizon = (0.5, 0.7, 0.9), bottom = (0.4, 0.3, 0.2). The bottom and horizon
values are near-identical perceptually, producing a flat dome.

### Color direction

Change fallback values in all six `CreateProcedural` call sites in Skybox.cpp:

| Layer    | Current linear             | Direction linear           |
|----------|---------------------------|----------------------------|
| Top      | (0.10, 0.20, 0.60)        | (0.05, 0.12, 0.52)         |
| Horizon  | (0.50, 0.70, 0.90)        | (0.55, 0.72, 0.88)         |
| Bottom   | (0.40, 0.30, 0.20)        | (0.18, 0.22, 0.28)         |

The changes: top is slightly deeper blue (more visible saturation gradient),
horizon is preserved but just slightly warmer, bottom shifts from muddy brown
to dark blue-grey. This gives the ground plane a more convincing atmospheric
look and keeps the sky in the same blue-teal family as the water system.

The gradient blend zone should widen slightly. Current code uses 0.55/0.45
thresholds (10% band). Change to 0.65/0.35 (30% band) for a softer atmospheric
gradient that reads better in path-traced GI bounce.

### Do not do

- Do not change face resolution above 64x64. At 64 the gradient is perfectly
  smooth. Larger wastes VRAM for no visual gain.

---

## 6. Weather

**Current state.** Flat square quads for both rain and snow. Rain reads as
falling rectangles. Snow reads as white squares.

### Silhouette

Rain: replace the square quad with a 4-vert stretched quad, 1:6 aspect ratio
(width 0.15, height 0.9 in local space). This costs zero extra verts and
makes rain streaks read as streaks.

Snow: keep square but rotate to face camera in the vertex shader (billboard).
Current implementation does not billboard. Snow flakes that are edge-on are
invisible. Billboard around Y-axis only (not full spherical) so flakes still
fall straight down visually.

Implementation of both: delegate to technical-artist.

### Color

Rain: (0.68, 0.74, 0.82) at 70% opacity -- blue-grey, not white. White rain
looks like code artifacts. Specular highlight on the streak quad using the
sky warm color at 30% as a rim.

Snow: (0.90, 0.92, 0.95) at 85% opacity. Slight blue tint, not pure white,
so it reads against bright sky backgrounds.

### Motion

Rain: current billboard quads should have a slight velocity stretch. If the
renderer supports per-instance velocity in the vertex shader, multiply the
Y extent of the quad by `1.0 + fallSpeed * 0.3`. At low speeds (snow-like
settings) quads are square; at high speed they become natural streaks.

### Do not do

- Do not use alpha-cutout for weather particle shapes. The geometry IS the
  shape.

---

## 7. Fire / Elemental

**Current state.** Point particles from ElementalSystem feeding transient
point lights. No geometry shape beyond the light contribution.

### Direction

The current system creates fire as a lighting effect rather than a visible
geometry effect. This is correct for path-traced mode. For retro/flat modes
where the light contribution does not show, fire becomes invisible.

Recommendation: add a companion geometry pass (6-vert flame quad, 2 tris) that
sits at the emitter and uses the existing wind uniform to flicker. Color this
with the fire ramp below and it reads in both rendering modes.

Color ramp (base to tip, V 0 to 1):
- V=0.0: (0.65, 0.12, 0.02) -- deep red-orange, near-black core
- V=0.35: (0.90, 0.45, 0.05) -- orange
- V=0.70: (1.00, 0.82, 0.20) -- yellow
- V=1.00: (1.00, 0.98, 0.85) -- pale yellow-white tip

Flame quad uses `heightFraction^1.5` for the lerp curve so the red base is
narrow and the orange body is wide. This is the opposite curve to grass and
shrub, which gives fire a visually distinct gradient character.

Delegate geometry implementation to technical-artist.

### Do not do

- Do not set fire emissive strength so high that it clips in PBR mode.
  Cap emissiveStrength at 2.0 for the default flame. Users can increase it.

---

## 8. What No System Should Do

1. Use alpha-cutout to define the visual silhouette of a geometry primitive.
   If the shape needs defining, add verts.
2. Output tip/emissive colors brighter than 1.2 in linear space for default
   presets. This breaks CRT and bloom modes.
3. Use pure white (1,1,1) or pure black (0,0,0) anywhere in default color
   ramps. Both extremes lose information in retro palette and path-traced modes.
4. Use a hue that does not appear elsewhere in the scene palette. The four
   anchor hues above cover everything. New systems should pick from them or
   derive adjacent hues, not introduce unrelated color families.
5. Add per-instance GPU data beyond the existing push-constant layout. All
   variation must derive from `gl_InstanceIndex` hash. Any exception requires
   a pipeline architecture change coordinated with technical-artist.

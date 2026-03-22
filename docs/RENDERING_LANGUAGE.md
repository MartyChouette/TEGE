# Tege Rendering Language

A material and effect taxonomy for composable, aesthetics-first rendering. Each technique is designed to be implementable as a shader building block that artists can mix and match without writing GLSL.

---

## Core Principles

| Principle | Description |
|-----------|-------------|
| **Perceptual Priority** | Optimize for what the eye notices, not physical accuracy |
| **Layered Illusion** | Stack cheap effects to approximate expensive ones |
| **Event Optics** | Visual responses tied to gameplay events, not just physics |
| **Quantized Response** | Stepped/discrete transitions instead of smooth (retro, stylized) |
| **Screen Reuse** | Derive effects from existing screen buffers (depth, normal, color) |
| **Meaningful Failure** | When an effect can't run, degrade to something that still looks intentional |

---

## Material Taxonomy

| Category | Description |
|----------|-------------|
| **Surface-Bound** | Standard opaque materials (PBR, stylized, flat) |
| **Transmissive** | Glass, water, ice — light passes through |
| **Participating Volume** | Fog, smoke, clouds — light scatters within |
| **View-Reactive** | Materials that change based on view angle (iridescence, fresnel) |
| **Event-Driven** | Materials that respond to gameplay (damage flash, power-up glow) |
| **Layer-Composed** | Multiple material layers blended (snow on rock, moss on brick) |

---

## Implementation Modes

How each effect derives its data:

| Mode | Source |
|------|--------|
| **Screen-Derived** | Sample from the rendered scene color buffer |
| **Depth-Derived** | Use depth buffer for thickness, distance, intersection |
| **Normal-Derived** | Use surface normals for rim, fresnel, curvature |
| **Projected** | Project textures/patterns onto surfaces from world space |
| **Layered** | Composite multiple passes or textures |
| **Event-Driven** | Triggered by gameplay events (timers, health, triggers) |
| **Quantized** | Stepped values, palette-locked, dithered |

---

## Effect Catalog

### Transmission

| Effect | Description | Mode |
|--------|-------------|------|
| **Screen-Bent Transmission** | Refract the background through a surface using screen-space UV distortion | Screen-Derived |
| **Ghosted Transmission** | Semi-transparent overlay with desaturated/blurred background | Screen-Derived |
| **Quantized Blur Transmission** | Stepped blur levels (not smooth gaussian — discrete steps) | Quantized |
| **Depth Veil** | Fade-to-color based on depth behind the surface | Depth-Derived |

### Subsurface / Soft Materials

| Effect | Description | Mode |
|--------|-------------|------|
| **Edge Bloom** | Glow at silhouette edges (wax, skin, leaves) | Normal-Derived |
| **Thickness Tint** | Color shift based on mesh thickness (ears, fingers, petals) | Depth-Derived |
| **Pulsed Subsurface** | Event-driven subsurface intensity (heartbeat, damage, magic) | Event-Driven |

### Atmosphere

| Effect | Description | Mode |
|--------|-------------|------|
| **Staged Atmosphere** | Discrete fog layers at fixed distances (foreground/mid/far) | Depth-Derived |
| **Plane-Fog Stack** | Multiple horizontal fog planes at different heights/densities | Layered |
| **Dithered Volume** | Volumetric fog approximated with ordered dithering | Quantized |
| **Settling Haze** | Fog that accumulates over time in low areas | Event-Driven |

### Light Projection

| Effect | Description | Mode |
|--------|-------------|------|
| **Projected Caustic Field** | Animated caustic patterns projected onto surfaces | Projected |
| **Authored Light Script** | Hand-authored light animations (flickering torch, pulsing crystal) | Event-Driven |
| **Surface-Wake Caustics** | Caustic patterns generated from water surface displacement | Projected |

### Reflection

| Effect | Description | Mode |
|--------|-------------|------|
| **Broken Reflection** | Fragmented/distorted reflections (cracked mirror, rippled water) | Screen-Derived |
| **Jittered Reflection** | Noisy/shimmering reflections (wet pavement, brushed metal) | Screen-Derived |
| **Echo Reflection** | Delayed/ghosted reflections that trail behind movement | Screen-Derived |
| **Cubic Impression Reflection** | Low-res cubemap reflection with artistic color grading | Projected |

### Water / Liquid

| Effect | Description | Mode |
|--------|-------------|------|
| **Layered Water Body** | Surface + depth layers with independent movement | Layered |
| **Surface Drift** | Animated UV flow on water surface (foam, ripples) | Projected |
| **Depth Tint** | Color absorption based on water depth (shallow=clear, deep=dark) | Depth-Derived |
| **Contact Foam** | White foam where objects intersect the water surface | Depth-Derived |

### Fibers / Hair / Cloth

| Effect | Description | Mode |
|--------|-------------|------|
| **Clump Shading** | Grouped strand shading (hair clumps, fur tufts) | Normal-Derived |
| **Band Sheen** | Anisotropic highlight bands along fiber direction | Normal-Derived |
| **Fuzz Rim** | Soft rim lighting for fuzzy/velvet surfaces | Normal-Derived |
| **Fiber Breakup** | Noise-driven edge dissolution for cloth/fabric silhouettes | Screen-Derived |

### Iridescence

| Effect | Description | Mode |
|--------|-------------|------|
| **Angle-Shift Color** | Color changes based on view angle (beetle shell, oil slick) | Normal-Derived |
| **Spectral Ramp** | Map view angle to a custom color gradient | Normal-Derived |
| **Grazing Flare** | Bright spectral flare at extreme grazing angles | Normal-Derived |

### Snow / Ice

| Effect | Description | Mode |
|--------|-------------|------|
| **Event Sparkle** | Glitter points that appear based on view/light angle changes | Event-Driven |
| **Blue-Body Ice** | Deep blue absorption tint with surface frost layer | Layered |
| **Crack-Light Ice** | Light bleeding through ice crack patterns | Projected |

### Fire / Heat

| Effect | Description | Mode |
|--------|-------------|------|
| **Shape-Driven Flame** | Flame shape from animated noise fields (not particles) | Projected |
| **Layered Ember Body** | Hot core + cooling outer layer with color gradient | Layered |
| **Heat Veil** | Screen-space distortion above hot surfaces | Screen-Derived |

---

## Naming Convention

```
[Domain]_[Behavior]_[Mode]
```

| Example | Domain | Behavior | Mode |
|---------|--------|----------|------|
| `Glass_Ghosted_Screen` | Glass | Ghosted Transmission | Screen-Derived |
| `Fog_Staged_Depth` | Fog | Staged Atmosphere | Depth-Derived |
| `Water_Layered_Multi` | Water | Layered Water Body | Multi-pass |
| `Snow_Sparkle_Event` | Snow | Event Sparkle | Event-Driven |
| `Hair_BandSheen_Normal` | Hair | Band Sheen | Normal-Derived |
| `Ice_CrackLight_Projected` | Ice | Crack-Light | Projected |
| `Fire_HeatVeil_Screen` | Fire | Heat Veil | Screen-Derived |

---

## Implementation Strategy

Each effect in this catalog maps to a **Material Expression Node** in the shader graph system. The implementation path:

1. **Phase 1 — Screen-Derived effects** (cheapest, most reusable): Heat Veil, Broken Reflection, Ghosted Transmission, Fiber Breakup, Contact Foam
2. **Phase 2 — Normal/Depth-Derived effects**: Edge Bloom, Thickness Tint, Staged Atmosphere, Depth Tint, Angle-Shift Color, Band Sheen, Fuzz Rim
3. **Phase 3 — Projected effects**: Projected Caustics, Surface-Wake Caustics, Crack-Light Ice, Shape-Driven Flame, Surface Drift
4. **Phase 4 — Event-Driven effects**: Pulsed Subsurface, Authored Light Script, Event Sparkle, Settling Haze
5. **Phase 5 — Quantized/Layered composites**: Dithered Volume, Plane-Fog Stack, Layered Water Body, Blue-Body Ice, Layered Ember Body

Each phase builds on the previous — Screen-Derived effects teach the infrastructure for sampling scene buffers, which Depth/Normal effects extend, which Projected effects build on, etc.

---

## Relationship to Existing Systems

| Existing System | Rendering Language Connection |
|----------------|-------------------------------|
| Post-Processing (PostProcessing.cpp) | Screen-Derived effects run as PP passes |
| Material SSBO (binding 2) | Material flags select which expressions are active per-material |
| Retro Effects (8 art styles) | Quantized mode effects integrate with existing retro pipeline |
| Water System (Gerstner waves) | Water/Liquid effects layer on top of existing wave simulation |
| Weather System (rain, snow, fog) | Atmosphere effects compose with existing weather particles |
| Shader Graph (54 node types) | Each effect becomes a new shader graph node type |

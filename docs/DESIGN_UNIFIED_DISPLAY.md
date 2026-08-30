# Unified Display: one system for text, vector, and raster graphics — in world and on screen

Marty, 2026-08-30: "a unified in-world / out-world vector motion graphic and
raster motion graphic UI and in-world text and graphics" — connected to the
UI layout task and the Ink_Ribbon world-text spec.

## What exists today (survey, source-verified)

Seven ways to put text on screen, each its own pipeline:

| Path | Mechanism | Space | Weakness |
|---|---|---|---|
| TextComponent | TextRasterizer -> RGBA texture -> quad (auto-quad since b6129652) | world | re-rasterizes + swaps a texture per change; fixed texture res = blurry when scaled |
| UICanvas labels | ImGui foreground draw list | screen (+ world-anchored) | ImGui font stack, 8 fixed widgets, separate styling |
| SubtitleSystem | own overlay | screen | separate |
| GameMenus | ImGui background list | screen | separate |
| HUD (retired) | migrated to UICanvas | — | — |
| Editor text | ImGui | screen | fine (editor-only) |
| Console/etc | ImGui | screen | fine (editor-only) |

Graphics: Sprite2D/AnimatedSprite2D (raster + flipbook), F1 material UV
anim/flipbook, VectorDrawing panel (authoring), SWFLoader (vector art +
timelines - the unannounced Flash capability), FlashTimeline panel,
ParticleGraph. SDFRenderer.h is 3D geometry distance fields, NOT font SDF -
there is no font-SDF pipeline yet.

## The unifying idea: the display object

Flash got this right and TEGE already has Flash DNA (SWFLoader, timelines,
vector authoring). One retained tree of DISPLAY OBJECTS:

- **Shape** - vector paths (fills, strokes) - from VectorDrawing or SWF
- **Bitmap** - raster image / flipbook frame
- **TextRun** - glyphs from ONE shared font atlas
- **Group** - children + transform + timeline track

One renderer walks the tree and composites. The SAME tree mounts two ways:

- **Screen mount** ("out-world"): drawn as the UI layer - this IS the UI
  system; layout containers (anchors, stacks, grids) are just Group nodes
  with layout rules. Task #12's dev/creative layouts become two authoring
  views over the same tree.
- **World mount** ("in-world"): the tree renders into the scene at an
  entity's transform - either rasterized to a live texture on an auto-quad
  (cheap, works today) or tessellated to real geometry (crisp, later).
  Ortho 2D and perspective 3D both just work because it's an entity.

Motion = the timeline system (FlashTimeline / SWF tweens) driving Group
transforms + Shape morphs - shared by UI animation and in-world animation
because they are the same objects.

## Text specifically: MSDF is the effective answer

The per-change rasterize-to-texture path can't be the future: it allocates
per edit (churn - now crash-safe via the texture graveyard, still wasteful)
and its fixed resolution blurs under zoom/perspective. The industry answer
is an MSDF (multi-channel signed distance field) glyph atlas:

- ONE atlas per font, built once (msdfgen is small, MIT, or bake offline)
- every consumer (world text, UI labels, subtitles, menus) draws quads
  sampling the same atlas - crisp at ANY scale, no re-rasterize on edit,
  a text change is just new vertex data
- effects (outline, glow, softness - the retro looks) are shader params,
  not rasterizer features

TextComponent keeps its authoring surface (text, font, size, color, wrap,
align); only the backend changes. The rasterizer stays for the fallback
and for baked-texture cases (matching the hand-crafted philosophy: you can
always bake).

## Phasing (each step ships alone)

- **P0 (done 2026-08-30)**: text-change safety - replaced text textures park
  in a graveyard + bindless slots freed (typewriter/caret no longer leaks
  or crashes). Auto-quad already shipped (b6129652).
- **P1**: MSDF font atlas + a glyph-quad draw path; route TextComponent
  through it (worldHeight/lit/billboard fields from the Ink_Ribbon spec
  land here). UICanvas labels + subtitles switch to the same atlas.
- **P2**: DisplayGraphic component - a vector/raster display tree (from
  VectorDrawing or SWF) usable as a world entity OR a canvas element; one
  compositor, two mounts. Timelines animate it in both.
- **P3**: UI layout system (#12) rebuilt ON display trees - layout
  containers as Group rules, dev layout and creative layout as two views.
  UICanvas's 8 fixed widgets become display-tree presets instead of
  hardcoded types (closes the "UI canvas flexibility" audit gap).

## Why this order

P1 kills the worst per-frame cost and unifies all text with no visible
authoring change. P2 turns the dormant Flash assets (SWFLoader, vector
panel, timeline) into the engine's signature 2D system. P3 then gets a
flexible UI for free, because UI stops being a special case - it's the
same display tree mounted to the screen.

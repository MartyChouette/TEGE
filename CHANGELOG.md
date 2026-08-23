# Changelog

Release history lives on
[GitHub Releases](https://github.com/MartyChouette/TEGE/releases) — each release
carries its notes and downloadable builds. This file is the quick orientation.

## Unreleased (since 0.9.7)

- **Procgen component suite**: DungeonGenerator, Scatter (4 distributions +
  terrain conform), TerrainGenerator (FBM + erosion + auto-splat), Wave Function
  Collapse (2D tiles + 3D prefab modules), RandomBag (5 modes incl. Markov).
- **Play-mode rewind timeline**: every play session records the whole scene;
  pause, step backward, scrub, resume from anywhere.
- **Options live preview**: hover a visual setting in the in-game options and
  the screen splits — left without the effect, right with it.
- **Editor MCP server**: drive the running editor over an open protocol —
  entity/component CRUD, play control, screenshots (off by default).
- **Serializer registry**: one table drives all component save/load; fixed a
  class of silently-vanishing components (audio suite and others on reload).
- **CI render smoke test**: every push boots the editor on software Vulkan and
  verifies a real frame renders.
- Sprite images no longer import upside down; editor selection glow no longer
  leaks into the game view; scene `"version"` format documented.

## 0.9.7 — current public preview

The version on [the website](https://www.marty64.net/enjin/) and
[Releases](https://github.com/MartyChouette/TEGE/releases): full editor, 8 art
styles, accessibility-by-default exports, WebGPU web player, Jolt/Box2D physics,
AngelScript + visual scripting, ray tracing preview.

## Earlier

0.8.0 → 0.9.5 previews: see their release entries.

# Changelog

Release history lives on
[GitHub Releases](https://github.com/MartyChouette/TEGE/releases), each release
carries its notes and downloadable builds. This file is the quick orientation.

## Unreleased (since 0.9.7)

- **Procgen component suite**: DungeonGenerator, Scatter (4 distributions +
  terrain conform), TerrainGenerator (FBM + erosion + auto-splat), Wave Function
  Collapse (2D tiles + 3D prefab modules), RandomBag (5 modes incl. Markov).
- **Play-mode rewind timeline**: every play session records the whole scene;
  pause, step backward, scrub, resume from anywhere.
- **Options live preview**: hover a visual setting in the in-game options and
  the screen splits, left without the effect, right with it.
- **Editor MCP server**: drive the running editor over an open protocol
  entity/component CRUD, play control, screenshots (off by default).
- **Serializer registry**: one table drives all component save/load; fixed a
  class of silently-vanishing components (audio suite and others on reload).
- **CI render smoke test**: every push boots the editor on software Vulkan and
  verifies a real frame renders.
- **Script exceptions say what and where**: a thrown exception used to reach
  the log as "Unknown error in OnUpdate". It now carries the fault, the
  function, and the section and line it threw on.
- **Sprites are a real pipeline now**: every sprite carries its own texture and
  the whole scene draws in one call, so a scene with twenty different images
  costs what one image used to. Fixes along the way: sort order stopped being
  applied after the first frame, textured sprites drew upside down, scaling a
  sprite worked only if it had a parent, a scene with two textures drew both
  with one of them, `pivot` did nothing, and a rotated non-square sprite kept
  its silhouette. `Sprite_SetSize`/`GetWidth`/`GetHeight` added. Lit vs unlit is
  now per sprite instead of decided for the whole scene by its light count.
- **Web sprites caught up with desktop**: rotation reached them (it was pinned
  at zero), so did flip, the parent chain, and sprite-sheet UVs, which were
  being sent as raw pixels.
- **Audio, gone over as a whole**: `SimpleAudio` is `AudioEngine`, and an
  unused FMOD/Wwise backend layer that had never run in any build is gone
  (1,518 lines). The Audio Source inspector can finally play a sound: drop or
  browse for a clip, press Play to hear it without entering play mode, scrub a
  real transport. Double-click any audio file in the Asset Browser to preview
  it. The Randomization section now does something — alternate clips and
  pitch/volume ranges were authored and saved and read by nothing, so every
  footstep was the same recording.
- **Web audio was silent until the player clicked, and now waits properly**:
  browsers refuse to start audio before a user gesture, and the engine used to
  play into the suspended context anyway. Play-on-awake sounds are held until
  that first input, so they start from the beginning.
- **Scripts: ask where the audio is** — `Audio_GetTime`, `Audio_GetLength` and
  `Audio_Seek`, which is what timed subtitles need. Data Assets and the sprite
  bindings are documented for the first time.
- **A failed entity lookup says something**: `Scene_FindEntity` returned 0 and
  logged nothing. It now names near-misses, and points out when a name exists
  in a different case.
- **Double-click a console error to open the script at that line**, in the
  editor, no external IDE needed.
- Editor selection glow no longer leaks into the game view; scene `"version"`
  format documented.

## 0.9.7, current public preview

The version on [the website](https://www.marty64.net/enjin/) and
[Releases](https://github.com/MartyChouette/TEGE/releases): full editor, 8 art
styles, accessibility-by-default exports, WebGPU web player, Jolt/Box2D physics,
AngelScript + visual scripting, ray tracing preview.

## Earlier

0.8.0 → 0.9.5 previews: see their release entries.

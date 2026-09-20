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
- **Fluid simulation**: fluid volumes that know about scene colliders, liquid
  that falls and sinks, a surface mesh instead of a cloud of particles, presets
  named after what they make, and recording a sim so it can be played back or
  baked. Bakeable from the editor without writing C++. Three volumes now cost
  three volumes; 59% of one was never being drawn.
- **First-party acoustics**: a room's reverb measured from its own geometry,
  early reflections for the cue that says where in the room a sound is, and
  per-surface acoustic materials, where every surface in the world used to be
  handed the same one. Three reasons the acoustics were inaudible turned out to
  be upstream of the DSP. Ships with the Acoustic Range, eleven spaces to walk
  and a trigger to hear them with.
- **Caves**: the terrain surface can be punched out and a Cave tool digs the
  tunnel, meshed with signed distance fields and surface nets. Dig where you
  point, with five brushes and a preview of what the dig will do. Terrain could
  previously only be raised, and a held stroke crawled away from the cursor.
- **Inverse kinematics reaches the screen**, which no IK in this engine had ever
  done. Spline IK chains run, the look-at solver's answer arrives at the head
  bone, and per-finger conforming puts a hand on a worktop or a railing and
  works out for itself which it is. Reachable from the editor, and it saves.
- **TAA antialiases, for the first time on any platform**. A scene's
  antialiasing mode now reaches the renderer, moving objects carry velocity so
  TAA can reproject them, and an exported game applies its scene's
  post-processing instead of the defaults. Selecting TAA or an upscaler used to
  make a build look worse, and turned the game view black.
- **Geometry can leave the editor**: right-click an entity and Export as Model
  writes a `.glb`, a selection exports as one file with its arrangement intact.
  **Remesh** welds a wall of overlapping, self-intersecting parts into one
  closed surface, which Reduce cannot do.
- **Ctrl+X, Ctrl+C and Ctrl+V do something**, and they take the selection rather
  than the entire scene.
- **Saves, gone over end to end**: a save is a delta against the level rather
  than a copy of it, save points save, and auto-save had never run in a shipped
  game. New Game and Continue were the same button, and Continue said there was
  something to continue when there was not. A script can declare what persists,
  so run state can live on entities, and a game can ask for its saves to be
  uploaded.
- **Undo covers the colour pickers**: all of them, including alpha, the skybox
  and 2D water, layer colours and palette entries. The sub-editors keep their
  own undo, because they are editing their own documents and not the scene.
- **Three official editor modes**: Developer, Creative and Tutorial. Creative
  Mode's seven build tools dragged out a preview and then built nothing; the
  engine's vegetation was reachable only from a window you had to know about;
  23 of 26 tool panels opened larger than the window.
- **Gameplay that was authored and never ran**: gravity zones now reach the
  first-person controller, so a zero-G room drifts. A destructible drops the
  pickups it was authored to drop, damage resistances apply to damage, dynamic
  difficulty adjusts damage instead of publishing a number about it, a rigidbody's
  speed ceiling is enforced instead of documented, a Triggered platform waits for
  someone to press its switch, a trigger zone tells the entity it names, and a
  key can open a door. Eight gameplay components had an inspector and no system.
- **An entity can follow a route you drew**, without writing a script.
- **Web caught up again**: parallax occlusion mapping, three retro shading modes,
  2D water and the orthographic camera it needed, 2D weather falling as a sheet,
  fluid drawing and simulating, tilemaps, the cel outline, MeshRenderer filters,
  frustum culling and animation LOD. Web accessibility stopped being
  desktop-minus. Two that had been wrong since the beginning: the web sky was
  the engine's default in every scene ever shipped, and every lake in a browser
  was a static slab, behind a shader bit nothing set.
- **Colour and lighting fixes**: desktop stopped gamma-encoding twice, terrain
  stopped rendering in primary colours (its texture layers were authored, saved,
  and never rendered), an empty shadow map got cached on frame one and kept for
  the whole run, a post-process volume applies the strength it was authored with,
  and the scene-wide probe stopped re-baking a world that never stops moving.
- **A thousand instances of one asset upload its geometry once.**
- **`.ogg` makes a sound**, and a file that cannot be decoded says so instead of
  failing silently. Embedded glTF textures survive the import. Particle sprite
  sheets animate, one frame per particle's own age.
- **LAN multiplayer could not connect at all, and reliable messages were being
  thrown away.** Two separate faults in the same layer, both found by chasing
  collaborative editing. The host authenticates its connection reply, a joining
  client has no session key yet, so every client dropped that reply as malformed
  and waited forever; the handshake is exempt from authentication on both sides
  now, the way it has to be. Underneath that, a reliably-sent message was
  wrapped in a packet type the receiver had no case for, so it was discarded
  while the ack still went back: reliable delivery reported success for messages
  it never dispatched. Fixed, along with what a real transfer then needs --
  messages larger than one datagram are split and reassembled instead of
  vanishing, retransmits are de-duplicated, both are paced so a burst does not
  trip the receiver's own flood protection, and a transfer that cannot complete
  says so instead of leaving the other end waiting. Collaborative editing
  connects and requests a scene sync; completing one is not verified yet.
- **Removed**: Weather2D, the 2D weather that never drew anything, and the
  Template Marketplace.
- **Under the build**: 91 tests rejoined CI, a new field that nothing reads now
  fails CI, an absent test stopped looking like a green one, and there is a guard
  for the "works in the editor, dead in a build" class of bug. One `Vector.h`
  instead of two, which is what finally started the SIMD math running.

## 0.9.7, current public preview

The version on [the website](https://www.marty64.net/enjin/) and
[Releases](https://github.com/MartyChouette/TEGE/releases): full editor, 8 art
styles, accessibility-by-default exports, WebGPU web player, Jolt/Box2D physics,
AngelScript + visual scripting, ray tracing preview.

## Earlier

0.8.0 → 0.9.5 previews: see their release entries.

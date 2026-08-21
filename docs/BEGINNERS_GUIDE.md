# TEGE — Beginner's Guide & Cheat Sheet

The one page to start from. It gets you from "just cloned it" to a running,
exported game, then stays open as a quick reference. Everything here is checked
against the actual engine (commit-current as of 2026-08-21), so where older docs
disagree, trust this one.

**TEGE (The Enjin Game Engine)** is a from-scratch C++20 / Vulkan game engine with
a full ImGui editor, an Entity-Component-System core, dual physics (Jolt 3D,
Box2D 2D), AngelScript + visual scripting, and two flagship ideas: **any art style
is a one-click preset**, and **every game ships accessible by default**.

---

## 1. Install & first launch

**You need:** a C++20 compiler (MSVC on Windows / GCC or Clang on Linux), CMake,
and the Vulkan SDK. Windows is the primary platform; Linux builds from source.

```bash
git clone https://github.com/MartyChouette/TEGE.git
cd TEGE

# Windows (Visual Studio)
cd build && cmake .. && cmake --build . --config Release

# Linux / Mac
cd build && cmake .. && make -j$(nproc)
```

Run the editor:
- Windows: `build/bin/Release/EnjinEditor.exe`
- Linux/Mac: `build/bin/EnjinEditor`

> After adding new source files, re-run `cmake ..` in `build/` — the build globs
> sources at configure time. If you touched shaders, see §11.

---

## 2. Ship your first game in 10 minutes

The fastest path to a real, playable, exported `.exe`:

1. **Launch `EnjinEditor`.** You land on the Project Hub.
2. **New Project → pick a template.** Choose **Coin Rush** (a complete
   collect-the-coins game) — not "Blank" for your first run.
3. **Press Play** (the ▶ toolbar button, or the Play shortcut). Move with WASD,
   collect coins. This is the actual game loop running.
4. **Press Stop.** Any changes you made in Play mode roll back (scene edits persist,
   play-state doesn't).
5. **Tweak something.** Select the player in the Hierarchy, change a value in the
   Inspector (movement speed on its Script, a Material colour, a Light).
6. **Build → Export.** Configure output directory + window title, run the build.
   TEGE packs your project into a `game.enjpak` and drops a standalone player next
   to it.
7. **Run the exported `.exe`.** That's your game, shipping without the editor.

That is the whole arc: template → edit → play → export → run. Everything below is
detail on each step.

---

## 3. The editor at a glance

| Panel | What it's for |
|---|---|
| **Hierarchy** | The entity tree for the open scene. Select, parent, rename, delete. |
| **Inspector** | Components on the selected entity. Each panel is self-documenting (what it does, how to use it, a script snippet, what it connects to). Add Component is at the bottom. |
| **Viewport** | The 3D/2D scene. Fly with WASD + right-mouse. Gizmos move/rotate/scale the selection. |
| **Asset Browser** | Project files. Drag assets onto entities, the viewport, or component fields. |
| **Scene Settings** | Per-scene art style, sky, fog, post-processing. |
| **Console** (`` ` ``) | Log output + commands. |

### Keyboard shortcuts

| Key | Action | Key | Action |
|---|---|---|---|
| `1` / `2` / `3` | Translate / Rotate / Scale gizmo | `F` | Focus camera on selection |
| `4` | Toggle world/local gizmo space | `Ctrl+D` | Duplicate selection |
| `WASD` + RMB | Fly camera | `Delete` | Delete selection |
| `Ctrl+P` | Command palette | `` ` `` | Toggle console |
| `F11` | Fullscreen game view (while playing) | `F1` | Game Debug panel |
| `Esc` | Exit fullscreen / focus mode | `F2` | Engine Debug Workstation |

---

## 4. Core concepts

- **Project** — a `.enjinproject` file (JSON manifest) plus its `assets/`, `scripts/`,
  and scene files. One project, many scenes.
- **Scene** — a `.enjin` file (JSON). A tree of entities. One scene is the start
  scene (`isStartScene`); the build list controls inclusion/order.
- **Entity** — a handle. Owns nothing by itself; it's the sum of its components.
  IDs are generational — a destroyed-and-recreated entity is a *different* handle.
- **Component** — plain data attached to an entity (Transform, Mesh, Material,
  Light, Camera, Sprite2D, Rigidbody, Script, …). 140+ types.
- **System** — logic that runs over components each frame (rendering, physics,
  scripting, animation).

> **File extensions:** `.enjinproject` = project, `.enjin` = scene. (There is no
> `.enjscene`.) In scene JSON the script key is `scriptComponent`.

---

## 5. Component cheat sheet (the ones you'll use most)

| Component | Does | Notes |
|---|---|---|
| **Transform** | Position / rotation / scale | Rotation is Euler degrees in the inspector. Parenting nests transforms. |
| **Mesh** | The 3D geometry drawn | Pair with a **Material**. `subMeshes` for multi-material. |
| **Material** | PBR look: colour, textures, roughness, metalness | Drives lit shading. Has a "Surface Response" block (footstep/impact sounds). |
| **Light** | Directional / point / spot | **No direction field** — a directional light's aim comes from the Transform's rotation. |
| **Camera** | The view | One is the game camera. Perspective or orthographic (don't mix in one scene). |
| **Sprite2D** | A 2D image quad | `flipX`/`flipY` toggles. Sorting layer + order control draw depth. |
| **Rigidbody / Colliders** | Physics body + shape | 3D = Jolt, 2D = Box2D — never mix in one scene. Collider sizes are **world-space** (scale is ignored). |
| **Script** | Attaches an AngelScript behavior | See §7. Serialized under key `scriptComponent`. |
| **Tilemap** | 2D tile grid | Painted by the Dungeon/WFC procgen components. |
| **Terrain** | Heightmesh | Painted by the Terrain Generator. |
| **Procgen suite** | DungeonGenerator, Scatter, TerrainGenerator, WFC (2D tiles + 3D modules), RandomBag | Each has a "Generate Now" button and a self-documenting panel. |

Add a component via **Add Component** at the bottom of the Inspector (grouped by
category: Rendering, Physics, Effects, Procedural, UI, …). Every panel tells you
what it connects to and offers to add missing partners.

---

## 6. One-click art styles (flagship)

**Scene Settings → Art Style Preset.** Pick a look from a dropdown and the whole
scene re-shades. Seven presets:

`Realistic PBR` · `Classic Blinn-Phong` · `Hand-Painted` · `Toon / Anime` ·
`Low-Poly Retro` · `Pixel Art` · `NPR Sketch`

Each preset is a bundle of shading model + post-processing settings (dithering,
cel outlines, crosshatch, palette). After applying one you can still hand-tune
every value underneath it — the preset is a starting point, not a lock.

---

## 7. Scripting quickstart (AngelScript)

Scripts live in your project's `scripts/` folder as `.as` files and attach via a
**Script** component. Every script is a class that inherits **`TegeBehavior`**.

```angelscript
class PlayerController : TegeBehavior {
    [Property] float moveSpeed = 5.0f;   // shows up as a slider in the Inspector

    void OnStart() {
        // called once when play begins
    }

    void OnUpdate(float dt) {
        Vector3 move = Vector3(0, 0, 0);
        if (Input_GetKey(Key::W)) move.z -= 1;
        if (Input_GetKey(Key::S)) move.z += 1;
        if (Input_GetKey(Key::A)) move.x -= 1;
        if (Input_GetKey(Key::D)) move.x += 1;

        move = move.Normalized() * moveSpeed;
        SetPosition(GetPosition() + move * dt);   // TegeBehavior transform helpers
    }
}
```

**Lifecycle callbacks** (override any): `OnCreate()`, `OnStart()`, `OnUpdate(float dt)`,
`OnFixedUpdate(float dt)`, `OnLateUpdate(float dt)`, `OnDestroy()`.

**Built-in helpers on `TegeBehavior`:** `GetPosition()/SetPosition()`,
`GetRotation()/SetRotation()`, `GetScale()/SetScale()`, `GetName()`, `GetEntity()`,
`StartCoroutine("funcName")`.

**Common global API families** (roughly 1,000+ bindings): `Input_*`, `Entity_*`,
`Physics_*`, `Camera_*`, `Audio_*`, plus the procgen ones (`RandomBag_Draw`,
`Scatter_Generate`, `TerrainGen_Generate`, `WFC_Generate`). The full reference is
`docs/SCRIPTING_API.md`. Scripts are sandboxed (1M-instruction limit) and `#include`
is restricted to the script directory.

> Module names are `parentDir_stem` (`scripts/Foo.as` → module `scripts_Foo`).

---

## 8. Accessibility (flagship — on by default)

Every exported game carries an accessibility menu. What you get out of the box:

- **Vision:** 8 colorblind correction modes (Protanopia, Deuteranopia, Tritanopia,
  their -anomaly variants, Achromatopsia, Achromatomaly) with strength, plus
  brightness/contrast and high-contrast themes.
- **Reading:** OpenDyslexic dyslexia-friendly font, text scaling.
- **Motion:** reduced-motion toggle (respected by animated UI).
- **Motor:** remappable input, one-handed presets, dwell-click (hover to click),
  sticky/switch access.
- **Audio & narration:** screen-reader / announcer TTS and a status bar in exported
  games.

You don't wire this up per game — it's engine-provided. Author your UI with the
UICanvas anchors and it inherits the accessibility layer. There's an
**Accessibility Demo** template that is exactly the public web demo.

---

## 9. Build & export your game

1. **Build → Export** (or configure a `BuildConfig`: project path, output dir,
   window title/size).
2. TEGE runs the **BuildPipeline**: packs assets + scripts into a `game.enjpak`,
   emits loose `scripts/`, `scripts/enjin_api/`, and `assets/` next to the exe, and
   copies a prebuilt **`EnjinPlayer.exe`**.
3. The player loads `game.enjpak` from its own directory at startup.

> If you changed engine code, rebuild the `EnjinPlayer` target too, or exports ship
> a stale engine.

**Web:** builds target WebAssembly + WebGPU via Emscripten (`ENJIN_PLATFORM_WEB=ON`);
output is `build-web/bin/EnjinPlayer.{js,wasm}`. Best in Chrome/Edge (WebGPU).

---

## 10. Gotchas that will bite you (keep these in mind)

- **Scene extension is `.enjin`**, project is `.enjinproject`. No `.enjscene`.
- **Directional lights have no direction field** — rotate the light's Transform.
- **Collider sizes are world-space** — Jolt/Box2D ignore Transform scale. A
  `(50, 0.1, 50)` box is 50 units regardless of the entity's scale.
- **Never mix 2D and 3D physics** in one scene (Box2D vs Jolt are separate worlds).
- **Don't mix perspective and orthographic cameras** in one scene.
- **UICanvas anchors are Unity-style:** `edge = anchor*parent + offset`. A centered
  200-wide element needs `offsetLeft=-100, offsetRight=+100` (not `+100/-100`).
- **Unknown entity keys in scene JSON are silently dropped** on the next save — if
  you hand-edit a scene, use the exact serializer keys.
- **The editor auto-saves the open scene on a timer** — don't edit a scene file
  out-of-band while the editor is open on it.

---

## 11. Shader edit workflow (only if you touch `Engine/shaders/`)

Editing a shader isn't just editing the GLSL — the engine reads embedded SPIR-V:

```
edit .glsl  →  glslangValidator -V  →  python _gen_all.py  →  rebuild engine
```

After any change to a UBO/SSBO struct (LightingUBO, UniformBufferObject,
MaterialGPU, …), recompile **all** affected shaders and regenerate `ShaderData.h`,
or the GPU reads wrong offsets (dark scenes, wrong colours, crashes). Ray-tracing
shaders use `glslc --target-env=vulkan1.2 -I.` then `python _gen_rt.py`.

---

## 12. Quick reference card

**Extensions:** `.enjinproject` (project) · `.enjin` (scene) · `.enjprefab` (prefab)
· `.enjpak` (packed game) · `.enjshader` (shader graph)

**Build:** `cd build && cmake .. && cmake --build . --config Release`
**Run editor:** `build/bin/Release/EnjinEditor.exe`
**Run tests:** `cd build && ctest -C Release --output-on-failure` (needs
`-DENJIN_BUILD_TESTS=ON` at configure)

**Gizmos:** `1` move · `2` rotate · `3` scale · `4` space · `F` focus ·
`Ctrl+D` dup · `Delete` delete
**Windows:** `Ctrl+P` palette · `` ` `` console · `F1` game debug · `F2` engine debug
· `F11` fullscreen game

**Script skeleton:** `class Name : TegeBehavior { void OnUpdate(float dt) { … } }`
**Script input:** `Input_GetKey(Key::W)` · **move:** `SetPosition(GetPosition() + v*dt)`

**Physics:** 3D = Jolt · 2D = Box2D · never mix · collider sizes are world-space
**Art style:** Scene Settings → Art Style Preset (7 presets)
**Accessibility:** engine-provided menu in every export (8 colorblind modes,
dyslexia font, motor + TTS)

---

## Where to go next

- `docs/USER_MANUAL.md` — the full component + editor reference (canonical manual).
- `docs/SCRIPTING_API.md` — every AngelScript binding.
- `docs/TUTORIALS.md` — step-by-step tutorials.
- `docs/BUILD.md` — detailed build/dependency guide.
- `docs/ARCHITECTURE.md` — how the engine is put together.

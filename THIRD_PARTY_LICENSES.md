# Third-Party Licenses

Enjin (TEGE) uses the following third-party libraries. Each library retains
its original license and copyright.

---

## Dear ImGui

- **Version:** 1.92.7
- **License:** MIT
- **Copyright:** (c) 2014-2026 Omar Cornut
- **Source:** https://github.com/ocornut/imgui
- **Description:** Immediate-mode graphical user interface library used for the
  editor UI, inspector panels, and debug overlays.
- **Location:** `third_party/imgui/`

---

## ImGuizmo

- **Version:** 1.92.5
- **License:** MIT
- **Copyright:** (c) 2016-2021 Cedric Guillemet
- **Source:** https://github.com/CedricGuillemet/ImGuizmo
- **Description:** Immediate-mode 3D gizmo for scene manipulation (translate,
  rotate, scale handles in the viewport).
- **Location:** `third_party/imguizmo/`

---

## AngelScript

- **Version:** 2.38.0
- **License:** zlib
- **Copyright:** (c) 2003-2025 Andreas Jonsson
- **Source:** http://www.angelcode.com/angelscript/
- **Description:** Scripting language engine used for gameplay scripting.
  Sandboxed with instruction limits.
- **Location:** `third_party/angelscript/`

---

## NanoSVG

- **Version:** (header-only, no tagged version)
- **License:** zlib
- **Copyright:** (c) 2013-14 Mikko Mononen
- **Source:** https://github.com/memononen/nanosvg
- **Description:** SVG parser and rasterizer used for vector icon rendering.
- **Location:** `third_party/nanosvg/`

---

## Jolt Physics

- **Version:** 5.2.0
- **License:** MIT
- **Copyright:** (c) 2021 Jorrit Rouwe
- **Source:** https://github.com/jrouwe/JoltPhysics
- **Description:** 3D physics engine providing rigid body simulation, character
  controllers, raycasting, and collision detection.
- **Fetched via:** CMake FetchContent (`Engine/CMakeLists.txt`)

---

## Box2D

- **Version:** 3.0.0
- **License:** MIT
- **Copyright:** (c) 2022 Erin Catto
- **Source:** https://github.com/erincatto/box2d
- **Description:** 2D physics engine providing rigid body simulation, sensors,
  raycasting, and collision callbacks.
- **Fetched via:** CMake FetchContent (`Engine/CMakeLists.txt`)

---

## Assimp (Open Asset Import Library)

- **Version:** 5.4.3
- **License:** BSD 3-Clause
- **Copyright:** (c) 2006-2021, assimp team
- **Source:** https://github.com/assimp/assimp
- **Description:** 3D model importer supporting glTF, FBX, OBJ, and many other
  formats. Used for mesh and scene loading.
- **Fetched via:** CMake FetchContent (`Engine/CMakeLists.txt`)

---

## nlohmann/json

- **Version:** 3.11.3
- **License:** MIT
- **Copyright:** (c) 2013-2022 Niels Lohmann
- **Source:** https://github.com/nlohmann/json
- **Description:** JSON parser and serializer for C++. Used for scene files,
  project settings, build configs, and editor state.
- **Fetched via:** CMake FetchContent (`Engine/CMakeLists.txt`)

---

## GLFW

- **Version:** 3.3.8
- **License:** zlib/libpng
- **Copyright:** (c) 2002-2006 Marcus Geelnard, (c) 2006-2019 Camilla Loewy
- **Source:** https://github.com/glfw/glfw
- **Description:** Cross-platform window creation, input handling, and OpenGL/Vulkan
  context management.
- **Fetched via:** CMake FetchContent (`Core/CMakeLists.txt`)

---

## miniaudio

- **Version:** 0.11.21
- **License:** Public Domain (Unlicense) or MIT-0 (dual-licensed, choose either)
- **Copyright:** (c) 2023 David Reid
- **Source:** https://github.com/mackron/miniaudio
- **Description:** Single-header audio playback and capture library. Provides the
  default audio backend for sound effects and music.
- **Fetched via:** CMake FetchContent (`Engine/CMakeLists.txt`)

---

## stb_image

- **Version:** 2.30
- **License:** Public Domain (Unlicense) or MIT (dual-licensed, choose either)
- **Copyright:** (c) 2017 Sean Barrett
- **Source:** https://github.com/nothings/stb
- **Description:** Single-header image loader supporting JPEG, PNG, BMP, TGA, PSD,
  GIF, HDR, and PIC formats. Used for texture loading.
- **Location:** `Engine/include/stb_image.h`

---

## stb_image_write

- **Version:** 1.16
- **License:** Public Domain (Unlicense) or MIT (dual-licensed, choose either)
- **Copyright:** (c) 2017 Sean Barrett
- **Source:** https://github.com/nothings/stb
- **Description:** Single-header image writer for PNG, BMP, TGA, JPEG, and HDR.
  Used for screenshot capture and texture export.
- **Location:** `Engine/include/stb_image_write.h`

---

## stb_truetype

- **Version:** 1.26
- **License:** Public Domain (Unlicense) or MIT (dual-licensed, choose either)
- **Copyright:** (c) 2009-2021 Sean Barrett / RAD Game Tools
- **Source:** https://github.com/nothings/stb
- **Description:** Single-header TrueType font rasterizer. Used for runtime text
  rendering.
- **Location:** `Engine/include/stb_truetype.h`

---

## Vulkan SDK

- **License:** Apache 2.0 (Vulkan headers and loader), various (validation layers)
- **Copyright:** The Khronos Group Inc.
- **Source:** https://vulkan.lunarg.com/
- **Description:** Graphics and compute API. The Vulkan SDK provides headers,
  validation layers, and shader compilation tools (glslangValidator). Installed
  separately; not bundled in this repository.

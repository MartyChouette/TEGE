# Getting Started with Enjin Engine

## Prerequisites

- C++20 compatible compiler (MSVC 2022, GCC 12+, Clang 14+)
- CMake 3.20+
- Vulkan SDK
- GLFW3

## Building

### Windows (Visual Studio)

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux/Mac

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Running the Editor

```bash
# Windows
./build/bin/Release/EnjinEditor.exe

# Linux/Mac
./build/bin/EnjinEditor
```

## First Launch

1. **Splash Screen** appears for 3 seconds
2. **Template Selector** lets you pick a starting point:
   - **Blank** - Empty scene with a light
   - **2D Platformer** - Side-scrolling game setup
   - **2D Top-Down** - Overhead 2D game setup
   - **3D Isometric** - Fixed-angle 3D setup
   - **3D Third Person** - Over-the-shoulder camera
   - **3D First Person** - FPS-style camera
   - Or open an existing `.enjin` scene file
3. **Editor** opens with all panels

## Editor Quick Start

### Creating Entities

- `Entity > Create Empty` - New empty entity with transform
- `Entity > 3D Object` - Cube, Sphere, Plane, Cylinder, Cone
- `Entity > 2D Object` - Quad, Sprite
- `Entity > Light` - Directional, Point, or Spot light
- `Entity > Camera` - In-game camera

### Adding Components

Select an entity in the Hierarchy, then in the Inspector panel use `Add Component` to attach:
- Physics (Rigidbody, Colliders, Triggers)
- Controllers (Platformer, Top-Down, Third/First Person)
- Gameplay (Health, Damage, Pickup, Inventory, Timer)
- Environment (Weather Zone, Water, Grass, Gravity Zone)
- AI (AI Controller, Follow/LookAt Target, Waypoints)
- Visual (Billboard, Particles, Sprites, Tilemap)

### Importing 3D Models

`File > Import Model...` (Ctrl+I) to load `.gltf` or `.glb` files. Models are automatically converted to entities with meshes, materials, and (if present) skeletal animation.

### Scene Management

Save scenes as `.enjin` files (`Ctrl+S`). For multi-scene projects:
1. Open `View > Scene List`
2. Create a project (`File > New Project...`)
3. Add scenes to the project
4. Set a start scene
5. Save the project as `.enjinproject`

Scenes can be loaded with transitions (fade to black, fade to white, cross fade) via the Scene List panel.

### Play Mode

Use the Game View panel controls:
- **Play** - Start game preview (controllers become active)
- **Pause** - Freeze game state
- **Stop** - Return to editor state

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `1/2/3` | Translate/Rotate/Scale gizmo |
| `4` | Toggle local/world space |
| `WASD` | Fly camera |
| `Shift` | Sprint |
| `Ctrl+S` | Save scene |
| `Ctrl+O` | Open scene |
| `Ctrl+I` | Import model |
| `Ctrl+X/C/V` | Cut/Copy/Paste entity |

## Troubleshooting

### "Vulkan not found"
- Install the Vulkan SDK and ensure `VULKAN_SDK` is set

### Build errors
- Verify C++20 support in your compiler
- Check CMake version is 3.20+
- Run `cmake ..` again after adding new source files

### No window appears
- Check GPU has Vulkan support
- Update GPU drivers

# Building Enjin Engine

This is the consolidated build guide for all platforms. It covers dependency installation, building, shader compilation, cross-compilation, and troubleshooting.

## 1. Prerequisites

### Required

- **CMake** 3.20 or higher
- **C++20 compatible compiler**:
  - GCC 10+ (Linux)
  - Clang 12+ (Linux/macOS)
  - MSVC 2019+ (Windows)
- **Vulkan SDK** 1.3+
  - Download from: https://vulkan.lunarg.com/
  - Ensure the `VULKAN_SDK` environment variable is set after installation
- **GLFW3** (for windowing)
  - Available via package managers or from https://www.glfw.org/

### Optional

- **glslang / glslc** (for shader compilation)
- **shaderc** (alternative shader compiler)

### Verifying Installation

```bash
# Check Vulkan
vulkaninfo --summary

# Check shader compiler
glslc --version

# Check GLFW
pkg-config --modversion glfw3

# Check CMake
cmake --version
```

## 2. Installing Dependencies

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers \
    libglfw3-dev \
    glslang-tools
```

On Ubuntu 20.04 or Debian 11, if `glslang-tools` is not available, try `glslang-dev` instead, or install the Vulkan SDK directly from LunarG which includes `glslc`.

### Fedora / RHEL

```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    vulkan-devel \
    vulkan-tools \
    glfw-devel \
    glslang
```

### Arch Linux

```bash
sudo pacman -S \
    base-devel \
    cmake \
    vulkan-devel \
    vulkan-tools \
    glfw \
    glslang
```

### macOS

```bash
brew install cmake vulkan-headers vulkan-loader glfw glslang
```

macOS does not have native Vulkan support. You will need MoltenVK:

```bash
brew install molten-vk
```

### Windows

1. **Visual Studio 2019 or 2022**
   - Install with the "Desktop development with C++" workload
   - Includes Windows SDK and MSVC compiler

2. **CMake** (3.20+)
   - Download from https://cmake.org/download/
   - Or use: `winget install Kitware.CMake`
   - Add to PATH during installation

3. **Vulkan SDK**
   - Download from https://vulkan.lunarg.com/sdk/home
   - Install to the default location (usually `C:\VulkanSDK\<version>`)
   - The installer automatically sets PATH and environment variables

4. **GLFW3** (choose one method)
   - **Option A (vcpkg, recommended)**:
     ```cmd
     git clone https://github.com/Microsoft/vcpkg.git
     cd vcpkg
     .\bootstrap-vcpkg.bat
     .\vcpkg install glfw3:x64-windows
     .\vcpkg integrate install
     ```
   - **Option B**: Download pre-built binaries from https://www.glfw.org/
   - **Option C**: Build GLFW from source with CMake

### Building GLFW from Source (Any Platform)

If GLFW is not available via your package manager:

```bash
git clone https://github.com/glfw/glfw.git
cd glfw
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

## 3. Building

### Windows -- Visual Studio GUI

1. Open Visual Studio
2. File > Open > CMake...
3. Select `CMakeLists.txt` in the project root
4. Build > Build All (or press F7)
5. Binaries appear in `build/bin/Release/`

### Windows -- Command Line

Open an "x64 Native Tools Command Prompt for VS 2019" (or 2022) and run:

```cmd
cd C:\path\to\enjin

mkdir build
cd build

:: Visual Studio 2019:
cmake .. -G "Visual Studio 16 2019" -A x64

:: Visual Studio 2022:
:: cmake .. -G "Visual Studio 17 2022" -A x64

:: Build Release:
cmake --build . --config Release

:: Build Debug:
:: cmake --build . --config Debug
```

If using vcpkg for GLFW, add the toolchain file flag:

```cmd
cmake .. -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### Linux

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### macOS

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.ncpu)
```

## 4. Build Options

Configure with CMake flags:

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENJIN_BUILD_EDITOR=ON \
    -DENJIN_BUILD_PLAYER=ON \
    -DENJIN_BUILD_TESTS=OFF \
    -DENJIN_BUILD_EXAMPLES=OFF
```

| Option | Default | Description |
|--------|---------|-------------|
| `ENJIN_BUILD_EDITOR` | ON | Build the editor application |
| `ENJIN_BUILD_PLAYER` | ON | Build the standalone game player |
| `ENJIN_BUILD_HUB` | OFF | Build the Enjin Hub launcher |
| `ENJIN_BUILD_TESTS` | OFF | Build unit tests |
| `ENJIN_BUILD_EXAMPLES` | OFF | Build the `Enjin::App` examples (output: `bin/Examples/`), including OrbCollector |
| **Physics** | | |
| `ENJIN_PHYSICS_JOLT` | ON | Enable Jolt Physics backend for 3D (FetchContent v5.2.0) |
| `ENJIN_PHYSICS_BOX2D` | ON | Enable Box2D v3 backend for 2D (FetchContent v3.0.0) |
| **Rendering** | | |
| `ENJIN_CLUSTERED_LIGHTING` | ON | Clustered forward lighting (16x9x24 spatial grid) |
| `ENJIN_VRS` | OFF | Variable Rate Shading (VK_KHR_fragment_shading_rate) |
| `ENJIN_VIRTUAL_TEXTURING` | OFF | Virtual texturing (page-based streaming) |
| `ENJIN_VISIBILITY_BUFFER` | OFF | Visibility buffer render path |
| **Ray Tracing Denoisers** | | |
| `ENJIN_RAYTRACING_OIDN` | OFF | Intel Open Image Denoise backend (requires OIDN install) |
| `ENJIN_RAYTRACING_OPTIX` | OFF | NVIDIA OptiX AI Denoiser (requires OptiX SDK + CUDA) |
| **Upscaling** | | |
| `ENJIN_UPSCALING_DLSS` | OFF | NVIDIA DLSS temporal upscaling (requires Streamline SDK) |
| `ENJIN_UPSCALING_XESS` | OFF | Intel XeSS temporal upscaling (requires XeSS SDK) |
| **Audio** | | |
| `ENJIN_AUDIO_STEAM_AUDIO` | OFF | Steam Audio HRTF binaural rendering (requires SDK in `third_party/steamaudio/`) |
| **Integrations** | | |
| `ENJIN_STEAM` | OFF | Steam integration (requires Steamworks SDK in `third_party/steamworks/`) |
| **Platform** | | |
| `ENJIN_PLATFORM_WEB` | OFF | Build for WebAssembly with WebGPU (requires Emscripten) |
| **Sanitizers** | | |
| `ENJIN_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `ENJIN_ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer (GCC/Clang only) |
| `ENJIN_ENABLE_TSAN` | OFF | Enable ThreadSanitizer (GCC/Clang only) |
| **Build Type** | | |
| `CMAKE_BUILD_TYPE` | -- | Debug, Release, RelWithDebInfo, MinSizeRel |

## 5. Running

After building, executables are located in `build/bin/` (Linux/macOS) or `build/bin/Release/` (Windows).

```bash
# Editor
./build/bin/Release/EnjinEditor.exe   # Windows
./build/bin/EnjinEditor               # Linux/macOS

# Standalone Player (requires game.enjpak in same directory)
./build/bin/Release/EnjinPlayer.exe   # Windows
./build/bin/EnjinPlayer               # Linux/macOS
```

Exported game builds emit `game.enjpak` plus loose `scripts/`, `scripts/enjin_api/`, and `assets/` folders next to the player executable. Scripts are read from disk, not from the pak.

## 6. Shader Compilation

Shaders are written in GLSL and compiled to SPIR-V for Vulkan.

### Using the compile script

```bash
chmod +x scripts/compile_shaders.sh
./scripts/compile_shaders.sh
```

### Manual compilation

```bash
cd Engine/shaders

# Using glslc (from Vulkan SDK or glslang-tools)
glslc triangle.vert -o triangle.vert.spv
glslc triangle.frag -o triangle.frag.spv
glslc outline.vert -o outline.vert.spv
glslc outline.frag -o outline.frag.spv
glslc cull.comp -o cull.comp.spv

# Using glslangValidator (alternative)
glslangValidator -V triangle.vert -o triangle.vert.spv
glslangValidator -V triangle.frag -o triangle.frag.spv
glslangValidator -V outline.vert -o outline.vert.spv
glslangValidator -V outline.frag -o outline.frag.spv
```

### Ray tracing shader compilation

RT shaders require Vulkan 1.2+ target environment and the `GL_EXT_ray_tracing` extension:

```bash
cd Engine/shaders

# Ray generation shaders (.rgen)
glslangValidator --target-env vulkan1.2 -V rt_shadow.rgen -o rt_shadow.rgen.spv
glslangValidator --target-env vulkan1.2 -V rt_reflect.rgen -o rt_reflect.rgen.spv
glslangValidator --target-env vulkan1.2 -V rt_ao.rgen -o rt_ao.rgen.spv
glslangValidator --target-env vulkan1.2 -V rt_gi.rgen -o rt_gi.rgen.spv
glslangValidator --target-env vulkan1.2 -V rt_pathtrace.rgen -o rt_pathtrace.rgen.spv

# Miss shaders (.rmiss)
glslangValidator --target-env vulkan1.2 -V rt_shadow.rmiss -o rt_shadow.rmiss.spv
glslangValidator --target-env vulkan1.2 -V rt_reflect.rmiss -o rt_reflect.rmiss.spv
glslangValidator --target-env vulkan1.2 -V rt_ao.rmiss -o rt_ao.rmiss.spv
glslangValidator --target-env vulkan1.2 -V rt_gi.rmiss -o rt_gi.rmiss.spv
glslangValidator --target-env vulkan1.2 -V rt_pathtrace.rmiss -o rt_pathtrace.rmiss.spv

# Closest hit shaders (.rchit)
glslangValidator --target-env vulkan1.2 -V rt_shadow.rchit -o rt_shadow.rchit.spv
glslangValidator --target-env vulkan1.2 -V rt_reflect.rchit -o rt_reflect.rchit.spv
glslangValidator --target-env vulkan1.2 -V rt_ao.rchit -o rt_ao.rchit.spv
glslangValidator --target-env vulkan1.2 -V rt_gi.rchit -o rt_gi.rchit.spv
glslangValidator --target-env vulkan1.2 -V rt_pathtrace.rchit -o rt_pathtrace.rchit.spv

# SVGF denoiser compute shaders
glslangValidator --target-env vulkan1.2 -V svgf_temporal.comp -o svgf_temporal.comp.spv
glslangValidator --target-env vulkan1.2 -V svgf_variance.comp -o svgf_variance.comp.spv
glslangValidator --target-env vulkan1.2 -V svgf_atrous.comp -o svgf_atrous.comp.spv

# RT compositor compute shader
glslangValidator --target-env vulkan1.2 -V rt_composite.comp -o rt_composite.comp.spv
```

RT SPIR-V bytecodes are embedded into `RTShaderData.h`. The system currently has placeholder stubs — replace them with compiled bytecode to activate the RT pipeline.

### Embedding compiled SPIR-V

After compiling to SPIR-V, the bytecodes are embedded into header files for distribution:

- Raster/compute shaders → `ShaderData.h`
- Ray tracing shaders → `RTShaderData.h`

The full workflow is:

1. Edit shader files in `Engine/shaders/`
2. Compile to `.spv` using glslc or glslangValidator
3. Convert to C++ byte arrays and update `ShaderData.h` / `RTShaderData.h`
4. Rebuild the engine

## 7. Cross-Compilation

### MinGW (Linux to Windows)

Install the MinGW-w64 toolchain:

```bash
sudo apt-get install mingw-w64
```

Create a CMake toolchain file (`mingw-toolchain.cmake`):

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Then configure and build:

```bash
mkdir build-windows && cd build-windows
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

Note: You will need to cross-compile GLFW for Windows as well, or provide pre-built Windows binaries. The Vulkan SDK for Windows must also be obtained separately.

### CI/CD (GitHub Actions)

For automated Windows builds, use a GitHub Actions workflow targeting `windows-latest` runners. A typical workflow installs the Vulkan SDK, configures CMake with the Visual Studio generator, builds, and uploads artifacts. See `.github/workflows/` for examples if available.

## 8. Troubleshooting

### Vulkan not found

```bash
# Linux: set environment variables
export VULKAN_SDK=/path/to/vulkan/sdk
export PATH=$VULKAN_SDK/bin:$PATH
export LD_LIBRARY_PATH=$VULKAN_SDK/lib:$LD_LIBRARY_PATH

# Windows: set environment variable
set VULKAN_SDK=C:\VulkanSDK\1.3.xxx.x
# Or add VULKAN_SDK via System Environment Variables in Control Panel
```

### GLFW not found

- **Linux**: `sudo apt-get install libglfw3-dev`
- **Fedora**: `sudo dnf install glfw-devel`
- **macOS**: `brew install glfw`
- **Windows with vcpkg**: Pass `-DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake` to CMake
- **Windows manual**: Pass `-DGLFW3_DIR=C:\path\to\glfw\install\lib\cmake\glfw3` to CMake

### glslc / shader compiler not found

```bash
# Ubuntu/Debian
sudo apt-get install glslang-tools

# Or use glslc bundled with the Vulkan SDK
```

### C++20 not supported

- Ensure your compiler meets the minimum version requirements (GCC 10+, Clang 12+, MSVC 2019 Update 16.11+)
- On Windows, check project properties: C/C++ > Language > C++ Language Standard > C++20

### Linker errors

- Ensure all dependencies are built for the same architecture (x64)
- On Linux: `export LD_LIBRARY_PATH=$VULKAN_SDK/lib:$LD_LIBRARY_PATH`
- On Windows: verify that the Vulkan SDK and GLFW are properly installed and detected by CMake

### CMake version too old

- Download the latest from https://cmake.org/download/
- Or on Linux: `sudo snap install cmake --classic`

## 9. Development Tools

### Recommended IDEs

- **Visual Studio** (Windows) -- native CMake support, integrated debugger
- **Visual Studio Code** -- with C/C++ and CMake Tools extensions
- **CLion** (JetBrains) -- cross-platform, CMake-native

### Graphics Debugging

- **Vulkan SDK** -- includes validation layers, `vulkaninfo`, and `vkconfig`
- **RenderDoc** -- frame capture and GPU debugging (https://renderdoc.org/)
- **Nsight Graphics** -- NVIDIA GPU profiling and debugging (Linux/Windows)

### Build Output

After a successful build, the output structure is:

```
build/
  bin/
    Release/          # Windows (MSVC)
      EnjinEditor.exe
      EnjinPlayer.exe
    EnjinEditor       # Linux/macOS
    EnjinPlayer       # Linux/macOS
  lib/
    Release/          # Windows
      EnjinCore.lib
      EnjinEngine.lib
    libEnjinCore.a    # Linux/macOS
    libEnjinEngine.a  # Linux/macOS
```

## 10. Distribution / Installer

### Windows -- Inno Setup (recommended)

The primary Windows installer is built with **Inno Setup 6**. The script is at `installer/EnjinSetup.iss`.

Prerequisites:
- Build the project in Release first (`cd build && cmake --build . --config Release`)
- Install [Inno Setup 6](https://jrsoftware.org/isinfo.php)

Build the installer from the project root:

```cmd
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\EnjinSetup.iss
```

Or open `installer/EnjinSetup.iss` in the Inno Setup Compiler GUI and click Compile.

The output installer is written to `installer/output/TEGESetup-<version>.exe`. It includes:
- Editor and Player executables
- Compiled shaders (.spv)
- Script templates
- Documentation
- `.enjin` file association (registered in HKCU)
- Desktop and Start Menu shortcuts

### CPack (cross-platform)

CMake also ships CPack configuration for archive and package generation:

```bash
cd build
cmake --build . --config Release
cpack -G ZIP           # Windows: ZIP archive
cpack -G NSIS          # Windows: NSIS installer (alternative to Inno Setup)
cpack -G TGZ           # Linux/macOS: tar.gz archive
cpack -G DEB           # Linux: .deb package
```

The CPack/NSIS installer is a secondary option. For polished Windows distribution, prefer the Inno Setup installer above.

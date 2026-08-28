#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <vector>

// Forward declarations for ECS integration
namespace Enjin { namespace ECS {
    class World;
    using Entity = u64;
    struct MeshComponent;
    struct MaterialComponent;
} }

namespace Enjin {
namespace Effects {

// Water rendering style (era-appropriate techniques)
enum class WaterStyle : u8 {
    Flat,           // Solid color (very retro)
    Animated,       // UV scrolling texture (SNES style)
    VertexWave,     // Vertex displacement (PS1/N64)
    Reflective,     // Simple planar reflection (PS2/GameCube)
    Refractive      // Refraction + reflection (late PS2)
};

// 3D Water plane settings
struct Water3DSettings {
    // Position and size
    Math::Vector3 position = Math::Vector3(0, 0, 0);
    f32 width = 100.0f;
    f32 depth = 100.0f;
    f32 tileSize = 2.0f;        // Mesh tessellation

    // Visual style. Shared palette: teal shallow -> dark navy deep, keeping
    // B > G > R so water reads as water even in retro palette mode.
    // See docs/art/PROCEDURAL_EFFECTS_DIRECTION.md.
    WaterStyle style = WaterStyle::VertexWave;
    Math::Vector3 shallowColor = Math::Vector3(0.15f, 0.42f, 0.55f);  // teal
    Math::Vector3 deepColor = Math::Vector3(0.04f, 0.12f, 0.28f);     // dark navy
    f32 opacity = 0.7f;

    // Wave animation (vertex displacement)
    f32 waveSpeed = 1.0f;
    f32 waveHeight = 0.3f;
    f32 waveFrequency = 0.5f;
    Math::Vector2 waveDirection = Math::Vector2(1.0f, 0.5f);

    // Gerstner (trochoidal) waves: vertices also displace HORIZONTALLY toward
    // crests, giving sharp peaks and flat troughs instead of rounded sine
    // swells. Rides the same speed/height/frequency/direction parameters.
    bool gerstnerWaves = false;
    f32 waveSteepness = 0.6f;   // 0 = plain sine look, 1 = maximum crest sharpness

    // UV scrolling (texture animation)
    Math::Vector2 uvScrollSpeed = Math::Vector2(0.02f, 0.01f);

    // Reflection/refraction
    f32 reflectionStrength = 0.5f;
    f32 fresnelPower = 2.0f;     // Edge reflection strength

    // Foam at edges (optional)
    bool enableFoam = false;
    f32 foamThreshold = 0.5f;
    f32 foamScale = 1.0f;

    // Buoyancy: dynamic rigidbodies below the surface within the plane's footprint get
    // pushed up so they float. On by default so floating works out of the box.
    bool enableBuoyancy = true;
    f32 buoyancyStrength = 1.6f;   // >1 = floats
    f32 buoyancyDrag = 2.5f;       // water resistance
};

// Lightweight 3D water system
class ENJIN_API Water3D {
public:
    Water3D() = default;
    ~Water3D() = default;

    void Initialize(const Water3DSettings& settings);
    void Update(f32 deltaTime);

    // Get settings for modification
    Water3DSettings& GetSettings() { return m_Settings; }
    const Water3DSettings& GetSettings() const { return m_Settings; }

    // Get current wave offset for shader
    f32 GetWaveTime() const { return m_WaveTime; }
    Math::Vector2 GetUVOffset() const { return m_UVOffset; }

    // For vertex wave calculation in shader or CPU.
    // With gerstnerWaves on, this returns the trochoidal surface's vertical
    // component at (x, z) — an approximation that keeps buoyancy and the
    // Water_GetWaveHeight VS node bobbing at the visual surface.
    f32 GetWaveHeight(f32 x, f32 z) const;

    // Full Gerstner displacement (horizontal + vertical) at grid point (x, z).
    // Only meaningful when settings.gerstnerWaves is true.
    Math::Vector3 GetGerstnerOffset(f32 x, f32 z) const;

    // Reflection matrix (for planar reflection)
    Math::Matrix4 GetReflectionMatrix() const;

    // Get mesh data (for rendering)
    // Returns vertex positions for a water plane grid
    void GenerateMesh(std::vector<Math::Vector3>& positions,
                      std::vector<Math::Vector2>& uvs,
                      std::vector<u32>& indices) const;

    // ECS Integration — build and update water entity mesh for Vulkan rendering
    // Creates a MeshComponent + MaterialComponent on the given entity
    void BuildEntityMesh(ECS::World* world, ECS::Entity entity) const;

    // Update existing entity's MeshComponent with current wave animation
    // Call each frame after Update() to animate vertex positions
    void UpdateEntityMesh(ECS::World* world, ECS::Entity entity) const;

private:
    Water3DSettings m_Settings;
    f32 m_WaveTime = 0.0f;
    Math::Vector2 m_UVOffset;
};

// 2D Water for side-scrollers and top-down games
struct Water2DSettings {
    // Area
    Math::Vector2 position;
    Math::Vector2 size;

    // Visual
    Math::Vector3 color = Math::Vector3(0.1f, 0.3f, 0.6f);
    f32 opacity = 0.6f;

    // Animation style
    bool enableWaveTop = true;      // Wavy top edge
    f32 waveAmplitude = 4.0f;       // Pixels
    f32 waveFrequency = 0.1f;
    f32 waveSpeed = 2.0f;

    // Reflection (flip sprites above water)
    bool enableReflection = true;
    f32 reflectionOpacity = 0.3f;
    f32 reflectionDistortion = 2.0f;  // Pixels of wobble

    // Parallax scrolling layers (very SNES)
    u32 parallaxLayers = 2;
    f32 parallaxSpeed = 0.5f;

    // Caustics (light patterns on floor underwater)
    bool enableCaustics = false;
    f32 causticsScale = 1.0f;
    f32 causticsSpeed = 1.0f;
};

class ENJIN_API Water2D {
public:
    void Initialize(const Water2DSettings& settings);
    void Update(f32 deltaTime);

    Water2DSettings& GetSettings() { return m_Settings; }
    const Water2DSettings& GetSettings() const { return m_Settings; }

    // Get wave offset at X position (for wavy top edge)
    f32 GetWaveOffset(f32 x) const;

    // Get reflection UV offset (for sprite distortion)
    f32 GetReflectionOffset(f32 x, f32 y) const;

    // Get current animation time
    f32 GetTime() const { return m_Time; }

    // Check if point is underwater
    bool IsUnderwater(const Math::Vector2& point) const;

private:
    Water2DSettings m_Settings;
    f32 m_Time = 0.0f;
};

// Splash effect (for both 2D and 3D)
struct SplashSettings {
    Math::Vector3 position;
    f32 size = 1.0f;
    f32 duration = 0.5f;
    u32 particleCount = 10;
    Math::Vector3 color = Math::Vector3(0.8f, 0.9f, 1.0f);
};

// Ripple effect (expanding ring)
struct RippleSettings {
    Math::Vector3 position;
    f32 maxRadius = 5.0f;
    f32 duration = 2.0f;
    f32 amplitude = 0.1f;
};

// Water interaction manager
class ENJIN_API WaterInteraction {
public:
    void SpawnSplash(const SplashSettings& settings);
    void SpawnRipple(const RippleSettings& settings);

    void Update(f32 deltaTime);

    // Get active ripples for water shader
    struct ActiveRipple {
        Math::Vector3 position;
        f32 currentRadius;
        f32 amplitude;
        f32 progress;  // 0 to 1
    };

    const std::vector<ActiveRipple>& GetRipples() const { return m_Ripples; }

private:
    std::vector<ActiveRipple> m_Ripples;
    // Splash particles would be handled by particle system
};

} // namespace Effects
} // namespace Enjin

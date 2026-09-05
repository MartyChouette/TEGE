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

    // Wind. Water had none: waveDirection and waveHeight were authored
    // constants, so a storm rolled in and the sea kept its calm-day swell
    // heading the way the author happened to point it. Meanwhile WindSystem
    // already carries a direction and a weather-driven strength that the grass
    // and the cloth both read.
    //
    // 0 leaves the authored values exactly as they were, so every existing
    // scene is unchanged. Above 0 the waves turn toward the wind and grow with
    // it, blended by this amount.
    f32 windInfluence = 0.0f;      // 0 = authored waves only, 1 = fully wind-driven
    // What "fully wind-driven" means, so a calm day is not glassy and a gale
    // is not infinite.
    f32 windWaveHeightScale = 1.5f;   // wave height at full wind strength
    f32 windWaveSpeedScale = 1.5f;    // wave speed at full wind strength

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

    // The wind this water is sitting in, from WindSystem. Direction is a world
    // vector (Y ignored); strength is the same 0..1-ish scale the grass reads.
    //
    // Set every frame by whichever runtime is ticking the water, so a gust
    // reaches the surface at the same moment it reaches the grass.
    void SetWind(const Math::Vector3& direction, f32 strength);

    // The settings after wind is folded in -- what the surface is ACTUALLY
    // being built from this frame. Buoyancy has to sample the same numbers the
    // mesh was generated with, or a boat floats to a surface that is not there.
    const Water3DSettings& GetEffectiveSettings() const { return m_Effective; }

    // Surface height at (x, z) for a given settings + wave clock, with no
    // Water3D instance involved.
    //
    // Physics needs this: buoyancy floated things at a FLAT plane -- the water
    // entity's Y -- while the visible surface waved, so a boat sat at the mean
    // level and the swell passed straight through it. The physics backend
    // cannot call Water3D (it does not know the type, and the water is ticked
    // by the runtime, not by physics), so the water publishes its settings and
    // clock onto the component and physics samples them through here.
    //
    // Same function the instance method uses, so the two cannot drift.
    static f32 SampleWaveHeight(const Water3DSettings& settings, f32 waveTime, f32 x, f32 z);

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
                      std::vector<u32>& indices,
                      std::vector<Math::Vector3>* outNormals = nullptr) const;

    // Gerstner position AND normal in one pass.
    //
    // The normal used to come from four EXTRA GetGerstnerOffset calls per vertex
    // (sampling left/right/up/down and crossing the tangents), which made every
    // vertex cost five evaluations - thirty sin/cos - when the surface
    // derivative is closed form and shares the sines the position already
    // computes. On a 161x161 grid that was 777,000 trig calls per frame and it
    // was 96% of the frame time.
    void GerstnerSurface(f32 x, f32 z, Math::Vector3& outOffset,
                         Math::Vector3& outNormal) const;

    // ECS Integration — build and update water entity mesh for Vulkan rendering
    // Creates a MeshComponent + MaterialComponent on the given entity
    void BuildEntityMesh(ECS::World* world, ECS::Entity entity) const;

    // Update existing entity's MeshComponent with current wave animation
    // Call each frame after Update() to animate vertex positions
    void UpdateEntityMesh(ECS::World* world, ECS::Entity entity) const;

private:
    Water3DSettings m_Settings;
    // m_Settings with wind folded in. Every wave computation reads THIS, so the
    // mesh, the height query and buoyancy cannot disagree about what the
    // surface is doing this frame.
    Water3DSettings m_Effective;
    void RecomputeEffectiveSettings();
    Math::Vector3 m_WindDirection = Math::Vector3(1.0f, 0.0f, 0.0f);
    f32 m_WindStrength = 0.0f;
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

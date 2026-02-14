#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include <vector>
#include <string>

namespace Enjin {
namespace Effects {

// Types of screen-space distortion effects
enum class DistortionType : u8 {
    None,
    HeatHaze,       // Rising wavy distortion
    Shockwave,      // Expanding ring distortion
    Underwater,     // Wavy full-screen caustic distortion
    PortalEdge,     // Radial distortion near a portal
    Ripple,         // Concentric ripples from a point
    BarrelFisheye,  // Lens distortion
    Custom          // User distortion map
};

// Per-entity distortion emitter component
struct DistortionSourceComponent {
    DistortionType type = DistortionType::None;
    f32 strength = 0.02f;       // UV displacement magnitude
    f32 radius = 5.0f;          // World-space effect radius
    f32 frequency = 3.0f;       // Wave frequency
    f32 speed = 1.0f;           // Animation speed
    f32 falloff = 2.0f;         // Distance falloff exponent
    f32 phase = 0.0f;           // Animation phase offset

    // Shockwave-specific
    f32 shockwaveRadius = 0.0f; // Current expanding radius (animated)
    f32 shockwaveSpeed = 10.0f; // Expansion speed (units/sec)
    f32 shockwaveWidth = 0.5f;  // Ring width
    f32 shockwaveLife = 0.0f;   // Current lifetime
    f32 shockwaveMaxLife = 1.5f; // Auto-destroy time

    // Texture-based distortion
    std::string distortionMapPath; // Normal map used as UV offset
};

// Global distortion configuration
struct DistortionConfig {
    f32 globalStrength = 1.0f;     // Master strength multiplier
    bool enabled = true;
    i32 fieldDownscale = 2;        // Compute distortion at half/quarter res
    f32 maxDisplacement = 0.1f;    // Clamp max UV displacement
};

// Result of computing the distortion field: a 2D grid of UV offsets
struct DistortionField {
    std::vector<Math::Vector2> offsets; // UV offset per pixel (fieldWidth * fieldHeight)
    i32 width = 0;
    i32 height = 0;

    void Resize(i32 w, i32 h) {
        width = w;
        height = h;
        offsets.resize(static_cast<usize>(w) * static_cast<usize>(h), Math::Vector2(0.0f, 0.0f));
    }

    void Clear() {
        for (auto& o : offsets) {
            o.x = 0.0f;
            o.y = 0.0f;
        }
    }

    Math::Vector2& At(i32 x, i32 y) {
        return offsets[static_cast<usize>(y) * static_cast<usize>(width) + static_cast<usize>(x)];
    }

    const Math::Vector2& At(i32 x, i32 y) const {
        return offsets[static_cast<usize>(y) * static_cast<usize>(width) + static_cast<usize>(x)];
    }
};

// Simple RGBA pixel buffer for CPU-side distortion application
struct PixelBuffer {
    std::vector<Math::Vector4> pixels; // RGBA per pixel
    i32 width = 0;
    i32 height = 0;

    void Resize(i32 w, i32 h) {
        width = w;
        height = h;
        pixels.resize(static_cast<usize>(w) * static_cast<usize>(h), Math::Vector4(0.0f));
    }

    Math::Vector4& At(i32 x, i32 y) {
        return pixels[static_cast<usize>(y) * static_cast<usize>(width) + static_cast<usize>(x)];
    }

    const Math::Vector4& At(i32 x, i32 y) const {
        return pixels[static_cast<usize>(y) * static_cast<usize>(width) + static_cast<usize>(x)];
    }
};

// Camera info needed for world-to-screen projection
struct DistortionCameraInfo {
    Math::Matrix4 viewProjection;
    Math::Vector3 position;
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;
    i32 screenWidth = 1920;
    i32 screenHeight = 1080;
};

// Screen-space distortion system
// Processes all DistortionSourceComponent entities and composites a distortion field
class ENJIN_API ScreenDistortionSystem {
public:
    ScreenDistortionSystem() = default;
    ~ScreenDistortionSystem() = default;

    // Update animation timers, expand shockwaves, remove expired effects
    void Update(ECS::World* world, f32 deltaTime);

    // Compute the distortion field from all active distortion sources
    // Returns a DistortionField with per-pixel UV offsets at (possibly downscaled) resolution
    DistortionField ComputeDistortionField(ECS::World* world,
                                           const DistortionCameraInfo& camera,
                                           i32 width, i32 height);

    // Apply the distortion field to remap source pixels into output pixels
    // Uses bilinear sampling with edge clamping
    static void ApplyDistortion(const PixelBuffer& source,
                                const DistortionField& field,
                                PixelBuffer& output);

    // Trigger a one-shot shockwave at a world position
    // Creates a temporary entity with DistortionSourceComponent (Shockwave type)
    void TriggerShockwave(ECS::World* world,
                          const Math::Vector3& position,
                          f32 strength = 0.05f,
                          f32 speed = 10.0f,
                          f32 width = 0.5f);

    // Configuration
    void SetConfig(const DistortionConfig& config) { m_Config = config; }
    DistortionConfig& GetConfig() { return m_Config; }
    const DistortionConfig& GetConfig() const { return m_Config; }

    // Stats
    u32 GetActiveSourceCount() const { return m_ActiveSourceCount; }

private:
    // Project a world position to normalized screen coordinates [0,1]
    // Returns false if the position is behind the camera
    static bool ProjectToScreen(const Math::Vector3& worldPos,
                                const DistortionCameraInfo& camera,
                                Math::Vector2& screenPos);

    // Compute per-pixel UV offset for a specific distortion type
    static Math::Vector2 ComputeDistortionOffset(DistortionType type,
                                                  const Math::Vector2& pixelUV,
                                                  const Math::Vector2& sourceScreenPos,
                                                  f32 normalizedDist,
                                                  f32 worldDist,
                                                  const DistortionSourceComponent& source,
                                                  f32 time);

    // Bilinear sample from a pixel buffer
    static Math::Vector4 SampleBilinear(const PixelBuffer& buffer, f32 u, f32 v);

    DistortionConfig m_Config;
    f32 m_Time = 0.0f;
    u32 m_ActiveSourceCount = 0;

    // Reusable field to avoid per-frame allocation
    DistortionField m_CachedField;
};

} // namespace Effects
} // namespace Enjin

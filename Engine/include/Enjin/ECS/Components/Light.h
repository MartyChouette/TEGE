#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Light types
enum class LightType : u32 {
    Directional = 0,
    Point = 1,
    Spot = 2
};

// Light component - attach to entities to create light sources
struct ENJIN_API LightComponent {
    LightType type = LightType::Point;

    // Color and intensity
    Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 intensity = 1.0f;

    // Point/Spot light attenuation
    f32 range = 10.0f;           // Maximum range
    f32 constantAttenuation = 1.0f;
    f32 linearAttenuation = 0.09f;
    f32 quadraticAttenuation = 0.032f;

    // Spot light specific
    f32 innerConeAngle = 12.5f;  // Degrees
    f32 outerConeAngle = 17.5f;  // Degrees

    // Shadow casting
    bool castShadows = true;

    // Helper to calculate attenuation at a distance
    f32 CalculateAttenuation(f32 distance) const {
        if (distance > range) return 0.0f;
        return 1.0f / (constantAttenuation +
                       linearAttenuation * distance +
                       quadraticAttenuation * distance * distance);
    }
};

// Maximum lights supported in a single draw
constexpr u32 MAX_DIRECTIONAL_LIGHTS = 4;
constexpr u32 MAX_POINT_LIGHTS = 64;
constexpr u32 MAX_SPOT_LIGHTS = 32;

// Maximum shadow-casting point/spot lights (cubemap faces are expensive)
constexpr u32 MAX_SHADOW_POINT_LIGHTS = 4;
constexpr u32 MAX_SHADOW_SPOT_LIGHTS = 4;

// GPU-side directional light data (aligned for UBO)
struct alignas(16) DirectionalLightData {
    Math::Vector3 direction;
    f32 _pad0;
    Math::Vector3 color;
    f32 intensity;
};

// GPU-side point light data (aligned for UBO)
struct alignas(16) PointLightData {
    Math::Vector3 position;
    f32 range;
    Math::Vector3 color;
    f32 intensity;
    f32 constantAttenuation;
    f32 linearAttenuation;
    f32 quadraticAttenuation;
    f32 _pad0;
};

// GPU-side spot light data (aligned for UBO)
struct alignas(16) SpotLightData {
    Math::Vector3 position;
    f32 range;
    Math::Vector3 direction;
    f32 intensity;
    Math::Vector3 color;
    f32 innerCutoff;  // cos(innerConeAngle)
    f32 outerCutoff;  // cos(outerConeAngle)
    f32 constantAttenuation;
    f32 linearAttenuation;
    f32 quadraticAttenuation;
};

// GPU shadow data SSBO for point/spot light shadow maps (binding 12)
struct alignas(16) ShadowDataSSBO {
    Math::Matrix4 pointFaceViewProj[MAX_SHADOW_POINT_LIGHTS * 6]; // 24 matrices
    Math::Vector4 pointLightParams[MAX_SHADOW_POINT_LIGHTS];       // xyz=pos, w=range
    Math::Matrix4 spotViewProj[MAX_SHADOW_SPOT_LIGHTS];             // 4 matrices
    i32 pointShadowCount;
    i32 spotShadowCount;
    i32 _pad[2];
};

// Lighting uniform buffer object
struct alignas(16) LightingUBO {
    // Scene ambient
    Math::Vector3 ambientColor;
    f32 ambientIntensity;

    // Camera position (for specular calculations)
    Math::Vector3 cameraPosition;
    f32 _pad0;

    // Light counts
    u32 directionalLightCount;
    u32 pointLightCount;
    u32 spotLightCount;
    u32 _pad1;

    // Cascaded shadow mapping data
    Math::Matrix4 cascadeViewProj[4];  // Per-cascade light view-projection matrices
    Math::Vector4 cascadeSplits;       // View-space far distance of each cascade (x,y,z,w)
    f32 shadowSoftness;                // 0 = hard (3x3 PCF), >0 = soft (Poisson disk radius in texels)
    i32 shadowEnabled;                 // 1 = shadows enabled
    f32 shadowStrength;                // 0..1 shadow strength (0 = no shadow, 1 = full)
    f32 shadowMaxDistance;             // Maximum shadow distance (for fade-out)

    // Point/spot shadow counts (how many UBO lights at indices 0..N-1 have shadow maps)
    i32 pointShadowCount;
    i32 spotShadowCount;
    f32 celDiffuseBands;       // 0 = disabled, 2-8 = number of quantized bands
    f32 celSpecularCutoff;     // 0 = disabled, >0 = hard cutoff threshold for specular highlights

    // Wind data for vegetation/weather shaders
    alignas(16) Math::Vector4 windData;  // xyz = wind direction * strength, w = time

    // Fog parameters
    alignas(16) Math::Vector4 fogParams;     // x=density, y=start, z=end, w=heightFalloff
    alignas(16) Math::Vector4 fogColorSnow;  // xyz=fog color, w=snow intensity

    // Player position for vegetation stepping (xyz = world pos, w = step radius)
    alignas(16) Math::Vector4 playerPosition;

    // World curvature (x = strength, yzw reserved)
    alignas(16) Math::Vector4 worldCurvature;

    // Sky reflection color for water/ice surfaces (xyz = color, w = reserved)
    alignas(16) Math::Vector4 skyReflectColor;

    // SH light probe irradiance (xyz = RGB irradiance from nearest probe, w = blend weight 0 or 1)
    alignas(16) Math::Vector4 shProbeIrradiance;

    // Light data arrays
    DirectionalLightData directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLightData pointLights[MAX_POINT_LIGHTS];
    SpotLightData spotLights[MAX_SPOT_LIGHTS];
};

} // namespace ECS
} // namespace Enjin

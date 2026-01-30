#pragma once

#include "Enjin/Platform/Platform.h"
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
    bool castShadows = false;
    u32 shadowMapResolution = 1024;

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

    // Shadow mapping data
    Math::Matrix4 lightSpaceMatrix;  // Light view-projection matrix
    f32 shadowBias;                  // Depth bias for shadow acne
    i32 shadowEnabled;               // 1 = shadows enabled
    f32 _shadowPad[2];               // Padding for alignment

    // Light data arrays
    DirectionalLightData directionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLightData pointLights[MAX_POINT_LIGHTS];
    SpotLightData spotLights[MAX_SPOT_LIGHTS];
};

} // namespace ECS
} // namespace Enjin

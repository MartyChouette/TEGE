#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"

namespace Enjin {
namespace Renderer {

// Uniform buffer object for view/projection matrices (shared across all objects)
struct UniformBufferObject {
    alignas(16) Math::Matrix4 view;
    alignas(16) Math::Matrix4 proj;
    alignas(16) Math::Matrix4 prevViewProj;  // Previous frame view*proj for velocity
    alignas(16) Math::Vector4 jitterOffset;  // xy = current jitter (NDC), zw = previous jitter
};

// Push constants for per-object data (model matrix + material)
// 128 bytes max. Vulkan uses native push constants, WebGPU/Metal use a reserved UBO.
struct PushConstants {
    alignas(16) Math::Matrix4 model;
    // Material data (must match fragment shader)
    alignas(16) Math::Vector3 baseColor;
    f32 metallic;
    alignas(16) Math::Vector3 emissiveColor;
    f32 roughness;
    f32 emissiveStrength;
    f32 opacity;
    f32 alphaCutoff;
    i32 flags;
    f32 parallaxScale;
    f32 surfaceParam1 = 0.0f;  // water: shoreWidth | artistic: reflectivity
    f32 surfaceParam2 = 0.0f;  // water: foamIntensity | artistic: fresnelPower
    f32 surfaceParam3 = 0.0f;  // water: foamScale | artistic: rimLightStrength
};

// Note: LightingUBO is defined in Enjin/ECS/Components/Light.h
// Use ECS::LightingUBO for multi-light support

} // namespace Renderer
} // namespace Enjin

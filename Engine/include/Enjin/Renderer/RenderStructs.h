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

// Indirect-draw mode, carried in PushConstants::parallaxScale.
//
// The push-constant block above is exactly 128 bytes -- the Vulkan guaranteed minimum -- and
// full, so the mode rides the one field that has no meaning for an indirect draw: those take
// every per-object value from an SSBO, parallaxScale included. A material's real parallaxScale
// is a height-map depth and is never negative, so the negative half of the range is free.
//
// The two modes read DIFFERENT SSBO bindings, and that is the point. vkUpdateDescriptorSets is
// a HOST operation applied at SUBMIT, so a binding written twice while one command buffer is
// recording holds only its LAST value for every draw in that buffer. Both paths draw into the
// same command buffer -- static indirect early, the bone arena late -- so sharing one binding
// silently fed the arena's transforms and materials to the static indirect draws.
//
// Keep in lockstep with INDIRECT_MODE_* in triangle.vert and triangle.frag.
inline constexpr f32 kIndirectModeStatic = -1.0f;  // ObjectData at binding 24: GPU culling, textured batcher, DGC
inline constexpr f32 kIndirectModeArena  = -2.0f;  // ObjectData at binding 13: bone-arena instanced draws

// A material's parallax height scale and its march step limit, in the one
// float slot that carries parallax (push constants and MaterialGPU are both
// full). Integer part = Max Steps, fraction = Height Scale, which the inspector
// keeps below 1. An unpacked value (integer part 0) decodes as the old fixed
// 32 steps, so a writer that does not pack still draws as it did. A scale of 0
// packs to 0, which the shaders read as "no parallax". Positive, so it never
// meets the negative indirect-mode sentinels above.
inline f32 PackParallax(f32 heightScale, u32 maxSteps) {
    if (!(heightScale > 0.0f)) return 0.0f;
    const f32 scale = heightScale < 0.999f ? heightScale : 0.999f;
    const u32 steps = maxSteps < 1u ? 1u : (maxSteps > 256u ? 256u : maxSteps);
    return static_cast<f32>(steps) + scale;
}

// Note: LightingUBO is defined in Enjin/ECS/Components/Light.h
// Use ECS::LightingUBO for multi-light support

} // namespace Renderer
} // namespace Enjin

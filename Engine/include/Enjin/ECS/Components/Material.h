#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace ECS {

// Material component for surface properties
struct MaterialComponent {
    // Base color (albedo)
    Math::Vector3 baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 opacity = 1.0f;

    // PBR properties
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;

    // Emission
    Math::Vector3 emissiveColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    f32 emissiveStrength = 0.0f;

    // Texture indices (-1 means no texture)
    i32 baseColorTexture = -1;
    i32 normalTexture = -1;
    i32 metallicRoughnessTexture = -1;
    i32 emissiveTexture = -1;

    // Rendering flags
    bool doubleSided = false;
    bool castShadows = true;
    bool receiveShadows = true;

    // Alpha mode
    enum class AlphaMode { Opaque, Mask, Blend } alphaMode = AlphaMode::Opaque;
    f32 alphaCutoff = 0.5f;
};

// GPU-aligned material data for shader upload
struct alignas(16) MaterialGPU {
    alignas(16) Math::Vector3 baseColor;
    alignas(4) f32 metallic;

    alignas(16) Math::Vector3 emissiveColor;
    alignas(4) f32 roughness;

    alignas(4) f32 emissiveStrength;
    alignas(4) f32 opacity;
    alignas(4) f32 alphaCutoff;
    alignas(4) i32 flags; // Bit flags for various settings

    static MaterialGPU FromComponent(const MaterialComponent& mat) {
        MaterialGPU gpu;
        gpu.baseColor = mat.baseColor;
        gpu.metallic = mat.metallic;
        gpu.emissiveColor = mat.emissiveColor;
        gpu.roughness = mat.roughness;
        gpu.emissiveStrength = mat.emissiveStrength;
        gpu.opacity = mat.opacity;
        gpu.alphaCutoff = mat.alphaCutoff;

        gpu.flags = 0;
        if (mat.doubleSided) gpu.flags |= 1;
        if (mat.castShadows) gpu.flags |= 2;
        if (mat.receiveShadows) gpu.flags |= 4;
        gpu.flags |= (static_cast<i32>(mat.alphaMode) << 8);

        return gpu;
    }
};

} // namespace ECS
} // namespace Enjin

#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {

// Forward declaration for texture caching
namespace Renderer { class Texture; }

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

    // Texture indices (-1 means no texture, used internally by render system)
    i32 baseColorTexture = -1;
    i32 normalTexture = -1;
    i32 metallicRoughnessTexture = -1;
    i32 emissiveTexture = -1;

    // Texture file paths (set these to load textures)
    std::string baseColorTexturePath;
    std::string normalTexturePath;
    std::string metallicRoughnessTexturePath;
    std::string emissiveTexturePath;

    // Rendering flags
    bool doubleSided = false;
    bool castShadows = true;
    bool receiveShadows = true;

    // Alpha mode
    enum class AlphaMode { Opaque, Mask, Blend } alphaMode = AlphaMode::Opaque;
    f32 alphaCutoff = 0.5f;

    // Height/parallax mapping
    i32 heightTexture = -1;
    std::string heightTexturePath;
    f32 parallaxScale = 0.05f;

    // Retro rendering flags (per-material)
    bool flatShading = false;
    bool affineTexturing = false;
    bool vertexSnapping = false;
    bool stippleTransparency = false;
    bool uvQuantize = false;
    bool gouraudOnly = false;
    u8 vertexSnapResolution = 160; // PS1-style grid resolution (80-320)

    // Cached texture pointers (mutable - cache only, not part of component state)
    // Avoids per-frame string hash lookups in GetOrLoadTexture
    mutable Renderer::Texture* cachedBaseColorTexture = nullptr;
    mutable Renderer::Texture* cachedHeightTexture = nullptr;
    mutable Renderer::Texture* cachedNormalTexture = nullptr;
    mutable Renderer::Texture* cachedMetallicRoughnessTexture = nullptr;
    mutable Renderer::Texture* cachedEmissiveTexture = nullptr;
    mutable bool textureCacheDirty = true;

    // Call when texture paths change to force re-lookup
    void InvalidateTextureCache() const {
        textureCacheDirty = true;
        cachedBaseColorTexture = nullptr;
        cachedHeightTexture = nullptr;
        cachedNormalTexture = nullptr;
        cachedMetallicRoughnessTexture = nullptr;
        cachedEmissiveTexture = nullptr;
    }
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

        // Texture flags (bits 16-19, bit 10 for height)
        if (mat.baseColorTexture >= 0) gpu.flags |= (1 << 16);
        if (mat.normalTexture >= 0) gpu.flags |= (1 << 17);
        if (mat.metallicRoughnessTexture >= 0) gpu.flags |= (1 << 18);
        if (mat.emissiveTexture >= 0) gpu.flags |= (1 << 19);
        if (mat.heightTexture >= 0) gpu.flags |= (1 << 10);

        // Retro flags (bits 20-23)
        if (mat.flatShading) gpu.flags |= (1 << 20);
        if (mat.affineTexturing) gpu.flags |= (1 << 21);
        if (mat.vertexSnapping) gpu.flags |= (1 << 22);
        if (mat.stippleTransparency) gpu.flags |= (1 << 23);
        if (mat.uvQuantize) gpu.flags |= (1 << 12);
        if (mat.gouraudOnly) gpu.flags |= (1 << 13);
        // Vertex snap resolution packed into bits 24-31
        gpu.flags |= (static_cast<i32>(mat.vertexSnapResolution) << 24);

        return gpu;
    }

    // Texture flag bit positions
    static constexpr i32 FLAG_HAS_BASE_COLOR_TEXTURE = (1 << 16);
    static constexpr i32 FLAG_HAS_NORMAL_TEXTURE = (1 << 17);
    static constexpr i32 FLAG_HAS_METALLIC_ROUGHNESS_TEXTURE = (1 << 18);
    static constexpr i32 FLAG_HAS_EMISSIVE_TEXTURE = (1 << 19);
    static constexpr i32 FLAG_HAS_HEIGHT_TEXTURE = (1 << 10);

    // Retro flag bit positions
    static constexpr i32 FLAG_FLAT_SHADING = (1 << 20);
    static constexpr i32 FLAG_AFFINE_TEXTURING = (1 << 21);
    static constexpr i32 FLAG_VERTEX_SNAPPING = (1 << 22);
    static constexpr i32 FLAG_STIPPLE_TRANSPARENCY = (1 << 23);
};

} // namespace ECS
} // namespace Enjin

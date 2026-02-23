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
    u32 parallaxMode = 0;       // 0=Basic, 1=Steep, 2=OcclusionMapping, 3=ReliefMapping
    u32 pomMaxSteps = 32;       // Max ray-march steps for POM modes
    f32 pomHeightScale = 0.05f; // Height scale for POM

    // Retro rendering flags (per-material)
    bool flatShading = false;
    bool affineTexturing = false;
    bool vertexSnapping = false;
    bool stippleTransparency = false;
    bool uvQuantize = false;
    bool gouraudOnly = false;
    u8 vertexSnapResolution = 160; // PS1-style grid resolution (80-320)

    // Shadow dither mode: 0=None, 1=By Darkness, 2=By Distance, 3=By Angle
    u8 shadowDitherMode = 0;
    // Shadow dither pattern: 0=Bayer4x4, 1=Bayer8x8, 2=BlueNoise, 3=Halftone, 4=Crosshatch, 5=Overlook
    u8 shadowDitherPattern = 0;

    // Artistic surface controls (reuse water push constant slots for non-water entities)
    f32 reflectivity = 0.0f;      // 0-1: environment reflection strength
    f32 fresnelPower = 5.0f;      // 0.5-10: edge vs center reflection falloff
    f32 rimLightStrength = 0.0f;  // 0-3: additive rim/edge glow

    // Cel shading opt-out (per-material)
    bool excludeFromCelShading = false;

    // Transmission (glass, water, thin surfaces)
    f32 transmission = 0.0f;        // 0=opaque, 1=fully transmissive
    f32 ior = 1.5f;                 // Index of refraction (1.0=vacuum, 1.33=water, 1.5=glass, 2.42=diamond)
    f32 thickness = 0.0f;           // Thin-surface thickness for translucency falloff (0=solid)

    // Subsurface Scattering
    f32 sssIntensity = 0.0f;        // 0=off, >0=SSS strength
    f32 sssRadius = 1.0f;           // Scatter radius in world units
    Math::Vector3 sssColor = Math::Vector3(1.0f, 0.2f, 0.1f); // Scatter tint (skin/wax default)

    // Dithered gradient rendering (flat shading + banded lighting with dither transitions)
    bool ditherGradient = false;
    u8 ditherGradientBands = 4;        // 2-8 color quantization bands
    u8 ditherGradientPattern = 0;      // 0=Bayer4x4, 1=Bayer8x8, 2=BlueNoise, 3=Halftone, 4=Crosshatch, 5=Overlook

    // Dithered transparency (CRT-style: alternating pixels between original and blend color,
    // naturally blurred by phosphor bloom in post-process for perceived transparency)
    bool ditherTransparency = false;
    u8 ditherTransPattern = 0;  // 0=Checkerboard, 1=HStripe, 2=VStripe, 3=Bayer2x2
    Math::Vector3 ditherTransBlendColor = Math::Vector3(0.7f, 0.85f, 1.0f); // light blue default
    f32 ditherTransOpacity = 0.5f; // 0=all blend color, 1=all original, 0.5=even mix

    // Lightweight key for comparing/sorting texture combinations by pointer identity.
    // Texture cache guarantees pointer stability, so pointer comparison is sufficient.
    struct TextureKey {
        Renderer::Texture* baseColor = nullptr;
        Renderer::Texture* height = nullptr;
        Renderer::Texture* normal = nullptr;
        Renderer::Texture* metallicRoughness = nullptr;
        Renderer::Texture* emissive = nullptr;

        bool operator==(const TextureKey& o) const {
            return baseColor == o.baseColor && height == o.height && normal == o.normal
                && metallicRoughness == o.metallicRoughness && emissive == o.emissive;
        }
        bool operator!=(const TextureKey& o) const { return !(*this == o); }
        bool operator<(const TextureKey& o) const {
            if (baseColor != o.baseColor) return baseColor < o.baseColor;
            if (height != o.height) return height < o.height;
            if (normal != o.normal) return normal < o.normal;
            if (metallicRoughness != o.metallicRoughness) return metallicRoughness < o.metallicRoughness;
            return emissive < o.emissive;
        }
    };

    // Cached texture pointers (mutable - cache only, not part of component state)
    // Avoids per-frame string hash lookups in GetOrLoadTexture
    mutable Renderer::Texture* cachedBaseColorTexture = nullptr;
    mutable Renderer::Texture* cachedHeightTexture = nullptr;
    mutable Renderer::Texture* cachedNormalTexture = nullptr;
    mutable Renderer::Texture* cachedMetallicRoughnessTexture = nullptr;
    mutable Renderer::Texture* cachedEmissiveTexture = nullptr;
    mutable TextureKey cachedTextureKey;  // Updated when textureCacheDirty clears
    mutable bool textureCacheDirty = true;

    // Call when texture paths change to force re-lookup
    void InvalidateTextureCache() const {
        textureCacheDirty = true;
        cachedBaseColorTexture = nullptr;
        cachedHeightTexture = nullptr;
        cachedNormalTexture = nullptr;
        cachedMetallicRoughnessTexture = nullptr;
        cachedEmissiveTexture = nullptr;
        cachedTextureKey = {};
    }
};

// GPU-aligned material data for shader upload (80 bytes)
struct alignas(16) MaterialGPU {
    alignas(16) Math::Vector3 baseColor;
    alignas(4) f32 metallic;

    alignas(16) Math::Vector3 emissiveColor;
    alignas(4) f32 roughness;

    alignas(4) f32 emissiveStrength;
    alignas(4) f32 opacity;
    alignas(4) f32 alphaCutoff;
    alignas(4) i32 flags; // Bit flags for various settings

    // Transmission / SSS (new — row 3-4, offset 48)
    alignas(4) f32 transmission;
    alignas(4) f32 ior;
    alignas(4) f32 thickness;
    alignas(4) f32 sssIntensity;

    alignas(16) Math::Vector3 sssColor;
    alignas(4) f32 sssRadius;

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
        if (mat.excludeFromCelShading) gpu.flags |= (1 << 4);
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
        // Shadow dither mode packed into bits 14-15
        gpu.flags |= (static_cast<i32>(mat.shadowDitherMode & 0x3) << 14);
        // Vertex snap resolution packed into bits 24-28 (5 bits, value/8)
        gpu.flags |= (static_cast<i32>((mat.vertexSnapResolution / 8) & 0x1F) << 24);
        // Shadow dither pattern packed into bits 29-31 (3 bits)
        gpu.flags |= (static_cast<i32>(mat.shadowDitherPattern & 0x7) << 29);

        // Dithered gradient: forces flat shading flag.
        // surfaceParam1 encoding happens in PushConstants (RenderSystem) since
        // MaterialGPU doesn't include surfaceParam fields.
        if (mat.ditherGradient) {
            gpu.flags |= (1 << 20);  // Force flat shading
        }

        // Transmission / SSS
        gpu.transmission = mat.transmission;
        gpu.ior = mat.ior;
        gpu.thickness = mat.thickness;
        gpu.sssIntensity = mat.sssIntensity;
        gpu.sssColor = mat.sssColor;
        gpu.sssRadius = mat.sssRadius;

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

    // Dithered gradient: encoded in surfaceParam1 (values > 1.0 = dither gradient mode)
    // surfaceParam1 = 100.0 + bands + pattern * 0.1
};

} // namespace ECS
} // namespace Enjin

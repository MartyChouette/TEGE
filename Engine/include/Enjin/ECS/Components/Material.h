#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {

// Forward declaration for texture caching
namespace Renderer { class Texture; }

namespace ECS {

// Material component for surface properties.
// Field order is optimized for cache locality: hot render-path data (PBR values,
// flags, texture indices) is packed into the first ~3 cache lines (~192 bytes).
// Cold data (std::string texture paths, cached texture pointers, TextureKey) is
// placed at the end so the render loop rarely touches those cache lines.
struct MaterialComponent {
    // ── Cache line 0 (bytes 0-63): PBR core + alpha + texture indices ──

    // Base color (albedo)
    Math::Vector3 baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 opacity = 1.0f;

    // PBR properties
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;

    // Emission
    Math::Vector3 emissiveColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    f32 emissiveStrength = 0.0f;

    // Alpha mode
    enum class AlphaMode { Opaque, Mask, Blend } alphaMode = AlphaMode::Opaque;
    f32 alphaCutoff = 0.5f;

    // Texture indices (-1 means no texture, used internally by render system)
    i32 baseColorTexture = -1;
    i32 normalTexture = -1;
    i32 metallicRoughnessTexture = -1;
    i32 emissiveTexture = -1;

    // ── Cache line 1 (bytes 64-127): flags, retro, artistic, outline ──

    i32 heightTexture = -1;
    i32 matcapTexture = -1;  // Matcap texture index (-1 = no matcap)

    // Rendering flags (packed booleans)
    bool doubleSided = false;
    bool castShadows = true;
    bool receiveShadows = true;
    bool excludeFromCelShading = false;

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

    // Cel shading
    u8 lightRampOverride = 0;  // 0=use global, 1-4=override (smooth/warm/cool/anime)

    // Per-material texture filter override for this material's textures.
    // 0=use global Texture Filtering setting, 1=Point (nearest, pixel-art),
    // 2=Bilinear, 3=Trilinear. Anisotropy/mipmaps/wrap follow the global setting.
    u8 textureFilterOverride = 0;

    // Dithered gradient rendering (flat shading + banded lighting with dither transitions)
    bool ditherGradient = false;
    u8 ditherGradientBands = 4;        // 2-8 color quantization bands
    u8 ditherGradientPattern = 0;      // 0=Bayer4x4, 1=Bayer8x8, 2=BlueNoise, 3=Halftone, 4=Crosshatch, 5=Overlook

    // Dithered transparency (CRT-style: alternating pixels between original and blend color,
    // naturally blurred by phosphor bloom in post-process for perceived transparency)
    bool ditherTransparency = false;
    u8 ditherTransPattern = 0;  // 0=Checkerboard, 1=HStripe, 2=VStripe, 3=Bayer2x2

    // Artistic surface controls (reuse water push constant slots for non-water entities)
    f32 reflectivity = 0.0f;      // 0-1: environment reflection strength
    f32 fresnelPower = 5.0f;      // 0.5-10: edge vs center reflection falloff
    f32 rimLightStrength = 0.0f;  // 0-3: additive rim/edge glow

    // Geometry outline (inverted-hull) — per-material override
    f32 outlineWidth = 0.0f;   // 0=use global, >0=per-material outline thickness (world units)
    Math::Vector3 outlineColor = Math::Vector3(0.0f, 0.0f, 0.0f); // Black default

    // ── Cache line 2 (bytes 128-191): transmission, SSS, surface effects, sort key ──

    // Transmission (glass, water, thin surfaces)
    f32 transmission = 0.0f;        // 0=opaque, 1=fully transmissive
    f32 ior = 1.5f;                 // Index of refraction (1.0=vacuum, 1.33=water, 1.5=glass, 2.42=diamond)
    f32 thickness = 0.0f;           // Thin-surface thickness for translucency falloff (0=solid)

    // Subsurface Scattering
    f32 sssIntensity = 0.0f;        // 0=off, >0=SSS strength
    f32 sssRadius = 1.0f;           // Scatter radius in world units
    Math::Vector3 sssColor = Math::Vector3(1.0f, 0.2f, 0.1f); // Scatter tint (skin/wax default)

    // Procedural surface noise (stylized color variation based on world position)
    f32 surfaceNoiseScale = 0.0f;     // 0=off, >0 = noise frequency in world units (e.g., 2.0-20.0)
    f32 surfaceNoiseStrength = 0.0f;  // 0-1: how much noise darkens/lightens the diffuse color

    Math::Vector3 ditherTransBlendColor = Math::Vector3(0.7f, 0.85f, 1.0f); // light blue default
    f32 ditherTransOpacity = 0.5f; // 0=all blend color, 1=all original, 0.5=even mix

    // Height/parallax mapping
    f32 parallaxScale = 0.05f;
    u32 parallaxMode = 0;       // 0=Basic, 1=Steep, 2=OcclusionMapping, 3=ReliefMapping
    u32 pomMaxSteps = 32;       // Max ray-march steps for POM modes
    f32 pomHeightScale = 0.05f; // Height scale for POM

    // 64-bit material sort key for fast radix-friendly sorting.
    // Layout: [8:pipeline][16:material hash][24:texture hash][16:depth]
    // Pipeline: opaque=0, alpha-mask=1, alpha-blend=2 (ensures correct render order)
    mutable u64 cachedSortKey = 0;

    // Texture cache dirty flag (checked every frame in RenderEntity)
    mutable bool textureCacheDirty = true;

    // ── Cold data (only touched on texture load, serialization, or inspector) ──

    // Texture file paths (set these to load textures)
    std::string baseColorTexturePath;
    std::string normalTexturePath;
    std::string metallicRoughnessTexturePath;
    std::string emissiveTexturePath;
    std::string specularTexturePath;  // Pre-PBR: direct specular intensity map (used when shadingModel != GGX)
    std::string matcapTexturePath;    // Matcap (Material Capture): 2D texture indexed by view-space normal
    std::string heightTexturePath;

    // ── Surface response (TotK-style): the sound + particle this surface makes ──
    // Footstep fires while a character controller walks/runs on an entity with this
    // material; impact fires when the surface is struck by a physics collision. Empty
    // sound = silent. surfaceParticle bursts at the foot / contact point.
    std::string footstepSound;         // per-footstep audio while walked/run on
    std::string impactSound;           // audio when struck by a collision
    // 0=None,1=Dust,2=Grass,3=Spark,4=Splash,5=Smoke,6=Snow (maps to a particle preset)
    u8 surfaceParticle = 0;
    f32 footstepVolume = 1.0f;
    f32 impactThreshold = 3.0f;        // min collision speed (units/s) to fire the impact

    // Cached texture pointers (mutable - cache only, not part of component state)
    // Avoids per-frame string hash lookups in GetOrLoadTexture
    mutable Renderer::Texture* cachedBaseColorTexture = nullptr;
    mutable Renderer::Texture* cachedHeightTexture = nullptr;
    mutable Renderer::Texture* cachedNormalTexture = nullptr;
    mutable Renderer::Texture* cachedMetallicRoughnessTexture = nullptr;
    mutable Renderer::Texture* cachedEmissiveTexture = nullptr;
    mutable Renderer::Texture* cachedMatcapTexture = nullptr;

    // Lightweight key for comparing/sorting texture combinations by pointer identity.
    // Texture cache guarantees pointer stability, so pointer comparison is sufficient.
    struct TextureKey {
        Renderer::Texture* baseColor = nullptr;
        Renderer::Texture* height = nullptr;
        Renderer::Texture* normal = nullptr;
        Renderer::Texture* metallicRoughness = nullptr;
        Renderer::Texture* emissive = nullptr;
        Renderer::Texture* matcap = nullptr;

        bool operator==(const TextureKey& o) const {
            return baseColor == o.baseColor && height == o.height && normal == o.normal
                && metallicRoughness == o.metallicRoughness && emissive == o.emissive
                && matcap == o.matcap;
        }
        bool operator!=(const TextureKey& o) const { return !(*this == o); }
        bool operator<(const TextureKey& o) const {
            if (baseColor != o.baseColor) return baseColor < o.baseColor;
            if (height != o.height) return height < o.height;
            if (normal != o.normal) return normal < o.normal;
            if (metallicRoughness != o.metallicRoughness) return metallicRoughness < o.metallicRoughness;
            if (emissive != o.emissive) return emissive < o.emissive;
            return matcap < o.matcap;
        }
    };
    mutable TextureKey cachedTextureKey;  // Updated when textureCacheDirty clears

    // Compute the sort key for a given camera distance.
    // Call after texture cache is populated. Depth is quantized to 16 bits.
    void ComputeSortKey(f32 depth) const {
        // Pipeline bucket (8 bits)
        u64 pipeline = 0;
        if (alphaMode == AlphaMode::Mask) pipeline = 1;
        else if (alphaMode == AlphaMode::Blend) pipeline = 2;

        // Hash 5 texture pointers into 40 bits (16 material + 24 texture)
        auto ptrHash = [](const void* p) -> u64 {
            u64 v = reinterpret_cast<u64>(p);
            v ^= v >> 16;
            v *= 0x45d9f3b;
            v ^= v >> 16;
            return v;
        };
        u64 texHash = ptrHash(cachedBaseColorTexture)
                    ^ (ptrHash(cachedHeightTexture) * 0x9E3779B97F4A7C15ULL)
                    ^ (ptrHash(cachedNormalTexture) * 0x517CC1B727220A95ULL)
                    ^ (ptrHash(cachedMetallicRoughnessTexture) * 0x6C62272E07BB0142ULL)
                    ^ (ptrHash(cachedEmissiveTexture) * 0x62B821756295C58DULL);
        u64 materialBits = (texHash >> 24) & 0xFFFF;   // 16 bits
        u64 textureBits  = texHash & 0xFFFFFF;          // 24 bits

        // Depth: front-to-back for opaque (minimize overdraw), back-to-front for blend
        u32 depthU16;
        if (pipeline <= 1) {
            // Opaque/mask: front-to-back — smaller depth = lower key = drawn first
            depthU16 = static_cast<u32>(Math::Clamp(depth / 10000.0f, 0.0f, 1.0f) * 65535.0f);
        } else {
            // Blend: back-to-front — larger depth = lower key = drawn first
            depthU16 = 65535u - static_cast<u32>(Math::Clamp(depth / 10000.0f, 0.0f, 1.0f) * 65535.0f);
        }

        cachedSortKey = (pipeline << 56)
                      | (materialBits << 40)
                      | (textureBits << 16)
                      | static_cast<u64>(depthU16);
    }

    // Call when texture paths change to force re-lookup
    void InvalidateTextureCache() const {
        textureCacheDirty = true;
        cachedBaseColorTexture = nullptr;
        cachedHeightTexture = nullptr;
        cachedNormalTexture = nullptr;
        cachedMetallicRoughnessTexture = nullptr;
        cachedEmissiveTexture = nullptr;
        cachedMatcapTexture = nullptr;
        cachedTextureKey = {};
    }
};

// GPU-aligned material data for shader upload (112 bytes — must match the
// material SSBO struct in the shaders and ShaderData.h)
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

    // Bindless texture indices (set 1 binding 0 array index, UINT32_MAX = no texture)
    alignas(4) u32 baseColorTexIdx = UINT32_MAX;
    alignas(4) u32 heightTexIdx = UINT32_MAX;
    alignas(4) u32 normalTexIdx = UINT32_MAX;
    alignas(4) u32 metallicRoughnessTexIdx = UINT32_MAX;
    alignas(4) u32 emissiveTexIdx = UINT32_MAX;
    alignas(4) u32 matcapTexIdx = UINT32_MAX;
    // Sampler table slot (set 1 binding 2): 0=global filter setting, 1=Point,
    // 2=Bilinear, 3=Trilinear. Maps 1:1 from textureFilterOverride, so a
    // per-material filter needs no duplicate texture registrations.
    alignas(4) u32 samplerIdx = 0;
    alignas(4) u32 _bindlessPad = 0;

    static MaterialGPU FromComponent(const MaterialComponent& mat) {
        MaterialGPU gpu;
        gpu.samplerIdx = (mat.textureFilterOverride <= 3u) ? mat.textureFilterOverride : 0u;
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

// Multi-material component for entities with sub-meshes.
// Each slot corresponds to a SubMesh::materialSlot index in MeshComponent.
// When present, the render system draws each sub-mesh with its corresponding
// material slot instead of using the single MaterialComponent.
struct MaterialSlotsComponent {
    std::vector<MaterialComponent> slots;

    // Get a material slot by index (returns nullptr if out of range)
    MaterialComponent* GetSlot(i32 index) {
        if (index < 0 || index >= static_cast<i32>(slots.size())) return nullptr;
        return &slots[index];
    }
    const MaterialComponent* GetSlot(i32 index) const {
        if (index < 0 || index >= static_cast<i32>(slots.size())) return nullptr;
        return &slots[index];
    }
};

} // namespace ECS
} // namespace Enjin

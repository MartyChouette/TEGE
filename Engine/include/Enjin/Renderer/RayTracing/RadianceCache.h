#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// GPU-side tile entry for the screen-space radiance cache (32 bytes, std430).
// Each tile covers tileSize x tileSize pixels and stores cached indirect irradiance.
struct RadianceCacheTile {
    f32 irradianceR;        // Cached indirect irradiance — red channel
    f32 irradianceG;        // Cached indirect irradiance — green channel
    f32 irradianceB;        // Cached indirect irradiance — blue channel
    f32 depth;              // Representative depth (center pixel at cache time)
    f32 normalX;            // Representative surface normal X
    f32 normalY;            // Representative surface normal Y
    f32 normalZ;            // Representative surface normal Z
    f32 age;                // Frames since last full re-trace (0 = just traced)
};

// Radiance cache configuration
struct RadianceCacheConfig {
    bool enabled = false;
    u32 tileSize = 32;              // Pixel tile size (16, 32, or 64)
    f32 maxAge = 8.0f;              // Max frames before cache entry must be re-traced
    f32 depthThreshold = 0.1f;      // Relative depth difference for cache invalidation
    f32 normalThreshold = 0.85f;    // Dot product threshold — below this, tile is stale
    bool excludeDirectional = true; // Keep directional light (sun/moon) out of cache
    f32 hysteresis = 0.9f;          // Temporal blend: 0 = no reuse, 1 = full reuse
};

// Screen-Space Radiance Cache
//
// Divides the screen into tileSize x tileSize pixel tiles and caches indirect
// irradiance (from RT GI) per tile. On subsequent frames, tiles whose depth and
// normal are similar to the current G-buffer reuse cached values instead of
// dispatching expensive GI rays. Stale tiles (age > maxAge or geometry mismatch)
// are marked for re-tracing.
//
// Architecture:
//   1. Update pass (compute): compare tile depth/normal with current G-buffer.
//      If valid, increment age and keep cached irradiance. If stale, mark for
//      re-trace by resetting age and clearing irradiance.
//   2. Read pass (compute): per-pixel bilinear interpolation of tile irradiance.
//      Writes the result into a full-resolution output image that the RT compositor
//      can blend with raw GI output.
//
// IMPORTANT: Directional light (sun/moon) is excluded from the cache by default.
//            It changes with time-of-day and must be evaluated separately via shadow maps.
//
// Dispatch order: after RT GI, before RT compositor.
class ENJIN_API RadianceCache {
public:
    RadianceCache(VulkanContext* context);
    ~RadianceCache();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Run the cache update pass — marks stale tiles and ages valid ones.
    // Must be called after RT GI dispatch.
    void DispatchUpdate(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                        u32 frameCount);

    // Run the cache read pass — interpolates tile irradiance to per-pixel output.
    void DispatchRead(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                      u32 frameCount);

    // Invalidate the entire cache (camera cut, major scene change, etc.)
    void InvalidateAll();

    // Output image for the RT compositor to blend with raw GI
    VkImageView GetOutputView() const { return m_OutputView; }
    VkImage GetOutputImage() const { return m_OutputImage; }

    // Tile buffer access (for descriptor set binding)
    VkBuffer GetTileBuffer() const { return m_TileBuffer; }
    u32 GetTileCountX() const { return m_TileCountX; }
    u32 GetTileCountY() const { return m_TileCountY; }
    u32 GetTotalTileCount() const { return m_TileCountX * m_TileCountY; }

    // Stale tile mask — 1 bit per tile, set if tile needs re-tracing.
    // GI system can read this to skip rays for cached tiles.
    VkBuffer GetStaleMaskBuffer() const { return m_StaleMaskBuffer; }

    RadianceCacheConfig& GetConfig() { return m_Config; }
    const RadianceCacheConfig& GetConfig() const { return m_Config; }

private:
    void CreateResources();
    void DestroyResources();
    void CreateOutputImage();
    void DestroyOutputImage();

    VulkanContext* m_Context = nullptr;
    RadianceCacheConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_TileCountX = 0;
    u32 m_TileCountY = 0;

    // Update compute pipeline — validates tiles, ages cache
    VkPipeline m_UpdatePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_UpdatePipelineLayout = VK_NULL_HANDLE;

    // Read compute pipeline — per-pixel interpolation from tile cache
    VkPipeline m_ReadPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_ReadPipelineLayout = VK_NULL_HANDLE;

    // Tile buffer (RadianceCacheTile[tileCountX * tileCountY])
    // Binding 21 in RT descriptor set
    VkBuffer m_TileBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_TileMemory = VK_NULL_HANDLE;

    // Stale tile mask buffer — u32 bitfield array, 1 bit per tile
    // Binding 22 in RT descriptor set
    VkBuffer m_StaleMaskBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_StaleMaskMemory = VK_NULL_HANDLE;

    // Full-resolution output image (interpolated irradiance)
    VkImage m_OutputImage = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputMemory = VK_NULL_HANDLE;
    VkImageView m_OutputView = VK_NULL_HANDLE;

    bool m_Initialized = false;
    bool m_InvalidateRequested = false;
};

} // namespace Renderer
} // namespace Enjin

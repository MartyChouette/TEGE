#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// A tile in the shadow atlas — represents one shadow map (cascade face, spot light, etc.)
struct ShadowTile {
    u32 x = 0;              // Pixel offset in atlas
    u32 y = 0;
    u32 size = 0;           // Tile resolution (square)
    bool allocated = false;

    // UV transform for shader sampling: uv = worldUV * scale + offset
    Math::Vector2 uvOffset() const;
    Math::Vector2 uvScale() const;
};

// Shadow atlas tile allocation request
struct ShadowTileRequest {
    u32 lightIndex = 0;      // Light entity or index
    u32 faceIndex = 0;       // Cubemap face (0-5) or cascade index
    u32 requestedSize = 1024; // Desired tile resolution
    f32 priority = 0.0f;     // Higher = more important (from shadow budget)
};

// Per-light shadow info passed to shaders via clustered lighting data
struct ShadowTileInfo {
    Math::Matrix4 viewProj;  // Light view-projection for this tile
    Math::Vector4 atlasRect; // x,y = UV offset, z,w = UV scale
    bool valid = false;      // Whether this tile has a valid shadow map
};

// Configuration for shadow atlas
struct ShadowAtlasConfig {
    u32 atlasSize = 4096;      // Atlas dimensions (square)
    u32 minTileSize = 256;     // Minimum tile resolution
    u32 maxTileSize = 2048;    // Maximum tile resolution
    u32 maxTiles = 32;         // Maximum simultaneous shadow tiles
};

// Unified shadow atlas — single large depth image subdivided into tiles.
// Replaces separate ShadowMap, PointLightShadowMap, SpotLightShadowMap allocations
// with a single shared resource. Tiles are allocated per-frame by the ShadowBudget system.
class ENJIN_API ShadowAtlas {
public:
    ShadowAtlas(VulkanContext* context);
    ~ShadowAtlas();

    bool Initialize(const ShadowAtlasConfig& config = ShadowAtlasConfig{});
    void Shutdown();

    // Tile management — called each frame by shadow budget
    void ResetAllocations();  // Clear all tile allocations for this frame
    ShadowTile AllocateTile(u32 requestedSize);  // Returns tile or {allocated=false} if full
    void FreeTile(const ShadowTile& tile);

    // Rendering — per-tile shadow pass
    void BeginTilePass(VkCommandBuffer cmd, const ShadowTile& tile);
    void EndTilePass(VkCommandBuffer cmd);

    // Resize atlas (recreates GPU resources)
    void SetAtlasSize(u32 size);

    // Shader resources
    VkImageView GetDepthView() const { return m_DepthView; }
    VkSampler GetShadowSampler() const { return m_ShadowSampler; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    u32 GetAtlasSize() const { return m_Config.atlasSize; }

    // Stats
    u32 GetAllocatedTileCount() const { return m_AllocatedCount; }
    u32 GetMaxTiles() const { return m_Config.maxTiles; }

private:
    bool CreateDepthImage();
    bool CreateRenderPass();
    bool CreateSampler();
    void DestroyResources();

    // Simple row-based tile packing (sufficient for power-of-two tiles)
    struct TileSlot {
        u32 x, y, size;
        bool used = false;
    };
    std::vector<TileSlot> m_Slots;
    void RebuildSlotGrid();

    VulkanContext* m_Context = nullptr;
    ShadowAtlasConfig m_Config;

    // Single depth image for entire atlas
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthView = VK_NULL_HANDLE;     // Full atlas view for shader sampling
    VkSampler m_ShadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;  // Single framebuffer, viewport-scissored per tile

    u32 m_AllocatedCount = 0;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

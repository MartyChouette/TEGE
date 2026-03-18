#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// GPU-side surfel structure (48 bytes, std430).
// Each surfel represents a surface patch in world space and stores cached
// indirect irradiance for that location.
struct Surfel {
    f32 positionX;          // World position X
    f32 positionY;          // World position Y
    f32 positionZ;          // World position Z
    f32 normalX;            // Surface normal X
    f32 normalY;            // Surface normal Y
    f32 normalZ;            // Surface normal Z
    f32 irradianceR;        // Cached indirect irradiance (red)
    f32 irradianceG;        // Cached indirect irradiance (green)
    f32 irradianceB;        // Cached indirect irradiance (blue)
    f32 radius;             // World-space coverage radius
    u32 lastUpdateFrame;    // Frame index when last updated
    f32 confidence;         // 0-1 temporal stability (higher = more stable)
};

// Surfel radiance cache configuration
struct SurfelRadianceCacheConfig {
    bool enabled = false;
    u32 maxSurfels = 65536;         // Maximum surfel budget
    f32 surfelRadius = 0.5f;        // World-space coverage radius per surfel
    f32 updateFraction = 0.125f;    // Fraction of surfels updated per frame (1/8)
    f32 maxAge = 32.0f;             // Frames before a surfel expires if not refreshed
    bool excludeDirectional = true; // Keep sun/moon out of surfel irradiance
    f32 cameraRadius = 50.0f;       // Only maintain surfels within this radius of camera
    f32 blendWeight = 0.5f;         // Blend weight with screen-space cache (0=screen only, 1=surfel only)
    f32 normalThreshold = 0.7f;     // Dot product threshold for surfel-surface alignment
    u32 placementInterval = 4;      // Re-evaluate surfel placement every N frames
    u32 raysPerSurfel = 2;          // Hemisphere rays per surfel per update (1-4)
};

// World-Space Surfel Radiance Cache
//
// Maintains a pool of surfels (surface elements) distributed on visible geometry
// in world space. Each surfel caches indirect irradiance from ray-traced GI and
// persists across frames and camera movement, unlike the screen-space radiance
// cache which is tied to screen tiles.
//
// Architecture:
//   1. Placement pass (compute): Spawns surfels on G-buffer surfaces within
//      cameraRadius. Uses a simple spatial density check to avoid clustering.
//      Runs every placementInterval frames to amortize cost.
//   2. Update pass (compute): Each frame, updates updateFraction (1/8) of all
//      active surfels by tracing hemisphere rays and accumulating irradiance
//      via exponential moving average. Pseudo-multi-bounce: surfel irradiance
//      feeds back into next frame's evaluation.
//   3. Lookup pass (compute): Per-pixel, finds nearby surfels and interpolates
//      irradiance weighted by distance, normal alignment, and confidence.
//      Blends result with the screen-space cache output.
//
// IMPORTANT: Directional light (sun/moon) is excluded by default.
// Surfels are world-space and persist across camera movement.
// The surfel cache SUPPLEMENTS the screen-space cache, it does not replace it.
//
// Dispatch order: after RT GI + screen-space RadianceCache, before RT compositor.
class ENJIN_API SurfelRadianceCache {
public:
    SurfelRadianceCache(VulkanContext* context);
    ~SurfelRadianceCache();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Run the surfel placement pass — spawns/removes surfels based on G-buffer.
    // Only does work every placementInterval frames.
    void DispatchPlacement(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                           const Math::Vector3& cameraPos, u32 frameCount);

    // Run the surfel update pass — traces rays for a fraction of surfels.
    void DispatchUpdate(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                        const Math::Vector3& cameraPos, u32 frameCount);

    // Run the surfel lookup pass — per-pixel interpolation of surfel irradiance.
    // Blends with screen-space cache output (binding 23) into final output.
    void DispatchLookup(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                        u32 frameCount);

    // Invalidate all surfels (camera cut, major scene change, teleport)
    void InvalidateAll();

    // Output image for the RT compositor (surfel + screen-space blended irradiance)
    VkImageView GetOutputView() const { return m_OutputView; }
    VkImage GetOutputImage() const { return m_OutputImage; }

    // Surfel buffer access (for descriptor set binding)
    VkBuffer GetSurfelBuffer() const { return m_SurfelBuffer; }
    VkBuffer GetSurfelCounterBuffer() const { return m_SurfelCounterBuffer; }
    u32 GetMaxSurfels() const { return m_Config.maxSurfels; }
    u32 GetActiveSurfelCount() const { return m_ActiveSurfelCount; }

    SurfelRadianceCacheConfig& GetConfig() { return m_Config; }
    const SurfelRadianceCacheConfig& GetConfig() const { return m_Config; }

private:
    void CreateResources();
    void DestroyResources();
    void CreateOutputImage();
    void DestroyOutputImage();

    VulkanContext* m_Context = nullptr;
    SurfelRadianceCacheConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    // Placement compute pipeline — spawns/removes surfels from G-buffer
    VkPipeline m_PlacementPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PlacementPipelineLayout = VK_NULL_HANDLE;

    // Update compute pipeline — traces rays and updates surfel irradiance
    VkPipeline m_UpdatePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_UpdatePipelineLayout = VK_NULL_HANDLE;

    // Lookup compute pipeline — per-pixel surfel interpolation
    VkPipeline m_LookupPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_LookupPipelineLayout = VK_NULL_HANDLE;

    // Surfel buffer (Surfel[maxSurfels])
    // Binding 24 in RT descriptor set
    VkBuffer m_SurfelBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_SurfelMemory = VK_NULL_HANDLE;

    // Surfel counter/metadata buffer:
    //   [0] = active surfel count
    //   [1] = next free slot index
    //   [2] = surfels updated this frame
    // Binding 25 in RT descriptor set
    VkBuffer m_SurfelCounterBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_SurfelCounterMemory = VK_NULL_HANDLE;

    // Full-resolution output image (blended surfel + screen-space irradiance)
    // Binding 26 in RT descriptor set
    VkImage m_OutputImage = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputMemory = VK_NULL_HANDLE;
    VkImageView m_OutputView = VK_NULL_HANDLE;

    // CPU-side readback of active surfel count (for stats display)
    u32 m_ActiveSurfelCount = 0;

    bool m_Initialized = false;
    bool m_InvalidateRequested = false;
};

} // namespace Renderer
} // namespace Enjin

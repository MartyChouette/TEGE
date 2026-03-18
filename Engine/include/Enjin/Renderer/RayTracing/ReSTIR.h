#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Per-pixel reservoir for Reservoir-based Spatiotemporal Importance Resampling.
// Stores the selected light sample and its running weight statistics.
// This is the GPU-side struct (16 bytes, std430-friendly).
struct Reservoir {
    u32 selectedLight;     // Index into the NEE light SSBO (binding 16)
    f32 weightSum;         // Running weight sum (W) — accumulated target/proposal ratios
    f32 sampleWeight;      // Weight of selected sample (1/p_hat * W / M) for unbiased estimator
    u32 sampleCount;       // M — number of candidates seen by this reservoir
};

// ReSTIR configuration
struct ReSTIRConfig {
    bool enabled = false;

    // Initial candidate generation
    u32 initialCandidates = 8;   // N — number of random light candidates per pixel (1-32)

    // Importance weighting parameters
    f32 distanceBias = 0.1f;     // Small bias to avoid division by zero in distance falloff

    // Temporal reuse — merge previous frame's reservoirs via motion vector reprojection
    bool temporalReuse = false;
    u32 temporalMaxHistory = 20; // Cap reservoir M to prevent stale samples dominating
    f32 temporalDepthThreshold = 0.1f;  // Relative depth ratio threshold for reprojection validity
    f32 temporalNormalThreshold = 0.9f; // Minimum normal dot product for reprojection validity

    // Spatial reuse — share reservoir data between similar neighboring pixels
    bool spatialReuse = false;
    u32 spatialNeighbors = 5;    // K — number of random neighbors to combine
    f32 spatialRadius = 30.0f;   // Screen-space radius (pixels) for neighbor search
    f32 spatialDepthThreshold = 0.1f;   // Relative depth ratio threshold for neighbor similarity
    f32 spatialNormalThreshold = 0.9f;  // Minimum normal dot product for neighbor similarity
};

// ReSTIR Direct Illumination — importance-weighted light selection via compute shader.
//
// Three-pass pipeline:
//   Pass 1 (Initial): Per-pixel importance-weighted light sampling.
//     For each pixel, samples N candidate lights from the NEE light SSBO, selects one
//     proportional to its estimated contribution, and writes to a reservoir buffer.
//
//   Pass 2 (Temporal): Reprojects previous frame's reservoirs via motion vectors.
//     Merges temporally-accumulated reservoirs with current-frame candidates, capping M
//     to prevent stale samples from dominating. Uses ping-pong buffers.
//
//   Pass 3 (Spatial): Shares reservoir data among spatially similar neighbors.
//     Samples K random neighbors, checks depth/normal similarity, applies Jacobian
//     correction, and merges neighbor reservoirs for improved convergence.
class ENJIN_API ReSTIR {
public:
    ReSTIR(VulkanContext* context);
    ~ReSTIR();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Dispatch all enabled ReSTIR passes: initial → temporal → spatial
    // Must be called after NEE light SSBO is uploaded and before RTShadows/RTGI dispatch.
    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                  u32 totalLightCount);

    // Current frame's reservoir buffer for binding into RT descriptor set (binding 19)
    VkBuffer GetReservoirBuffer() const { return m_ReservoirBuffers[m_CurrentBuffer]; }

    // Previous frame's reservoir buffer for binding into RT descriptor set (binding 20)
    VkBuffer GetPrevReservoirBuffer() const { return m_ReservoirBuffers[1 - m_CurrentBuffer]; }

    u32 GetReservoirCount() const { return m_Width * m_Height; }

    ReSTIRConfig& GetConfig() { return m_Config; }
    const ReSTIRConfig& GetConfig() const { return m_Config; }

private:
    void CreateReservoirBuffers();
    void DestroyReservoirBuffers();
    bool CreateTemporalPipeline(VkDescriptorSetLayout rtDescLayout);
    bool CreateSpatialPipeline(VkDescriptorSetLayout rtDescLayout);

    void DispatchInitial(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                         u32 totalLightCount);
    void DispatchTemporal(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                          u32 totalLightCount);
    void DispatchSpatial(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                         u32 totalLightCount);

    VulkanContext* m_Context = nullptr;
    ReSTIRConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    // Compute pipeline for initial candidate selection
    VkPipeline m_InitialPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_InitialPipelineLayout = VK_NULL_HANDLE;

    // Compute pipeline for temporal reuse
    VkPipeline m_TemporalPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_TemporalPipelineLayout = VK_NULL_HANDLE;

    // Compute pipeline for spatial reuse
    VkPipeline m_SpatialPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_SpatialPipelineLayout = VK_NULL_HANDLE;

    // Ping-pong reservoir storage buffers (binding 19 + 20 in RT descriptor set)
    // Buffer[m_CurrentBuffer] = current frame output (binding 19)
    // Buffer[1-m_CurrentBuffer] = previous frame data (binding 20)
    VkBuffer m_ReservoirBuffers[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDeviceMemory m_ReservoirMemory[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    u32 m_CurrentBuffer = 0;  // Toggles 0/1 each frame for ping-pong

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

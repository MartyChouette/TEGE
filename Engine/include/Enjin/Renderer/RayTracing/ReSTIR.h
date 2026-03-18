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

    // Temporal reuse (Phase 2 — infrastructure only, not yet dispatched)
    bool temporalReuse = false;
    u32 temporalMaxHistory = 20; // Cap reservoir M to prevent stale samples dominating

    // Spatial reuse (Phase 2 — infrastructure only, not yet dispatched)
    bool spatialReuse = false;
    u32 spatialNeighbors = 5;    // K — number of random neighbors to combine
    f32 spatialRadius = 30.0f;   // Screen-space radius (pixels) for neighbor search
};

// ReSTIR Direct Illumination — importance-weighted light selection via compute shader.
//
// Phase 1 (current): Per-pixel importance-weighted light sampling.
//   For each pixel, samples N candidate lights from the NEE light SSBO, selects one
//   proportional to its estimated contribution (emission * solid-angle * visibility-estimate),
//   and writes the selected light index + weight into a reservoir buffer.
//   RTShadows and RTGI can then read the reservoir to focus rays on the most important lights.
//
// Phase 2 (future): Temporal reuse via motion vectors, spatial neighbor sharing.
//   The Reservoir struct and buffer layout are designed for this extension.
class ENJIN_API ReSTIR {
public:
    ReSTIR(VulkanContext* context);
    ~ReSTIR();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Dispatch initial candidate selection (compute shader)
    // Must be called after NEE light SSBO is uploaded and before RTShadows/RTGI dispatch.
    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                  u32 totalLightCount);

    // Reservoir buffer for binding into RT descriptor set
    VkBuffer GetReservoirBuffer() const { return m_ReservoirBuffer; }
    u32 GetReservoirCount() const { return m_Width * m_Height; }

    ReSTIRConfig& GetConfig() { return m_Config; }
    const ReSTIRConfig& GetConfig() const { return m_Config; }

private:
    void CreateReservoirBuffer();
    void DestroyReservoirBuffer();

    VulkanContext* m_Context = nullptr;
    ReSTIRConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    // Compute pipeline for initial candidate selection
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    // Reservoir storage buffer (binding 19 in RT descriptor set)
    // Layout: Reservoir[width * height], indexed by pixel (y * width + x)
    VkBuffer m_ReservoirBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_ReservoirMemory = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

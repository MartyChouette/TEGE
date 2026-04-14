#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// Clustered forward lighting configuration
struct ClusterGridConfig {
    static constexpr u32 TILES_X = 16;
    static constexpr u32 TILES_Y = 9;
    static constexpr u32 TILES_Z = 24;  // Depth slices (exponential distribution)
    static constexpr u32 TOTAL_CLUSTERS = TILES_X * TILES_Y * TILES_Z; // 3456

    // Maximum lights per cluster (capped to avoid excessive memory)
    static constexpr u32 MAX_LIGHTS_PER_CLUSTER = 128;

    // Maximum total light indices (clusters * max lights, but realistically much less)
    static constexpr u32 MAX_LIGHT_INDICES = TOTAL_CLUSTERS * 32; // ~110K indices

    // Maximum lights the system can handle
    static constexpr u32 MAX_LIGHTS = 1024;
};

// Cluster AABB (computed from inverse projection)
struct alignas(16) ClusterAABB {
    Math::Vector4 minPoint;
    Math::Vector4 maxPoint;
};

// Per-cluster light grid entry
struct ClusterLightGrid {
    u32 offset;  // Start index into the global light index list
    u32 count;   // Number of lights in this cluster
};

// Light data for clustered lighting (more compact than LightingUBO format)
struct alignas(16) ClusterLight {
    Math::Vector3 position;
    f32 range;
    Math::Vector3 color;
    f32 intensity;
    Math::Vector3 direction;  // For spot lights
    f32 outerConeAngle;       // 0 = point light, >0 = spot light
};

// Clustered forward lighting system
// Divides the view frustum into 3D clusters, assigns lights to clusters on GPU,
// then fragments look up only their cluster's lights instead of iterating all lights.
class ENJIN_API ClusteredLightingSystem {
public:
    ClusteredLightingSystem(VulkanContext* context);
    ~ClusteredLightingSystem();

    bool Initialize(u32 screenWidth, u32 screenHeight);
    void Shutdown();

    // Recompute cluster bounds when projection changes (screen resize, FOV change)
    void UpdateClusterBounds(const Math::Matrix4& inverseProjection, f32 nearPlane, f32 farPlane);

    // Assign lights to clusters (dispatch compute shader)
    // Call before main render pass, after light data is known.
    void AssignLights(
        VkCommandBuffer commandBuffer,
        const ClusterLight* lights,
        u32 lightCount,
        const Math::Matrix4& viewMatrix
    );

    // Bind the cluster SSBOs for fragment shader access
    // LightGrid at binding 14, LightIndex at binding 15
    VkBuffer GetLightGridBuffer() const;
    VkBuffer GetLightIndexBuffer() const;

    // Descriptor layout for cluster bindings (bindings 14-15)
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

    void Resize(u32 screenWidth, u32 screenHeight);

    // Get cluster grid config for shader uniforms
    static constexpr ClusterGridConfig GetConfig() { return {}; }

private:
    bool CreateBuffers();
    bool CreateComputePipelines();
    bool CreateDescriptorSets();

    VulkanContext* m_Context = nullptr;
    u32 m_ScreenWidth = 0;
    u32 m_ScreenHeight = 0;

    // Compute pipelines
    VkPipeline m_BoundsPipeline = VK_NULL_HANDLE;   // Compute cluster AABBs
    VkPipeline m_AssignPipeline = VK_NULL_HANDLE;    // Assign lights to clusters
    VkPipelineLayout m_BoundsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_AssignPipelineLayout = VK_NULL_HANDLE;

    // Descriptor sets
    VkDescriptorSetLayout m_BoundsDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_AssignDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE; // For fragment shader binding
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_BoundsDescSet = VK_NULL_HANDLE;
    VkDescriptorSet m_AssignDescSet = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    // GPU buffers
    std::unique_ptr<VulkanBuffer> m_ClusterAABBBuffer;    // ClusterAABB[TOTAL_CLUSTERS]
    std::unique_ptr<VulkanBuffer> m_LightGridBuffer;      // ClusterLightGrid[TOTAL_CLUSTERS]
    std::unique_ptr<VulkanBuffer> m_LightIndexBuffer;     // u32[MAX_LIGHT_INDICES]
    std::unique_ptr<VulkanBuffer> m_LightBuffer;          // ClusterLight[MAX_LIGHTS]
    std::unique_ptr<VulkanBuffer> m_GlobalIndexCountBuffer; // Atomic counter for light index allocation
    std::unique_ptr<VulkanBuffer> m_ClusterParamsBuffer;  // View params (near, far, screen size, etc.)

    bool m_Initialized = false;
    bool m_BoundsNeedUpdate = true;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

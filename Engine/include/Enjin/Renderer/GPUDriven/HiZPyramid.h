#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>

namespace Enjin {
namespace Renderer {

class VulkanBuffer;

// Hi-Z (Hierarchical-Z) depth pyramid for GPU occlusion culling.
// A max-reduction mip chain of a depth buffer: each texel is the FARTHEST depth
// in the area it covers, so "the nearest point of this box is behind it" means
// the box is hidden by everything there. Mip 0 is HALF the depth buffer's size
// and is built from it; every later level from the one above.
class ENJIN_API HiZPyramid {
public:
    HiZPyramid(VulkanContext* context);
    ~HiZPyramid();

    // depthWidth/Height: the depth buffer this will be built from.
    bool Initialize(u32 depthWidth, u32 depthHeight);
    void Shutdown();

    // Record the whole build into commandBuffer. The depth image must already be
    // in DEPTH_STENCIL_READ_ONLY_OPTIMAL with its writes made visible to compute,
    // and depthImageView must be a DEPTH-aspect view of the size given to
    // Initialize. Leaves every level in SHADER_READ_ONLY_OPTIMAL.
    void Generate(VkCommandBuffer commandBuffer, VkImageView depthImageView);

    u32 GetDepthWidth() const { return m_DepthWidth; }
    u32 GetDepthHeight() const { return m_DepthHeight; }

    VkImageView GetView() const { return m_FullView; }
    VkSampler GetSampler() const { return m_Sampler; }
    u32 GetMipLevels() const { return m_MipLevels; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }

private:
    bool CreateImage();
    bool CreateSampler();
    bool CreateComputePipeline();
    bool CreateDescriptorResources();

    VulkanContext* m_Context = nullptr;

    u32 m_DepthWidth = 0;               // the source depth buffer
    u32 m_DepthHeight = 0;
    u32 m_Width = 0;                    // mip 0
    u32 m_Height = 0;
    VkImageView m_BoundDepthView = VK_NULL_HANDLE;   // what descriptor set 0 reads
    u32 m_MipLevels = 0;

    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkImageView m_FullView = VK_NULL_HANDLE;         // All mip levels
    std::vector<VkImageView> m_MipViews;              // Per-mip views for compute writes

    VkSampler m_Sampler = VK_NULL_HANDLE;             // nearest; every read is a texelFetch

    // Compute pipeline for downsample
    VkPipeline m_DownsamplePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    // [0] reads the depth buffer and writes mip 0; [i] reads mip i-1, writes mip i.
    std::vector<VkDescriptorSet> m_DescriptorSets;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

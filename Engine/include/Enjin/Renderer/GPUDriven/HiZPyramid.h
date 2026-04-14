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
// Stores a max-reduction mipmap of the depth buffer. Each mip level
// contains the maximum depth of the 4 parent texels, enabling conservative
// occlusion queries: if the closest point of an AABB is behind the stored
// depth at the appropriate mip level, the object is fully occluded.
class ENJIN_API HiZPyramid {
public:
    HiZPyramid(VulkanContext* context);
    ~HiZPyramid();

    bool Initialize(u32 width, u32 height);
    void Shutdown();

    // Generate the Hi-Z pyramid from a depth image view.
    // Records compute dispatches into the given command buffer.
    void Generate(VkCommandBuffer commandBuffer, VkImageView depthImageView);

    // Resize the pyramid (e.g., on window resize)
    bool Resize(u32 width, u32 height);

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

    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_MipLevels = 0;

    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkImageView m_FullView = VK_NULL_HANDLE;         // All mip levels
    std::vector<VkImageView> m_MipViews;              // Per-mip views for compute writes

    VkSampler m_Sampler = VK_NULL_HANDLE;             // MIN reduction sampler for reads

    // Compute pipeline for downsample
    VkPipeline m_DownsamplePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;    // One per mip transition
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

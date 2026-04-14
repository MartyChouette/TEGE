#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <string>

namespace Enjin {
namespace Renderer {

// Attachment description for render passes
struct AttachmentDesc {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
};

// Subpass description
struct SubpassDesc {
    std::vector<u32> colorAttachments;
    i32 depthStencilAttachment = -1; // -1 means no depth
    std::vector<u32> inputAttachments;
    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

// Subpass dependency
struct SubpassDependency {
    u32 srcSubpass = VK_SUBPASS_EXTERNAL;
    u32 dstSubpass = 0;
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
};

// Render pass configuration
struct RenderPassConfig {
    std::string name;
    std::vector<AttachmentDesc> attachments;
    std::vector<SubpassDesc> subpasses;
    std::vector<SubpassDependency> dependencies;
};

// Vulkan render pass wrapper
class ENJIN_API RenderPass {
public:
    RenderPass(VulkanContext* context);
    ~RenderPass();

    bool Create(const RenderPassConfig& config);
    void Destroy();

    void Begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent,
               const std::vector<VkClearValue>& clearValues);
    void End(VkCommandBuffer cmd);
    void NextSubpass(VkCommandBuffer cmd);

    VkRenderPass GetHandle() const { return m_RenderPass; }
    const std::string& GetName() const { return m_Name; }
    u32 GetAttachmentCount() const { return static_cast<u32>(m_AttachmentCount); }
    u32 GetSubpassCount() const { return static_cast<u32>(m_SubpassCount); }

    // Preset configurations
    static RenderPassConfig SimpleColorPass(VkFormat colorFormat);
    static RenderPassConfig ColorDepthPass(VkFormat colorFormat, VkFormat depthFormat);
    static RenderPassConfig DeferredGBufferPass(VkFormat colorFormat, VkFormat normalFormat,
                                                 VkFormat positionFormat, VkFormat depthFormat);

private:
    VulkanContext* m_Context = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    std::string m_Name;
    usize m_AttachmentCount = 0;
    usize m_SubpassCount = 0;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

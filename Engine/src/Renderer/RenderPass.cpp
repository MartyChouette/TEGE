#include "Enjin/Renderer/RenderPass.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

RenderPass::RenderPass(VulkanContext* context)
    : m_Context(context) {
}

RenderPass::~RenderPass() {
    Destroy();
}

bool RenderPass::Create(const RenderPassConfig& config) {
    m_Name = config.name;
    m_AttachmentCount = config.attachments.size();
    m_SubpassCount = config.subpasses.size();

    // Convert attachment descriptions
    std::vector<VkAttachmentDescription> attachments(config.attachments.size());
    for (usize i = 0; i < config.attachments.size(); ++i) {
        const auto& src = config.attachments[i];
        auto& dst = attachments[i];
        dst.format = src.format;
        dst.samples = src.samples;
        dst.loadOp = src.loadOp;
        dst.storeOp = src.storeOp;
        dst.stencilLoadOp = src.stencilLoadOp;
        dst.stencilStoreOp = src.stencilStoreOp;
        dst.initialLayout = src.initialLayout;
        dst.finalLayout = src.finalLayout;
    }

    // Convert subpass descriptions
    std::vector<VkSubpassDescription> subpasses(config.subpasses.size());
    std::vector<std::vector<VkAttachmentReference>> colorRefs(config.subpasses.size());
    std::vector<VkAttachmentReference> depthRefs(config.subpasses.size());
    std::vector<std::vector<VkAttachmentReference>> inputRefs(config.subpasses.size());

    for (usize i = 0; i < config.subpasses.size(); ++i) {
        const auto& src = config.subpasses[i];
        auto& dst = subpasses[i];

        // Color attachments
        for (u32 colorIdx : src.colorAttachments) {
            colorRefs[i].push_back({ colorIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        }

        // Depth attachment
        if (src.depthStencilAttachment >= 0) {
            depthRefs[i] = { static_cast<u32>(src.depthStencilAttachment),
                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        }

        // Input attachments
        for (u32 inputIdx : src.inputAttachments) {
            inputRefs[i].push_back({ inputIdx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        }

        dst.pipelineBindPoint = src.bindPoint;
        dst.colorAttachmentCount = static_cast<u32>(colorRefs[i].size());
        dst.pColorAttachments = colorRefs[i].empty() ? nullptr : colorRefs[i].data();
        dst.pDepthStencilAttachment = src.depthStencilAttachment >= 0 ? &depthRefs[i] : nullptr;
        dst.inputAttachmentCount = static_cast<u32>(inputRefs[i].size());
        dst.pInputAttachments = inputRefs[i].empty() ? nullptr : inputRefs[i].data();
        dst.preserveAttachmentCount = 0;
        dst.pPreserveAttachments = nullptr;
        dst.pResolveAttachments = nullptr;
    }

    // Convert dependencies
    std::vector<VkSubpassDependency> dependencies(config.dependencies.size());
    for (usize i = 0; i < config.dependencies.size(); ++i) {
        const auto& src = config.dependencies[i];
        auto& dst = dependencies[i];
        dst.srcSubpass = src.srcSubpass;
        dst.dstSubpass = src.dstSubpass;
        dst.srcStageMask = src.srcStageMask;
        dst.dstStageMask = src.dstStageMask;
        dst.srcAccessMask = src.srcAccessMask;
        dst.dstAccessMask = src.dstAccessMask;
        dst.dependencyFlags = 0;
    }

    // Create render pass
    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<u32>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = static_cast<u32>(subpasses.size());
    createInfo.pSubpasses = subpasses.data();
    createInfo.dependencyCount = static_cast<u32>(dependencies.size());
    createInfo.pDependencies = dependencies.empty() ? nullptr : dependencies.data();

    VkResult result = vkCreateRenderPass(m_Context->GetDevice(), &createInfo, nullptr, &m_RenderPass);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render pass '%s': %d", m_Name.c_str(), result);
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Created render pass '%s' (%zu attachments, %zu subpasses)",
        m_Name.c_str(), m_AttachmentCount, m_SubpassCount);
    return true;
}

void RenderPass::Destroy() {
    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
        ENJIN_LOG_DEBUG(Renderer, "Destroyed render pass '%s'", m_Name.c_str());
    }
}

void RenderPass::Begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent,
                       const std::vector<VkClearValue>& clearValues) {
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = m_RenderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = extent;
    beginInfo.clearValueCount = static_cast<u32>(clearValues.size());
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::End(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void RenderPass::NextSubpass(VkCommandBuffer cmd) {
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
}

RenderPassConfig RenderPass::SimpleColorPass(VkFormat colorFormat) {
    RenderPassConfig config;
    config.name = "SimpleColor";

    // Color attachment
    AttachmentDesc colorAttachment;
    colorAttachment.format = colorFormat;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    config.attachments.push_back(colorAttachment);

    // Single subpass
    SubpassDesc subpass;
    subpass.colorAttachments = { 0 };
    config.subpasses.push_back(subpass);

    // Dependency
    SubpassDependency dep;
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    config.dependencies.push_back(dep);

    return config;
}

RenderPassConfig RenderPass::ColorDepthPass(VkFormat colorFormat, VkFormat depthFormat) {
    RenderPassConfig config;
    config.name = "ColorDepth";

    // Color attachment
    AttachmentDesc colorAttachment;
    colorAttachment.format = colorFormat;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    config.attachments.push_back(colorAttachment);

    // Depth attachment
    AttachmentDesc depthAttachment;
    depthAttachment.format = depthFormat;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    config.attachments.push_back(depthAttachment);

    // Single subpass
    SubpassDesc subpass;
    subpass.colorAttachments = { 0 };
    subpass.depthStencilAttachment = 1;
    config.subpasses.push_back(subpass);

    // Dependency
    SubpassDependency dep;
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    config.dependencies.push_back(dep);

    return config;
}

RenderPassConfig RenderPass::DeferredGBufferPass(VkFormat colorFormat, VkFormat normalFormat,
                                                  VkFormat positionFormat, VkFormat depthFormat) {
    RenderPassConfig config;
    config.name = "DeferredGBuffer";

    // G-Buffer attachments
    AttachmentDesc albedoAttachment;
    albedoAttachment.format = colorFormat;
    albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    config.attachments.push_back(albedoAttachment);

    AttachmentDesc normalAttachment;
    normalAttachment.format = normalFormat;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    config.attachments.push_back(normalAttachment);

    AttachmentDesc positionAttachment;
    positionAttachment.format = positionFormat;
    positionAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    config.attachments.push_back(positionAttachment);

    AttachmentDesc depthAttachment;
    depthAttachment.format = depthFormat;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    config.attachments.push_back(depthAttachment);

    // G-Buffer subpass
    SubpassDesc gbufferSubpass;
    gbufferSubpass.colorAttachments = { 0, 1, 2 };
    gbufferSubpass.depthStencilAttachment = 3;
    config.subpasses.push_back(gbufferSubpass);

    // Dependency
    SubpassDependency dep;
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    config.dependencies.push_back(dep);

    return config;
}

} // namespace Renderer
} // namespace Enjin

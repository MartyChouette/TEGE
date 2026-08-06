#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Player-side hybrid RT apply. The player renders the scene straight to the
// swapchain (no offscreen target, no normal post-process pass), so the editor's
// post-process overlay can't be reused. This records two fullscreen draws inside
// the open swapchain pass that blend the ray-traced effects onto the scene via
// fixed-function ROP blending (never reading the framebuffer in-shader):
//   - multiply pass: scene *= shadow * AO
//   - additive pass: scene += reflect + GI
// Both are gated by the caller only recording Apply() when hybrid RT is active.
class ENJIN_API RTHybridApply {
public:
    explicit RTHybridApply(VulkanContext* context) : m_Context(context) {}
    ~RTHybridApply() { Shutdown(); }

    // renderPass = the swapchain pass Apply() records into; colorAttachmentCount
    // matches it (2 for the player's MRT color+velocity pass, VUID-07609).
    bool Initialize(VkRenderPass renderPass, u32 colorAttachmentCount);
    void Shutdown();

    // Rebuild pipelines against a recreated swapchain pass (fullscreen/resize).
    void UpdateRenderPass(VkRenderPass renderPass, u32 colorAttachmentCount);

    // Bind the RT effect outputs (SHADER_READ_ONLY). Rewrites the descriptor set
    // only when a view changes.
    void SetInputs(VkImageView shadow, VkImageView ao, VkImageView reflect,
                   VkImageView gi, VkSampler sampler);

    // Record the two blend draws into an already-open render pass.
    void Apply(VkCommandBuffer cmd, u32 width, u32 height,
               f32 shadowStrength, f32 aoStrength, f32 reflectStrength, f32 giStrength);

    bool IsInitialized() const { return m_Initialized; }

private:
    bool CreatePipelines();
    void DestroyPipelines();

    VulkanContext* m_Context = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    u32 m_ColorAttachmentCount = 2;

    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_MultiplyPipeline = VK_NULL_HANDLE;  // scene *= shadow*AO
    VkPipeline m_AdditivePipeline = VK_NULL_HANDLE;   // scene += reflect+GI

    VkImageView m_ShadowView = VK_NULL_HANDLE;
    VkImageView m_AOView = VK_NULL_HANDLE;
    VkImageView m_ReflectView = VK_NULL_HANDLE;
    VkImageView m_GIView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    bool m_DescriptorsWritten = false;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

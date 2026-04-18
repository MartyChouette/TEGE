#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include <memory>

namespace Enjin::Renderer {

class VulkanContext;
class VulkanShaderManager;

struct VulkanPipelineSlot {
    std::unique_ptr<VulkanPipeline> pipeline;
};

class ENJIN_API VulkanPipelineManager : public IGPUPipelineManager {
public:
    VulkanPipelineManager(VulkanContext* context, VulkanShaderManager* shaderMgr);
    ~VulkanPipelineManager() override;

    // Set the render pass used for pipeline creation (must be called before CreateRenderPipeline)
    void SetRenderPass(VkRenderPass renderPass) { m_RenderPass = renderPass; }
    void SetMSAASamples(VkSampleCountFlagBits samples) { m_MSAASamples = samples; }

    GPUPipelineHandle CreateRenderPipeline(const GPURenderPipelineDesc& desc) override;
    void DestroyPipeline(GPUPipelineHandle handle) override;
    bool IsValid(GPUPipelineHandle handle) const override;

    // Access native VulkanPipeline for Vulkan-specific binding
    VulkanPipeline* GetNativePipeline(GPUPipelineHandle handle);

    void Shutdown();

private:
    VulkanContext* m_Context = nullptr;
    VulkanShaderManager* m_ShaderMgr = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkSampleCountFlagBits m_MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    GPUResourcePool<VulkanPipelineSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU

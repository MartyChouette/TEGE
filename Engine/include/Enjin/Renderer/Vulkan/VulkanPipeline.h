#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/RenderStructs.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

// UniformBufferObject and PushConstants are defined in RenderStructs.h (cross-platform)

// Graphics pipeline configuration
struct PipelineConfig {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool depthTest = true;
    bool depthWrite = true;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    // Depth bias for shadow mapping
    bool depthBiasEnable = false;
    f32 depthBiasConstant = 0.0f;
    f32 depthBiasSlope = 0.0f;
    // For depth-only passes (no color attachment)
    bool hasColorAttachment = true;
    // Alpha blending (src alpha, one-minus-src-alpha)
    bool alphaBlend = false;
    // Number of color attachments for MRT (velocity buffer).
    // Default 1 = color only, 2 = color + velocity (RG16F)
    u32 colorAttachmentCount = 1;
    // Custom vertex input state (for instanced pipelines with non-standard vertex layouts)
    // When non-null, replaces the default mesh vertex input
    const VkPipelineVertexInputStateCreateInfo* customVertexInput = nullptr;
};

// Graphics pipeline wrapper
class ENJIN_API VulkanPipeline {
public:
    VulkanPipeline(VulkanContext* context);
    ~VulkanPipeline();

    bool Create(
        const PipelineConfig& config,
        VulkanShader* vertexShader,
        VulkanShader* fragmentShader
    );

    // Create pipeline with external descriptor set layout (for sharing layouts)
    bool CreateWithLayout(
        const PipelineConfig& config,
        VulkanShader* vertexShader,
        VulkanShader* fragmentShader,
        VkDescriptorSetLayout sharedLayout
    );

    void Destroy();

    void Bind(VkCommandBuffer commandBuffer);
    
    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
    void SetBindlessLayout(VkDescriptorSetLayout layout) { m_BindlessSetLayout = layout; }

private:
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool CreatePipeline(const PipelineConfig& config, VulkanShader* vertexShader, VulkanShader* fragmentShader);

    VulkanContext* m_Context = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_BindlessSetLayout = VK_NULL_HANDLE;  // Set 1: bindless textures (optional)
    bool m_OwnsDescriptorSetLayout = true;  // False when using external layout
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

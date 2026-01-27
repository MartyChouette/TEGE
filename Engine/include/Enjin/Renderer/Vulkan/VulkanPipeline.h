#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

// Uniform buffer object for MVP matrices
struct UniformBufferObject {
    alignas(16) Math::Matrix4 model;
    alignas(16) Math::Matrix4 view;
    alignas(16) Math::Matrix4 proj;
};

// Lighting uniform buffer object
struct LightingUBO {
    alignas(16) Math::Vector3 ambientColor;
    alignas(4) f32 ambientIntensity;
    alignas(16) Math::Vector3 cameraPos;
    alignas(4) f32 _pad0;
    alignas(16) Math::Vector3 lightDir;
    alignas(4) f32 lightIntensity;
    alignas(16) Math::Vector3 lightColor;
    alignas(4) f32 _pad1;
};

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
    void Destroy();

    void Bind(VkCommandBuffer commandBuffer);
    
    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

private:
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool CreatePipeline(const PipelineConfig& config, VulkanShader* vertexShader, VulkanShader* fragmentShader);

    VulkanContext* m_Context = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Enjin

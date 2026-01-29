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

// Uniform buffer object for view/projection matrices (shared across all objects)
struct UniformBufferObject {
    alignas(16) Math::Matrix4 view;
    alignas(16) Math::Matrix4 proj;
};

// Push constants for per-object data (model matrix + material)
struct PushConstants {
    alignas(16) Math::Matrix4 model;
    // Material data (must match fragment shader)
    alignas(16) Math::Vector3 baseColor;
    f32 metallic;
    alignas(16) Math::Vector3 emissiveColor;
    f32 roughness;
    f32 emissiveStrength;
    f32 opacity;
    f32 alphaCutoff;
    i32 flags;
};

// Note: LightingUBO is defined in Enjin/ECS/Components/Light.h
// Use ECS::LightingUBO for multi-light support

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

private:
    bool CreateDescriptorSetLayout();
    bool CreatePipelineLayout();
    bool CreatePipeline(const PipelineConfig& config, VulkanShader* vertexShader, VulkanShader* fragmentShader);

    VulkanContext* m_Context = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    bool m_OwnsDescriptorSetLayout = true;  // False when using external layout
};

} // namespace Renderer
} // namespace Enjin

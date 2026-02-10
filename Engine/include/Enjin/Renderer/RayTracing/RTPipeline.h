#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// SBT (Shader Binding Table) region offsets
struct SBTRegions {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

// Ray tracing pipeline wrapper — manages pipeline creation, shader groups, and SBT
class ENJIN_API RTPipeline {
public:
    RTPipeline(VulkanContext* context);
    ~RTPipeline();

    // Shader stage definition for pipeline creation
    struct ShaderStage {
        VkShaderStageFlagBits stage;
        const u32* spirvData;
        usize spirvSize;  // In bytes
        const char* entryPoint = "main";
    };

    // Shader group definition
    struct ShaderGroup {
        VkRayTracingShaderGroupTypeKHR type;
        u32 generalShader = VK_SHADER_UNUSED_KHR;
        u32 closestHitShader = VK_SHADER_UNUSED_KHR;
        u32 anyHitShader = VK_SHADER_UNUSED_KHR;
        u32 intersectionShader = VK_SHADER_UNUSED_KHR;
    };

    // Create the RT pipeline
    bool Create(const std::vector<ShaderStage>& stages,
                const std::vector<ShaderGroup>& groups,
                VkDescriptorSetLayout descriptorSetLayout,
                VkPipelineLayout pipelineLayout,
                u32 maxRecursionDepth = 1);

    void Destroy();

    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
    const SBTRegions& GetSBTRegions() const { return m_SBTRegions; }

    bool IsValid() const { return m_Pipeline != VK_NULL_HANDLE; }

private:
    bool BuildSBT(const std::vector<ShaderGroup>& groups);

    VulkanContext* m_Context = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;  // Not owned — caller manages layout
    bool m_OwnsPipelineLayout = false;

    // SBT buffer and regions
    VkBuffer m_SBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_SBTMemory = VK_NULL_HANDLE;
    SBTRegions m_SBTRegions;

    // Shader modules (destroyed after pipeline creation)
    std::vector<VkShaderModule> m_ShaderModules;
};

} // namespace Renderer
} // namespace Enjin

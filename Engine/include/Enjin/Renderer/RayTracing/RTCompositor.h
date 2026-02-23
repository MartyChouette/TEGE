#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// RT compositor configuration
struct RTCompositeConfig {
    f32 shadowStrength = 1.0f;
    f32 reflectionStrength = 0.5f;
    f32 aoStrength = 1.0f;
    f32 giStrength = 0.5f;
    f32 translucencyStrength = 0.8f;
    f32 causticsStrength = 0.5f;
};

// Compute shader that composites RT layers into the scene HDR image
class ENJIN_API RTCompositor {
public:
    RTCompositor(VulkanContext* context);
    ~RTCompositor();

    bool Initialize(VkDescriptorSetLayout rtDescLayout);
    void Shutdown();

    // Dispatch composite pass
    // enableFlags: bit 0=shadow, 1=reflect, 2=ao, 3=gi
    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                  u32 width, u32 height, u32 enableFlags);

    RTCompositeConfig& GetConfig() { return m_Config; }
    const RTCompositeConfig& GetConfig() const { return m_Config; }

private:
    VulkanContext* m_Context = nullptr;
    RTCompositeConfig m_Config;

    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

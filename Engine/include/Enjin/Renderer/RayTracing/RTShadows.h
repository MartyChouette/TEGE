#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class RTPipeline;

// RT shadow configuration
struct RTShadowConfig {
    bool enabled = false;
    f32 maxDistance = 100.0f;  // Maximum shadow ray distance
    f32 radius = 0.01f;        // Soft shadow penumbra radius
};

// RT Shadows — traces 1 SPP shadow rays toward lights
class ENJIN_API RTShadows {
public:
    RTShadows(VulkanContext* context);
    ~RTShadows();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Dispatch shadow ray tracing
    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                  const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                  f32 lightIntensity, u32 frameCount);

    // Output image
    VkImageView GetOutputView() const { return m_OutputView; }
    VkImage GetOutputImage() const { return m_OutputImage; }

    RTShadowConfig& GetConfig() { return m_Config; }
    const RTShadowConfig& GetConfig() const { return m_Config; }

private:
    void CreateOutputImage();
    void DestroyOutputImage();

    VulkanContext* m_Context = nullptr;
    RTShadowConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    std::unique_ptr<RTPipeline> m_Pipeline;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    // Output image (R16F)
    VkImage m_OutputImage = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputMemory = VK_NULL_HANDLE;
    VkImageView m_OutputView = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

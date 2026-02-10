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

// RT global illumination configuration
struct RTGIConfig {
    bool enabled = false;  // Off by default (expensive)
    f32 maxDistance = 50.0f;
    f32 intensity = 1.0f;
    u32 bounces = 1;
};

// RT Global Illumination — multi-bounce diffuse GI
class ENJIN_API RTGlobalIllumination {
public:
    RTGlobalIllumination(VulkanContext* context);
    ~RTGlobalIllumination();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                  const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                  const Math::Vector3& cameraPos, u32 frameCount);

    VkImageView GetOutputView() const { return m_OutputView; }
    VkImage GetOutputImage() const { return m_OutputImage; }

    RTGIConfig& GetConfig() { return m_Config; }
    const RTGIConfig& GetConfig() const { return m_Config; }

private:
    void CreateOutputImage();
    void DestroyOutputImage();

    VulkanContext* m_Context = nullptr;
    RTGIConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    std::unique_ptr<RTPipeline> m_Pipeline;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    VkImage m_OutputImage = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputMemory = VK_NULL_HANDLE;
    VkImageView m_OutputView = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

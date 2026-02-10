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

// RT ambient occlusion configuration
struct RTAOConfig {
    bool enabled = false;
    f32 radius = 2.0f;   // AO sampling radius (world units)
    f32 power = 1.5f;     // AO power curve
};

// RT Ambient Occlusion — short-range cosine-weighted hemisphere rays
class ENJIN_API RTAmbientOcclusion {
public:
    RTAmbientOcclusion(VulkanContext* context);
    ~RTAmbientOcclusion();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                  const Math::Matrix4& invViewProj, const Math::Vector3& cameraPos,
                  u32 frameCount);

    VkImageView GetOutputView() const { return m_OutputView; }
    VkImage GetOutputImage() const { return m_OutputImage; }

    RTAOConfig& GetConfig() { return m_Config; }
    const RTAOConfig& GetConfig() const { return m_Config; }

private:
    void CreateOutputImage();
    void DestroyOutputImage();

    VulkanContext* m_Context = nullptr;
    RTAOConfig m_Config;
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

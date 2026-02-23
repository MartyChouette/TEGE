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

// RT caustics configuration
struct RTCausticsConfig {
    bool enabled = false;
    f32 intensity = 1.0f;           // Caustic brightness multiplier
    f32 maxDistance = 30.0f;         // Maximum photon trace distance
    u32 photonsPerLight = 1;        // Photons per light per pixel (more = less noise)
};

// RT Caustics — traces refracted light paths to produce caustic patterns
// Uses photon-tracing approach: cast rays from lights through refractive surfaces
// onto receiving surfaces, accumulating light concentration patterns.
class ENJIN_API RTCaustics {
public:
    RTCaustics(VulkanContext* context);
    ~RTCaustics();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                  const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                  u32 frameCount);

    VkImageView GetOutputView() const { return m_OutputView; }
    VkImage GetOutputImage() const { return m_OutputImage; }

    RTCausticsConfig& GetConfig() { return m_Config; }
    const RTCausticsConfig& GetConfig() const { return m_Config; }

private:
    void CreateOutputImage();
    void DestroyOutputImage();

    VulkanContext* m_Context = nullptr;
    RTCausticsConfig m_Config;
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

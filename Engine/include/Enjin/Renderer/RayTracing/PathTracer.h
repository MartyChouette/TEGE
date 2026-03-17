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

// Path tracer configuration
struct PathTracerConfig {
    u32 maxBounces = 4;
    u32 targetSPP = 1024;          // Target samples per pixel
    f32 fireflyClampValue = 10.0f; // Max radiance per sample (clamping threshold)
    bool enableNEE = true;         // Next Event Estimation (direct light sampling)
    bool enableMIS = true;         // Multiple Importance Sampling
    f32 russianRouletteMinBounce = 3; // Start Russian Roulette after this many bounces
    f32 russianRouletteMinProb = 0.05f; // Minimum survival probability
};

// Progressive path tracer with accumulation buffer
class ENJIN_API PathTracer {
public:
    PathTracer(VulkanContext* context);
    ~PathTracer();

    bool Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Dispatch one sample per pixel
    void Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                  const Math::Matrix4& invViewProj, const Math::Vector3& cameraPos,
                  const Math::Vector3& lightDir, u32 frameCount);

    // Reset accumulation (call on camera move, scene change)
    void ResetAccumulation();

    // Get accumulated sample count
    u32 GetAccumulatedSamples() const { return m_AccumulatedSamples; }
    bool IsConverged() const { return m_AccumulatedSamples >= m_Config.targetSPP; }

    VkImageView GetOutputView() const { return m_AccumulationView; }
    VkImage GetOutputImage() const { return m_AccumulationImage; }

    PathTracerConfig& GetConfig() { return m_Config; }
    const PathTracerConfig& GetConfig() const { return m_Config; }

private:
    void CreateAccumulationImage();
    void DestroyAccumulationImage();

    VulkanContext* m_Context = nullptr;
    PathTracerConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    std::unique_ptr<RTPipeline> m_Pipeline;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    // Accumulation buffer (RGBA16F)
    VkImage m_AccumulationImage = VK_NULL_HANDLE;
    VkDeviceMemory m_AccumulationMemory = VK_NULL_HANDLE;
    VkImageView m_AccumulationView = VK_NULL_HANDLE;

    u32 m_AccumulatedSamples = 0;
    Math::Matrix4 m_LastViewProj;  // For detecting camera movement

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

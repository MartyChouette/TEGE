#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

struct AdaptiveRayBudgetConfig {
    bool enabled = false;
    u32 minRaysPerPixel = 1;
    u32 maxRaysPerPixel = 4;
    f32 varianceThreshold = 0.1f;
    f32 varianceScale = 4.0f;
    bool edgeBoost = true;
    bool disocclusionBoost = true;
};

// Per-pixel adaptive ray budget based on temporal variance.
// High-variance regions get more rays, stable regions fewer.
class ENJIN_API AdaptiveRayBudget {
public:
    AdaptiveRayBudget(VulkanContext* context);
    ~AdaptiveRayBudget();

    bool Initialize(u32 width, u32 height);
    void Shutdown();
    void Resize(u32 width, u32 height);

    void DispatchVarianceEstimate(VkCommandBuffer cmd, VkImageView currentShadowView,
                                  VkImageView prevShadowView);
    void DispatchBudgetAllocation(VkCommandBuffer cmd, VkImageView depthView,
                                  VkImageView normalView, VkImageView staleMaskView);

    VkImageView GetVarianceView() const { return m_VarianceView; }
    VkImageView GetBudgetView() const { return m_BudgetView; }

    AdaptiveRayBudgetConfig& GetConfig() { return m_Config; }
    const AdaptiveRayBudgetConfig& GetConfig() const { return m_Config; }

private:
    void CreateImages();
    void DestroyImages();

    VulkanContext* m_Context = nullptr;
    AdaptiveRayBudgetConfig m_Config;
    u32 m_Width = 0, m_Height = 0;

    VkImage m_VarianceImage = VK_NULL_HANDLE;
    VkDeviceMemory m_VarianceMemory = VK_NULL_HANDLE;
    VkImageView m_VarianceView = VK_NULL_HANDLE;

    VkImage m_BudgetImage = VK_NULL_HANDLE;
    VkDeviceMemory m_BudgetMemory = VK_NULL_HANDLE;
    VkImageView m_BudgetView = VK_NULL_HANDLE;

    VkSampler m_Sampler = VK_NULL_HANDLE;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU

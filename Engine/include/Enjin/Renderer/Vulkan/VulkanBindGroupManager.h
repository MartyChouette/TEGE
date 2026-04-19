#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/GPUBindGroup.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Enjin::Renderer {

class VulkanContext;
class VulkanBufferManager;
class VulkanTextureManager;

struct VulkanBindGroupLayoutSlot {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    std::vector<GPUBindGroupLayoutEntry> entries;  // Keep for validation
};

struct VulkanBindGroupSlot {
    VkDescriptorSet set = VK_NULL_HANDLE;
    GPUBindGroupLayoutHandle layoutHandle;
};

// Vulkan implementation of IGPUBindGroupManager.
// Manages VkDescriptorSetLayout creation and VkDescriptorSet allocation from
// an internal descriptor pool. Pool grows automatically when exhausted.
class ENJIN_API VulkanBindGroupManager : public IGPUBindGroupManager {
public:
    VulkanBindGroupManager(VulkanContext* context,
                           VulkanBufferManager* bufferMgr,
                           VulkanTextureManager* textureMgr);
    ~VulkanBindGroupManager() override;

    GPUBindGroupLayoutHandle CreateBindGroupLayout(const GPUBindGroupLayoutDesc& desc) override;
    GPUBindGroupHandle CreateBindGroup(const GPUBindGroupDesc& desc) override;
    void DestroyBindGroup(GPUBindGroupHandle handle) override;
    void DestroyBindGroupLayout(GPUBindGroupLayoutHandle handle) override;
    bool IsValid(GPUBindGroupHandle handle) const override;
    bool IsValidLayout(GPUBindGroupLayoutHandle handle) const override;

    // Native access for Vulkan-specific code
    VkDescriptorSetLayout GetNativeLayout(GPUBindGroupLayoutHandle handle);
    VkDescriptorSet GetNativeSet(GPUBindGroupHandle handle);

    void Shutdown();

private:
    VkDescriptorType TranslateBindingType(GPUBindingType type);
    VkShaderStageFlags TranslateVisibility(GPUShaderStage stage);
    void EnsurePoolCapacity();

    VulkanContext* m_Context = nullptr;
    VulkanBufferManager* m_BufferMgr = nullptr;
    VulkanTextureManager* m_TextureMgr = nullptr;

    GPUResourcePool<VulkanBindGroupLayoutSlot> m_LayoutPool;
    GPUResourcePool<VulkanBindGroupSlot> m_SetPool;

    // Descriptor pool — grows by creating new pools when full
    std::vector<VkDescriptorPool> m_DescriptorPools;
    u32 m_SetsAllocated = 0;
    static constexpr u32 SETS_PER_POOL = 256;
};

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU

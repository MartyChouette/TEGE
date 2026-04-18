#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/GPUBuffer.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include <memory>

namespace Enjin::Renderer {

class VulkanContext;

// Native resource stored in the slotmap
struct VulkanBufferSlot {
    std::unique_ptr<VulkanBuffer> buffer;
    u64 size = 0;
};

// Vulkan implementation of IGPUBufferManager.
// Wraps VulkanBuffer instances, maps GPUBufferHandle to native VkBuffer.
class ENJIN_API VulkanBufferManager : public IGPUBufferManager {
public:
    explicit VulkanBufferManager(VulkanContext* context);
    ~VulkanBufferManager() override;

    GPUBufferHandle CreateBuffer(const GPUBufferDesc& desc) override;
    GPUBufferHandle CreateBufferWithData(const GPUBufferDesc& desc, const void* data) override;
    void DestroyBuffer(GPUBufferHandle handle) override;
    void UploadData(GPUBufferHandle handle, const void* data, u64 size, u64 offset) override;
    void* MapBuffer(GPUBufferHandle handle) override;
    void UnmapBuffer(GPUBufferHandle handle) override;
    u64 GetBufferSize(GPUBufferHandle handle) const override;
    bool IsValid(GPUBufferHandle handle) const override;

    // Access the native VulkanBuffer for Vulkan-specific operations (descriptor writes, etc.)
    VulkanBuffer* GetNativeBuffer(GPUBufferHandle handle);

    void Shutdown();

private:
    BufferUsage TranslateUsage(GPUBufferUsage usage);

    VulkanContext* m_Context = nullptr;
    GPUResourcePool<VulkanBufferSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU

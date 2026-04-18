#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/GPUTexture.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include "Enjin/Renderer/Texture.h"
#include <memory>

namespace Enjin::Renderer {

class VulkanContext;

struct VulkanTextureSlot {
    std::unique_ptr<Texture> texture;
    u32 width = 0;
    u32 height = 0;
};

// Vulkan implementation of IGPUTextureManager.
// Wraps Texture (VulkanImage + VulkanSampler) instances.
class ENJIN_API VulkanTextureManager : public IGPUTextureManager {
public:
    explicit VulkanTextureManager(VulkanContext* context);
    ~VulkanTextureManager() override;

    GPUTextureHandle CreateTexture(const GPUTextureDesc& desc) override;
    GPUTextureHandle CreateTextureWithData(const GPUTextureDesc& desc, const void* pixelData) override;
    GPUTextureHandle CreateSolidColor(u8 r, u8 g, u8 b, u8 a) override;
    GPUTextureHandle LoadFromMemory(const void* fileData, u64 fileSize, const char* label) override;
    void DestroyTexture(GPUTextureHandle handle) override;
    void UploadData(GPUTextureHandle handle, const void* data, u32 width, u32 height) override;
    u32 GetWidth(GPUTextureHandle handle) const override;
    u32 GetHeight(GPUTextureHandle handle) const override;
    bool IsValid(GPUTextureHandle handle) const override;

    // Access native Texture for Vulkan-specific descriptor writes
    Texture* GetNativeTexture(GPUTextureHandle handle);

    void Shutdown();

private:
    VulkanContext* m_Context = nullptr;
    GPUResourcePool<VulkanTextureSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU

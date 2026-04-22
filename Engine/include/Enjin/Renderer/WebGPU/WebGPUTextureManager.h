#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPUTexture.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"

namespace Enjin::Renderer {

struct WebGPUTextureSlot {
    WebGPUTextureHandle nativeHandle;
    u32 width = 0;
    u32 height = 0;
};

// WebGPU implementation of IGPUTextureManager.
class ENJIN_API WebGPUTextureManager : public IGPUTextureManager {
public:
    explicit WebGPUTextureManager(WebGPURenderer* renderer);
    ~WebGPUTextureManager() override;

    GPUTextureHandle CreateTexture(const GPUTextureDesc& desc) override;
    GPUTextureHandle CreateTextureWithData(const GPUTextureDesc& desc, const void* pixelData) override;
    GPUTextureHandle CreateSolidColor(u8 r, u8 g, u8 b, u8 a) override;
    GPUTextureHandle LoadFromMemory(const void* fileData, u64 fileSize, const char* label) override;
    void DestroyTexture(GPUTextureHandle handle) override;
    void UploadData(GPUTextureHandle handle, const void* data, u32 width, u32 height) override;
    u32 GetWidth(GPUTextureHandle handle) const override;
    u32 GetHeight(GPUTextureHandle handle) const override;
    bool IsValid(GPUTextureHandle handle) const override;

    // Access native WebGPU texture handles
    const WebGPUTextureHandle* GetNativeTexture(GPUTextureHandle handle) const;

    // Register a pre-created native texture (e.g. cubemaps created directly via WebGPURenderer)
    GPUTextureHandle RegisterNativeTexture(const WebGPUTextureHandle& native);

    void Shutdown();

private:
    WebGPURenderer* m_Renderer = nullptr;
    GPUResourcePool<WebGPUTextureSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB

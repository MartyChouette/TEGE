#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPUBuffer.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"

namespace Enjin::Renderer {

struct WebGPUBufferSlot {
    WebGPUBufferHandle nativeHandle;
};

// WebGPU implementation of IGPUBufferManager.
// Wraps WGPUBuffer via WebGPURenderer's buffer API.
class ENJIN_API WebGPUBufferManager : public IGPUBufferManager {
public:
    explicit WebGPUBufferManager(WebGPURenderer* renderer);
    ~WebGPUBufferManager() override;

    GPUBufferHandle CreateBuffer(const GPUBufferDesc& desc) override;
    GPUBufferHandle CreateBufferWithData(const GPUBufferDesc& desc, const void* data) override;
    void DestroyBuffer(GPUBufferHandle handle) override;
    void UploadData(GPUBufferHandle handle, const void* data, u64 size, u64 offset) override;
    void* MapBuffer(GPUBufferHandle handle) override;
    void UnmapBuffer(GPUBufferHandle handle) override;
    u64 GetBufferSize(GPUBufferHandle handle) const override;
    bool IsValid(GPUBufferHandle handle) const override;

    // Access native WebGPU buffer for direct WebGPU operations
    WGPUBuffer GetNativeBuffer(GPUBufferHandle handle);

    void Shutdown();

private:
    WGPUBufferUsage TranslateUsage(GPUBufferUsage usage);

    WebGPURenderer* m_Renderer = nullptr;
    GPUResourcePool<WebGPUBufferSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB

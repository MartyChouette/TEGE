#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPUBindGroup.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include <webgpu/webgpu.h>

namespace Enjin::Renderer {

class WebGPURenderer;
class WebGPUBufferManager;
class WebGPUTextureManager;

struct WebGPUBindGroupLayoutSlot {
    WGPUBindGroupLayout layout = nullptr;
};

struct WebGPUBindGroupSlot {
    WGPUBindGroup group = nullptr;
};

class ENJIN_API WebGPUBindGroupManager : public IGPUBindGroupManager {
public:
    WebGPUBindGroupManager(WebGPURenderer* renderer,
                           WebGPUBufferManager* bufferMgr,
                           WebGPUTextureManager* textureMgr);
    ~WebGPUBindGroupManager() override;

    GPUBindGroupLayoutHandle CreateBindGroupLayout(const GPUBindGroupLayoutDesc& desc) override;
    GPUBindGroupHandle CreateBindGroup(const GPUBindGroupDesc& desc) override;
    void DestroyBindGroup(GPUBindGroupHandle handle) override;
    void DestroyBindGroupLayout(GPUBindGroupLayoutHandle handle) override;
    bool IsValid(GPUBindGroupHandle handle) const override;
    bool IsValidLayout(GPUBindGroupLayoutHandle handle) const override;

    WGPUBindGroupLayout GetNativeLayout(GPUBindGroupLayoutHandle handle);
    WGPUBindGroup GetNativeGroup(GPUBindGroupHandle handle);

    void Shutdown();

private:
    WebGPURenderer* m_Renderer = nullptr;
    WebGPUBufferManager* m_BufferMgr = nullptr;
    WebGPUTextureManager* m_TextureMgr = nullptr;

    GPUResourcePool<WebGPUBindGroupLayoutSlot> m_LayoutPool;
    GPUResourcePool<WebGPUBindGroupSlot> m_GroupPool;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB

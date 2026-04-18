#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPUShader.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include <webgpu/webgpu.h>

namespace Enjin::Renderer {

class WebGPURenderer;

struct WebGPUShaderSlot {
    WGPUShaderModule module = nullptr;
    GPUShaderStage stage = GPUShaderStage::Vertex;
};

class ENJIN_API WebGPUShaderManager : public IGPUShaderManager {
public:
    explicit WebGPUShaderManager(WebGPURenderer* renderer);
    ~WebGPUShaderManager() override;

    // data is WGSL source text, size is string length (excluding null terminator)
    GPUShaderHandle LoadShader(const void* data, u64 size, GPUShaderStage stage,
                               const char* label) override;
    GPUShaderHandle LoadShaderFromFile(const std::string& path, GPUShaderStage stage,
                                       const char* label) override;
    void DestroyShader(GPUShaderHandle handle) override;
    bool IsValid(GPUShaderHandle handle) const override;

    // Access native WGPUShaderModule
    WGPUShaderModule GetNativeShader(GPUShaderHandle handle);

    void Shutdown();

private:
    WebGPURenderer* m_Renderer = nullptr;
    GPUResourcePool<WebGPUShaderSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB

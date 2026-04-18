#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/GPUShader.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include <memory>

namespace Enjin::Renderer {

class VulkanContext;

struct VulkanShaderSlot {
    std::unique_ptr<VulkanShader> shader;
    GPUShaderStage stage = GPUShaderStage::Vertex;
};

class ENJIN_API VulkanShaderManager : public IGPUShaderManager {
public:
    explicit VulkanShaderManager(VulkanContext* context);
    ~VulkanShaderManager() override;

    GPUShaderHandle LoadShader(const void* data, u64 size, GPUShaderStage stage,
                               const char* label) override;
    GPUShaderHandle LoadShaderFromFile(const std::string& path, GPUShaderStage stage,
                                       const char* label) override;
    void DestroyShader(GPUShaderHandle handle) override;
    bool IsValid(GPUShaderHandle handle) const override;

    // Access native VulkanShader for pipeline creation
    VulkanShader* GetNativeShader(GPUShaderHandle handle);

    void Shutdown();

private:
    VulkanContext* m_Context = nullptr;
    GPUResourcePool<VulkanShaderSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU

#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Vulkan/VulkanShaderManager.h"
#include "Enjin/Logging/Log.h"

namespace Enjin::Renderer {

VulkanShaderManager::VulkanShaderManager(VulkanContext* context)
    : m_Context(context) {}

VulkanShaderManager::~VulkanShaderManager() {
    Shutdown();
}

void VulkanShaderManager::Shutdown() {
    m_Pool.ForEach([](VulkanShaderSlot& slot) {
        if (slot.shader) slot.shader->Destroy();
    });
    m_Pool.Clear();
}

GPUShaderHandle VulkanShaderManager::LoadShader(const void* data, u64 size, GPUShaderStage stage,
                                                  const char* label) {
    auto shader = std::make_unique<VulkanShader>(m_Context);
    if (!shader->LoadFromSPIRV(static_cast<const u8*>(data), static_cast<usize>(size))) {
        ENJIN_LOG_ERROR(Core, "VulkanShaderManager: Failed to load SPIR-V shader%s%s",
                        label ? ": " : "", label ? label : "");
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->shader = std::move(shader);
    slot->stage = stage;
    return GPUShaderHandle{handle};
}

GPUShaderHandle VulkanShaderManager::LoadShaderFromFile(const std::string& path, GPUShaderStage stage,
                                                          const char* label) {
    auto shader = std::make_unique<VulkanShader>(m_Context);
    if (!shader->LoadFromFile(path)) {
        ENJIN_LOG_ERROR(Core, "VulkanShaderManager: Failed to load shader file: %s", path.c_str());
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->shader = std::move(shader);
    slot->stage = stage;
    return GPUShaderHandle{handle};
}

void VulkanShaderManager::DestroyShader(GPUShaderHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    if (slot->shader) slot->shader->Destroy();
    m_Pool.Free(handle.id);
}

bool VulkanShaderManager::IsValid(GPUShaderHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

VulkanShader* VulkanShaderManager::GetNativeShader(GPUShaderHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->shader.get() : nullptr;
}

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU

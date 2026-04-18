#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUShaderManager.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>

namespace Enjin::Renderer {

WebGPUShaderManager::WebGPUShaderManager(WebGPURenderer* renderer)
    : m_Renderer(renderer) {}

WebGPUShaderManager::~WebGPUShaderManager() {
    Shutdown();
}

void WebGPUShaderManager::Shutdown() {
    m_Pool.ForEach([](WebGPUShaderSlot& slot) {
        if (slot.module) { wgpuShaderModuleRelease(slot.module); slot.module = nullptr; }
    });
    m_Pool.Clear();
}

GPUShaderHandle WebGPUShaderManager::LoadShader(const void* data, u64 size, GPUShaderStage stage,
                                                  const char* label) {
    auto* compiler = m_Renderer->GetShaderCompiler();
    if (!compiler) return {};

    // Data is WGSL source text — ensure null-terminated
    std::string source(static_cast<const char*>(data), static_cast<size_t>(size));
    WGPUShaderModule module = compiler->CompileWGSL(source.c_str(), label ? label : "shader");
    if (!module) {
        ENJIN_LOG_ERROR(Core, "WebGPUShaderManager: WGSL compilation failed%s%s: %s",
                        label ? ": " : "", label ? label : "",
                        compiler->GetLastError().c_str());
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->module = module;
    slot->stage = stage;
    return GPUShaderHandle{handle};
}

GPUShaderHandle WebGPUShaderManager::LoadShaderFromFile(const std::string& path, GPUShaderStage stage,
                                                          const char* label) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        ENJIN_LOG_ERROR(Core, "WebGPUShaderManager: Cannot open shader file: %s", path.c_str());
        return {};
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    return LoadShader(source.data(), source.size(), stage, label ? label : path.c_str());
}

void WebGPUShaderManager::DestroyShader(GPUShaderHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    if (slot->module) { wgpuShaderModuleRelease(slot->module); slot->module = nullptr; }
    m_Pool.Free(handle.id);
}

bool WebGPUShaderManager::IsValid(GPUShaderHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

WGPUShaderModule WebGPUShaderManager::GetNativeShader(GPUShaderHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->module : nullptr;
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB

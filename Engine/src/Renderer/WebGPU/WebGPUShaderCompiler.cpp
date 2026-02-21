#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Logging/Log.h"
#include <fstream>
#include <sstream>

namespace Enjin {
namespace Renderer {

WebGPUShaderCompiler::WebGPUShaderCompiler(WGPUDevice device) : m_Device(device) {}

WGPUShaderModule WebGPUShaderCompiler::CompileWGSL(const std::string& source, const char* label) {
    m_LastError.clear();

    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = source.c_str();

    WGPUShaderModuleDescriptor desc = {};
    desc.nextInChain = &wgslDesc.chain;
    desc.label = label;

    WGPUShaderModule module = wgpuDeviceCreateShaderModule(m_Device, &desc);
    if (!module) {
        m_LastError = "Failed to create shader module";
        if (label) {
            m_LastError += " '";
            m_LastError += label;
            m_LastError += "'";
        }
        ENJIN_LOG_ERROR(Core, "WebGPU shader compile failed: %s", m_LastError.c_str());
    }
    return module;
}

WGPUShaderModule WebGPUShaderCompiler::CompileFile(const std::string& path, const char* label) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_LastError = "Cannot open shader file: " + path;
        ENJIN_LOG_ERROR(Core, "%s", m_LastError.c_str());
        return nullptr;
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    const char* effectiveLabel = label;
    if (!effectiveLabel) {
        effectiveLabel = path.c_str();
    }

    return CompileWGSL(ss.str(), effectiveLabel);
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
